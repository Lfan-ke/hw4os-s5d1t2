/* S19 · 协作式两任务运行时实现（给定，沿用 S5/S14，本课不需改这里）。
 * 两个任务 = 两根独立内核栈 + 两份 TaskContext；任务靠 ipc_yield() 让出，
 * schedule() 轮转挑下一个就绪任务并 __switch 过去；都退出后切回 boot_cx。
 * 内置“切换上限”守卫：万一两任务互相让出却谁也不前进（学生 IPC 代码有 bug 时可能发生），
 * 也不会死锁——到上限就强制收尾，测试自然不 PASS。 */
#include "kernel.h"
#include "sched.h"

#define NTASK      2
#define STACK_SIZE 4096
#define SWITCH_CAP 1000000   /* 切换次数上限：远超正确程序所需，仅作活锁守卫 */

enum { READY = 1, RUNNING, EXITED };

struct Task {
    struct TaskContext cx;
    int  state;
    task_fn work;
};

static struct Task tasks[NTASK];
static uint8_t task_stacks[NTASK][STACK_SIZE] __attribute__((aligned(16)));
static struct TaskContext boot_cx;
static int current = -1;
static long switch_count = 0;
static int  capped = 0;

/* 调度器：从 current 之后环形找第一个 READY，切过去；全退出则回 boot_cx。 */
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
    if (next < 0) {
        current = -1;
        __switch(&tasks[prev].cx, &boot_cx);
        return;
    }
    current = next;
    tasks[next].state = RUNNING;
    __switch(&tasks[prev].cx, &tasks[next].cx);
}

void ipc_yield(void) {
    tasks[current].state = READY;
    schedule();
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
    switch_count = 0;
    capped = 0;
    current = -1;
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
