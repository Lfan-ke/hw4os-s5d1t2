/* S03 · form 2: a minimal cooperative RTOS (reference solution).
 *
 * A static set of tasks share the single CPU by *cooperatively* yielding.
 * No timer, no preemption: each task runs until it calls rtos_yield(), which
 * hands the CPU back to a plain round-robin scheduler. That scheduler over a
 * static task table is the essence of a tiny RTOS.
 */
#include "kernel.h"

#define NTASK  3
#define ROUNDS 3

/* Saved context = callee-saved registers only (ra, sp, s0..s11). See switch.S. */
struct TaskContext { uint64_t ra; uint64_t sp; uint64_t s[12]; };
void __switch(struct TaskContext *cur, struct TaskContext *next);

enum { T_READY = 0, T_EXITED = 1 };

static struct TaskContext sched_ctx;                 /* scheduler's own context */
static struct TaskContext task_ctx[NTASK];
static int  task_state[NTASK];
static void (*task_entry[NTASK])(void);
static uint64_t task_stack[NTASK][512] __attribute__((aligned(16)));

static volatile int current;                         /* task currently on CPU */
static int run_log[NTASK * ROUNDS];                  /* order tasks actually ran */
static volatile int log_n;

/* cooperative yield: save THIS task's context, resume the scheduler. */
void rtos_yield(void) {
    __switch(&task_ctx[current], &sched_ctx);
}

/* a task function returns here; mark it done and drop back to the scheduler. */
static void task_exit(void) {
    task_state[current] = T_EXITED;
    __switch(&task_ctx[current], &sched_ctx);         /* never returns */
}

/* the first thing a freshly-scheduled task executes. */
static void task_bootstrap(void) {
    task_entry[current]();
    task_exit();
}

/* demo workload: log my id, yield, repeat ROUNDS times. */
static void worker(void) {
    int id = current;
    for (int r = 0; r < ROUNDS; r++) {
        run_log[log_n++] = id;
        rtos_yield();
    }
}

/* prime a task's context so the first __switch lands in task_bootstrap with a
 * fresh stack. */
static void task_create(int i, void (*fn)(void)) {
    task_entry[i] = fn;
    task_state[i] = T_READY;
    task_ctx[i].ra = (uint64_t)task_bootstrap;
    task_ctx[i].sp = (uint64_t)&task_stack[i][512];   /* top; grows downward */
    for (int k = 0; k < 12; k++) task_ctx[i].s[k] = 0;
}

/* round-robin scheduler: keep switching into READY tasks until all have exited. */
static void rtos_run(void) {
    for (;;) {
        int alive = 0;
        for (int i = 0; i < NTASK; i++) {
            if (task_state[i] != T_READY) continue;
            alive++;
            current = i;
            __switch(&sched_ctx, &task_ctx[i]);        /* resume task i */
        }
        if (alive == 0) break;
    }
}

int rtos_demo(void) {
    kputs("[rtos] 3 static tasks, cooperative round-robin yield\n");
    log_n = 0;
    for (int i = 0; i < NTASK; i++) task_create(i, worker);
    rtos_run();

    /* perfect round-robin must yield run order 0,1,2, 0,1,2, 0,1,2 */
    int ok = (log_n == NTASK * ROUNDS);
    for (int i = 0; ok && i < log_n; i++)
        if (run_log[i] != i % NTASK) ok = 0;

    kputs("[rtos] run order: ");
    for (int i = 0; i < log_n; i++) {
        kputdec((uint64_t)run_log[i]);
        console_putchar(' ');
    }
    console_putchar('\n');
    if (ok) { kputs("RTOS_PASS\n"); return 1; }
    return 0;
}
