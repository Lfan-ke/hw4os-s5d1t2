/* S4 · embassy 式无栈异步运行时（内核内）。
 *
 * 心智模型：无栈协程 = 一台被反复 poll 的状态机。
 *   - 有栈协程"换的是栈指针"；无栈协程"换的是状态号"。
 *   - 凡是"跨让出点还要活着"的局部，都塞进 struct Task（不是函数栈）。
 *   - executor 轮询任务队列：Pending 留着等唤醒，Ready 出队丢弃。
 *   - 延时 future：返回 Pending 并登记到 timer reactor；时钟中断推进
 *     g_ticks，reactor 到期把任务重新入就绪队列（这就是 waker 的本质）。
 */
#ifndef OSLAB_ASYNC_H
#define OSLAB_ASYNC_H
#include <stdint.h>

/* poll 的结果：还没好(Pending) / 好了(Ready)。 */
typedef enum { POLL_PENDING = 0, POLL_READY = 1 } PollResult;

/* Pending 时告诉 executor 何时把自己重新唤醒：
 *   WAKE_NOW   —— 立即可再调度（yield_now：让出后马上回就绪队列）
 *   WAKE_TIMER —— 等定时器：登记到 reactor，到 wake_tick 拍才回队 */
enum { WAKE_NOW = 0, WAKE_TIMER = 1 };

struct Task;
typedef PollResult (*poll_fn)(struct Task *self);

/* 无栈协程的"任务" = 状态机 + 它跨让出点存活的全部状态。 */
struct Task {
    poll_fn  poll;       /* 状态机推进函数 */
    int      state;      /* 当前状态号（推进进度，不放在栈上） */
    int      n;          /* 计数 future：还要让出几次 */
    char     label;      /* 交错输出标签（0 = 不打印） */
    uint32_t magic;      /* 就绪时的产出值 */
    uint32_t out;        /* poll 返回 READY 时写入 */
    int      want;       /* Pending 时的唤醒方式 WAKE_NOW / WAKE_TIMER */
    uint64_t wake_tick;  /* 延时 future：g_ticks 达此值即就绪 */
};

#define MAX_TASKS 16
/* 最小 executor：一个环形就绪队列 + 一组挂在 timer reactor 上的等待任务。 */
struct Executor {
    struct Task *ready[MAX_TASKS]; /* 就绪队列（环形） */
    int rhead, rtail, rlen;
    struct Task *timer[MAX_TASKS]; /* timer reactor：等定时唤醒的任务 */
    int tlen;
};

void exec_init(struct Executor *ex);
void exec_spawn(struct Executor *ex, struct Task *t);
void exec_run(struct Executor *ex);

/* 两个示例 future 的 poll（学生实现）。 */
PollResult count_poll(struct Task *self);
PollResult delay_poll(struct Task *self);

/* 由 harness(main.c) 提供：记录交错执行轨迹（用于判定多任务交错）。 */
void trace_emit(char c);

#endif
