/* S5 · 任务表 + 协作式轮转调度器（参考解）。
 * 模型对应 rcore ch3：每个任务一根独立内核栈 + 一份 TaskContext；
 * 任务靠 yield 主动让出，schedule() 轮转挑下一个就绪任务并 __switch 过去。
 * 无时钟中断、无抢占——这正是“协作式”。 */
#include "kernel.h"
#include "sched.h"

#define STACK_SIZE 4096

enum { UNINIT = 0, READY, RUNNING, EXITED };

struct Task {
    struct TaskContext cx;
    int state;
    int id;
};

static struct Task tasks[NTASK];
static uint8_t task_stacks[NTASK][STACK_SIZE] __attribute__((aligned(16)));
static struct TaskContext boot_cx;   /* 调度循环（boot 栈）的上下文 */
static int current = -1;             /* 正在占用 CPU 的任务 id */

int run_log[64];
int run_log_n = 0;

/* ============ 上下文切换自检（与调度器解耦） ============ */
static struct TaskContext probe_cx;
static struct TaskContext main_cx;
static uint8_t probe_stack[STACK_SIZE] __attribute__((aligned(16)));
static volatile uint64_t probe_magic = 0;

static void probe_entry(void) {
    probe_magic = 0xC0FFEEUL;       /* 证明确实切到了这里运行 */
    __switch(&probe_cx, &main_cx);  /* 切回 switch_selftest 的调用点 */
    for (;;) { }                    /* 不可达 */
}

/* 来回切一趟：main -> probe（运行并置 magic）-> main。
 * 切回来后若 magic 正确，说明 ra/sp 整存整取无误、控制流“回来值对”。 */
int switch_selftest(void) {
    probe_magic = 0;
    probe_cx.ra = (uint64_t)probe_entry;
    probe_cx.sp = (uint64_t)(probe_stack + STACK_SIZE);
    __switch(&main_cx, &probe_cx);
    return probe_magic == 0xC0FFEEUL;
}

/* ============ 任务体 ============ */
/* 每个任务：占用一个时间片就记录自己、然后 yield；共 SLICES 次，最后退出。 */
static void task_body(void) {
    int id = current;   /* 此局部跨多次 yield 存活，由 callee-saved 保住 */
    int i;
    for (i = 0; i < SLICES; i++) {
        if (run_log_n < (int)(sizeof(run_log) / sizeof(run_log[0])))
            run_log[run_log_n++] = id;
        tasks[id].state = READY;    /* 让出后仍就绪，等下轮再被选中 */
        schedule();                 /* 交出 CPU；下次被调度回到这里继续 */
    }
    tasks[id].state = EXITED;
    schedule();                     /* 退出：不再返回 */
}

void sched_init(void) {
    int i, j;
    run_log_n = 0;
    current = -1;
    for (i = 0; i < NTASK; i++) {
        tasks[i].id = i;
        tasks[i].state = READY;
        /* 首次被调度时：ret 落到 task_body，sp 指向本任务栈顶。 */
        tasks[i].cx.ra = (uint64_t)task_body;
        tasks[i].cx.sp = (uint64_t)(task_stacks[i] + STACK_SIZE);
        for (j = 0; j < 12; j++) tasks[i].cx.s[j] = 0;
    }
}

/* ============ 调度器（核心） ============
 * 被运行中的任务调用以让出 CPU：从 current 之后环形扫描，
 * 选第一个 READY 的任务（round-robin），切换过去；
 * 若全部 EXITED，则切回 boot_cx 结束调度循环。 */
void schedule(void) {
    int prev = current;
    int next = -1;
    int k;
    for (k = 1; k <= NTASK; k++) {
        int jcand = (current + k) % NTASK;
        if (tasks[jcand].state == READY) { next = jcand; break; }
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

/* 调度循环：拉起第 0 个任务；当所有任务退出后由 schedule() 切回这里。 */
void run_tasks(void) {
    current = 0;
    tasks[0].state = RUNNING;
    __switch(&boot_cx, &tasks[0].cx);
}

/* 正确的轮转序应是 0,1,2,0,1,2,...（NTASK*SLICES 片）。 */
int sched_order_ok(void) {
    int expect = NTASK * SLICES;
    int i;
    if (run_log_n != expect) return 0;
    for (i = 0; i < run_log_n; i++)
        if (run_log[i] != i % NTASK) return 0;
    return 1;
}
