/* S05d · 协作式两任务运行时实现（给定，本课不需改这里）。
 * 在 S05 轮转调度的基础上增加 BLOCKED 态与 sched_block/sched_wake：
 *   schedule() 只在 READY 的任务里轮转挑下一个；BLOCKED 的任务被跳过（零 CPU）。
 * 内置“切换上限”守卫：万一同步原语实现有 bug 导致两任务互相让出却不前进、
 * 或所有任务都阻塞（死锁），也不会真卡死——到上限/无就绪任务就强制收尾，测试自然不 PASS。 */
#include "kernel.h"
#include "sched.h"

#define STACK_SIZE 4096
#define SWITCH_CAP 1000000   /* 切换次数上限：远超正确程序所需，仅作活锁/死锁守卫 */

enum { READY = 1, RUNNING, BLOCKED, EXITED };

struct Task {
    struct TaskContext cx;
    int     state;
    task_fn work;
};

static struct Task tasks[NTASK];
static uint8_t task_stacks[NTASK][STACK_SIZE] __attribute__((aligned(16)));
static struct TaskContext boot_cx;
static int  current = -1;
static long switch_count = 0;
static int  capped = 0;

long g_block_events = 0;

/* 调度器：从 current 之后环形找第一个 READY，切过去；
 * 无 READY（全 EXITED 正常收尾，或全 BLOCKED 即死锁）则切回 boot_cx 结束本次运行。 */
static void schedule(void) {
    int prev = current;
    int next = -1;
    int k;

    if (++switch_count > SWITCH_CAP) {     /* 活锁守卫 */
        capped = 1;
        current = -1;
        __switch(&tasks[prev].cx, &boot_cx);
        return;
    }
    for (k = 1; k <= NTASK; k++) {
        int cand = (current + k) % NTASK;
        if (tasks[cand].state == READY) { next = cand; break; }
    }
    if (next < 0) {                        /* 无就绪任务：正常结束 或 死锁兜底 */
        current = -1;
        __switch(&tasks[prev].cx, &boot_cx);
        return;
    }
    current = next;
    tasks[next].state = RUNNING;
    __switch(&tasks[prev].cx, &tasks[next].cx);
}

int cur_task(void) { return current; }

void sync_yield(void) {
    tasks[current].state = READY;          /* 让出后仍就绪，下轮还会被选中（不是阻塞） */
    schedule();
}

void sched_block(void) {
    tasks[current].state = BLOCKED;        /* 真正阻塞：在被唤醒前不再被调度 */
    g_block_events++;
    schedule();
}

void sched_wake(int id) {
    if (id >= 0 && id < NTASK && tasks[id].state == BLOCKED)
        tasks[id].state = READY;           /* 仅置回就绪，等调度器之后切过去 */
}

/* 任务跳板：跑真正的工作体，跑完置 EXITED 并让出（永不真正返回）。 */
static void task_trampoline(void) {
    int id = current;
    tasks[id].work();
    tasks[id].state = EXITED;
    schedule();
}

int sched_run_pair(task_fn f0, task_fn f1) {
    task_fn fns[NTASK];
    int i, j;

    fns[0] = f0;
    fns[1] = f1;
    switch_count  = 0;
    capped        = 0;
    current       = -1;
    g_block_events = 0;
    for (i = 0; i < NTASK; i++) {
        tasks[i].state = READY;
        tasks[i].work  = fns[i];
        tasks[i].cx.ra = (uint64_t)task_trampoline;
        tasks[i].cx.sp = (uint64_t)(task_stacks[i] + STACK_SIZE);
        for (j = 0; j < 12; j++) tasks[i].cx.s[j] = 0;
    }
    current = 0;
    tasks[0].state = RUNNING;
    __switch(&boot_cx, &tasks[0].cx);   /* 拉起 f0；两任务都退出后切回这里 */
    return capped ? 0 : 1;
}
