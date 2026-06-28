/* S06b · HAL 自验证 harness（给定，勿改）。
 * 在迷你 Abstract Machine 之上，依次检验三层：
 *   TRM  putch 输出 + heap 可读写
 *   IOE  连读 TIMER_UPTIME 单调递增
 *   CTE  仿 yield-os 造 2 个任务互相 yield，调度器轮转，运行序应为 A B A B
 * 全过 → ALL_PASS（失败用 _MISS 诊断，绝不输出 FAIL/panic/UNEXPECTED）。 */
#include "am.h"
#include "kernel.h"

/* ============ TRM 自检：输出 + 一块可用内存 ============ */
static int trm_test(void) {
    /* putch 输出（也顺带证明控制台通路 OK）。 */
    const char *s = "  TRM: hello via putch\n";
    for (const char *p = s; *p; p++) putch(*p);

    /* heap 可读写：写一段图案再读回校验。 */
    volatile uint8_t *p = (volatile uint8_t *)heap.start;
    size_t n = 256;
    if ((uint8_t *)heap.end - (uint8_t *)heap.start < (long)n) return 0;
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)(i * 7 + 1);
    for (size_t i = 0; i < n; i++)
        if (p[i] != (uint8_t)(i * 7 + 1)) return 0;
    return 1;
}

/* ============ IOE 自检：连读 uptime 单调递增 ============ */
static void io_delay(void) {
    for (volatile int i = 0; i < 200000; i++) { }
}
static int ioe_test(void) {
    /* 经 IOE 写串口（设备无关地输出一个字符）。 */
    AM_UART_TX_T tx = { '.' };
    ioe_write(AM_UART_TX, &tx);

    AM_TIMER_UPTIME_T t;
    uint64_t prev = 0, first = 0;
    for (int k = 0; k < 4; k++) {
        ioe_read(AM_TIMER_UPTIME, &t);
        if (k == 0) first = t.us;
        else if (t.us < prev) return 0;   /* 不得回退 */
        prev = t.us;
        io_delay();
    }
    putch('\n');
    return prev > first;                   /* 整体确实在前进 */
}

/* ============ CTE 自检：仿 yield-os 的 2 任务协作调度 ============ */
#define NPROC 2
#define ROUNDS 2
#define STACK_SIZE (4096 * 4)

typedef struct {
    uint8_t stack[STACK_SIZE] __attribute__((aligned(16)));
    Context *cp;     /* 该任务最近一次让出时保存的现场 */
    int done;        /* 任务是否已结束 */
    char name;       /* 'A' / 'B' */
} PCB;

static PCB pcb[NPROC];
static int cur = -1;            /* 当前任务下标，-1 表示 boot（main）*/
static Context *boot_cp;        /* main 第一次 yield 时保存的现场 */

static char run_log[64];
static int run_log_n = 0;

/* 任务体：每轮记录自己的名字，然后 yield 让出；ROUNDS 轮后标记 done 并永久让出。 */
static void task_fn(void *arg) {
    long id = (long)arg;
    for (int i = 0; i < ROUNDS; i++) {
        if (run_log_n < (int)sizeof(run_log)) run_log[run_log_n++] = pcb[id].name;
        yield();
    }
    pcb[id].done = 1;
    yield();          /* 交出 CPU，不再被选中 → 不返回 */
    for (;;) { }      /* 不可达 */
}

/* 调度器 = CTE 的事件处理函数：每次 EVENT_YIELD 轮转挑下一个未结束任务。
 * 保存让出者现场到其 PCB，按 round-robin 选 next，返回 next 的现场；
 * 全部结束则返回 boot 现场，让 main 继续。 */
static Context *schedule(Event ev, Context *prev) {
    (void)ev;
    if (cur < 0) boot_cp = prev;     /* 让出者是 main */
    else pcb[cur].cp = prev;         /* 让出者是某任务 */

    int base = (cur < 0) ? 0 : (cur + 1);
    for (int k = 0; k < NPROC; k++) {
        int idx = (base + k) % NPROC;
        if (!pcb[idx].done) { cur = idx; return pcb[idx].cp; }
    }
    cur = -1;
    return boot_cp;                  /* 全部结束 → 回 main */
}

static int cte_test(void) {
    cte_init(schedule);
    for (long i = 0; i < NPROC; i++) {
        pcb[i].name = (char)('A' + i);
        pcb[i].done = 0;
        Area kstack = { pcb[i].stack, pcb[i].stack + STACK_SIZE };
        pcb[i].cp = kcontext(kstack, task_fn, (void *)i);
    }
    cur = -1;
    run_log_n = 0;
    yield();          /* 进入调度：自陷 → schedule 拉起任务 A；全部结束后回到这里 */

    /* 期望运行序：A B A B（NPROC*ROUNDS 个字符的轮转）。 */
    run_log[run_log_n < (int)sizeof(run_log) ? run_log_n : (int)sizeof(run_log) - 1] = '\0';
    kputs("  CTE run order: ");
    kputs(run_log);
    console_putchar('\n');

    if (run_log_n != NPROC * ROUNDS) return 0;
    for (int i = 0; i < run_log_n; i++)
        if (run_log[i] != (char)('A' + (i % NPROC))) return 0;
    return 1;
}

void kmain(void) {
    kputs("\n[S06b] mini Abstract Machine (HAL): TRM + IOE + CTE\n");

    if (trm_test()) kputs("TRM_PASS\n");
    else { kputs("TRM_MISS\n"); return; }

    ioe_init();
    if (ioe_test()) kputs("IOE_PASS\n");
    else { kputs("IOE_MISS\n"); return; }

    if (cte_test()) kputs("CTE_PASS\n");
    else { kputs("CTE_MISS\n"); return; }

    kputs("ALL_PASS\n");
    /* kmain 返回 → entry.S 自动 k_shutdown 退出 qemu。 */
}
