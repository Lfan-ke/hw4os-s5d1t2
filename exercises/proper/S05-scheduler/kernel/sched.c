/* S05 · 任务表 + 协作式轮转调度器（学生填空版）。
 * 模型对应 rcore ch3：每个任务一根独立内核栈 + 一份 TaskContext；
 * 任务靠 yield 主动让出，schedule() 轮转挑下一个就绪任务并 __switch 过去。
 * 无时钟中断、无抢占——这正是“协作式”。
 *
 * 你要填两处：本文件的 schedule()，以及 switch.S 的 __switch 体。
 * 在 __switch 跑通（自检过）之前，main 会停在 SWITCH_TODO，不会跑调度循环。 */
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

/* ============ 上下文切换自检（与调度器解耦，给定） ============ */
static struct TaskContext probe_cx;
static struct TaskContext main_cx;
static uint8_t probe_stack[STACK_SIZE] __attribute__((aligned(16)));
static volatile uint64_t probe_magic = 0;

static void probe_entry(void) {
    probe_magic = 0xC0FFEEUL;       /* 证明确实切到了这里运行 */
    __switch(&probe_cx, &main_cx);  /* 切回 switch_selftest 的调用点 */
    for (;;) { }                    /* 不可达 */
}

int switch_selftest(void) {
    probe_magic = 0;
    probe_cx.ra = (uint64_t)probe_entry;
    probe_cx.sp = (uint64_t)(probe_stack + STACK_SIZE);
    __switch(&main_cx, &probe_cx);
    return probe_magic == 0xC0FFEEUL;
}

/* ============ 任务体（给定） ============ */
static void task_body(void) {
    int id = current;   /* 此局部跨多次 yield 存活，由 callee-saved 保住 */
    int i;
    for (i = 0; i < SLICES; i++) {
        if (run_log_n < (int)(sizeof(run_log) / sizeof(run_log[0])))
            run_log[run_log_n++] = id;
        tasks[id].state = READY;
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
        tasks[i].cx.ra = (uint64_t)task_body;
        tasks[i].cx.sp = (uint64_t)(task_stacks[i] + STACK_SIZE);
        for (j = 0; j < 12; j++) tasks[i].cx.s[j] = 0;
    }
}

/* ============ 调度器（核心，学生实现） ============
 * 被运行中的任务调用以让出 CPU。 */
void schedule(void) {
    int prev = current;
    (void)prev;
    /* TODO: round-robin 选下一个 READY 任务并切换过去：
     *   1) 从 (current+1)%NTASK 起环形扫描 NTASK 个槽，找第一个 state==READY 的任务 next；
     *   2) 若没有 READY（全部 EXITED）：
     *          current = -1;
     *          __switch(&tasks[prev].cx, &boot_cx);   // 切回 run_tasks 结束调度循环
     *          return;
     *   3) 否则切换：
     *          current = next;
     *          tasks[next].state = RUNNING;
     *          __switch(&tasks[prev].cx, &tasks[next].cx);
     * 注意：先用“旧 current”选 next，再更新 current。
     * 提示：boot_cx 是给定的调度循环上下文（run_tasks 里 __switch 进首个任务时已保存）。 */
}

/* 调度循环（给定）：拉起第 0 个任务；当所有任务退出后由 schedule() 切回这里。 */
void run_tasks(void) {
    current = 0;
    tasks[0].state = RUNNING;
    __switch(&boot_cx, &tasks[0].cx);
}

int sched_order_ok(void) {
    int expect = NTASK * SLICES;
    int i;
    if (run_log_n != expect) return 0;
    for (i = 0; i < run_log_n; i++)
        if (run_log[i] != i % NTASK) return 0;
    return 1;
}
