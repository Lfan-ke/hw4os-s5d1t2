/* S4 · 异步运行时实现（参考解）：poll 状态机 + 最小 executor。 */
#include "async.h"
#include "kernel.h"

extern volatile uint64_t g_ticks; /* 由 trap_handler 累加（common/trap.S 入口） */

/* ===== 就绪队列 / timer reactor 原语（给定，勿改）===== */

void exec_init(struct Executor *ex) {
    ex->rhead = ex->rtail = ex->rlen = 0;
    ex->tlen = 0;
}

static void ready_push(struct Executor *ex, struct Task *t) {
    ex->ready[ex->rtail] = t;
    ex->rtail = (ex->rtail + 1) % MAX_TASKS;
    ex->rlen++;
}

static struct Task *ready_pop(struct Executor *ex) {
    struct Task *t = ex->ready[ex->rhead];
    ex->rhead = (ex->rhead + 1) % MAX_TASKS;
    ex->rlen--;
    return t;
}

void exec_spawn(struct Executor *ex, struct Task *t) {
    ready_push(ex, t); /* 新任务进就绪队列等首次 poll */
}

/* 把一个返回 Pending(WAKE_TIMER) 的任务挂到 reactor。 */
static void timer_register(struct Executor *ex, struct Task *t) {
    ex->timer[ex->tlen++] = t;
}

/* waker：扫 reactor，凡 g_ticks 已达 wake_tick 者，重新入就绪队列。 */
static void reactor_wake_due(struct Executor *ex) {
    int i = 0;
    while (i < ex->tlen) {
        struct Task *t = ex->timer[i];
        if (g_ticks >= t->wake_tick) {
            ready_push(ex, t);                  /* 唤醒：回就绪队列 */
            ex->timer[i] = ex->timer[--ex->tlen]; /* O(1) 移除 */
        } else {
            i++;
        }
    }
}

/* ===== 学生实现区 1：两个 future 的 poll（状态机推进）===== */

/* 计数让出 future：让出 n 次，每次推进一个状态号并打一个标签，
 * 第 n+1 次 poll 返回 Ready，产出 magic。
 * 关键：进度记在 self->state（无栈协程"换状态号"），不是函数栈上的局部。 */
PollResult count_poll(struct Task *self) {
    if (self->state < self->n) {
        self->state++;                 /* 推进状态机 */
        if (self->label) trace_emit(self->label); /* 标签：体现交错 */
        self->want = WAKE_NOW;         /* 让出后立即可再调度 */
        return POLL_PENDING;
    }
    self->out = self->magic;           /* 完成：写产出值 */
    return POLL_READY;
}

/* 延时 future：等到 g_ticks 达 wake_tick 才就绪；未到则请求定时唤醒。 */
PollResult delay_poll(struct Task *self) {
    if (g_ticks >= self->wake_tick) {
        self->out = (uint32_t)g_ticks; /* 完成：记录完成时刻 */
        return POLL_READY;
    }
    self->want = WAKE_TIMER;           /* 没好：登记 timer reactor 等唤醒 */
    return POLL_PENDING;
}

/* ===== 学生实现区 2：executor 调度循环 =====
 *
 * 轮询就绪队列；Pending 按 want 重新安排，Ready 出队丢弃。就绪队列空但还有
 * 定时任务在等时，wfi 等下一拍时钟中断，reactor 把到期任务唤回。
 *
 * 退化提醒：若所有 poll 都"一次就 Ready / 从不让出"，则每个任务被取出即
 * 跑完、再轮下一个，executor 退化为顺序批处理(无交错)——这正是"无让出即
 * 退化为顺序执行"。要交错，poll 必须在让出点返回 Pending(WAKE_NOW)。
 */
void exec_run(struct Executor *ex) {
    while (ex->rlen > 0 || ex->tlen > 0) {
        while (ex->rlen > 0) {
            struct Task *t = ready_pop(ex);
            PollResult r = t->poll(t);
            if (r == POLL_PENDING) {
                if (t->want == WAKE_NOW)
                    ready_push(ex, t);     /* yield：让出后回就绪队列 */
                else
                    timer_register(ex, t); /* 等定时：挂到 reactor */
            }
            /* POLL_READY：任务完成，自然出队、不再入队 */
        }
        if (ex->tlen > 0) {
            asm volatile("wfi");           /* 就绪队列空：等下一拍时钟中断 */
            reactor_wake_due(ex);          /* 唤醒到期的延时任务 */
        }
    }
}
