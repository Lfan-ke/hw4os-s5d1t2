/* S04 · 内核入口/测试驱动（给定，勿改）：驱动异步运行时跑三个场景并判定。
 *
 *   [1] POLL_PASS        —— 单 future 状态机：Pending 推进 N 次后 Ready。
 *   [2] EXEC_PASS        —— executor 轮询多任务：交错(ABCABC)而非批处理(AABBCC)。
 *   [3] TIMER_FUTURE_PASS —— 延时 future：靠时钟中断推进 g_ticks + waker 重新入队完成。
 */
#include "kernel.h"
#include "riscv.h"
#include "async.h"

volatile uint64_t g_ticks = 0; /* 由 trap_handler 累加 */

/* —— 交错轨迹缓冲（count_poll 经 trace_emit 写入，用于判定多任务交错）—— */
static char g_trace[64];
static int  g_trace_len = 0;
void trace_emit(char c) {
    if (g_trace_len < (int)sizeof(g_trace) - 1) g_trace[g_trace_len++] = c;
    console_putchar(c); /* 同时打到控制台，肉眼可见交错 */
}
static int streq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

static struct Task mk_count(char label, int n, uint32_t magic) {
    struct Task t;
    t.poll = count_poll; t.state = 0; t.n = n; t.label = label;
    t.magic = magic; t.out = 0; t.want = WAKE_NOW; t.wake_tick = 0;
    return t;
}

/* [1] 单 future 状态机推进：直接手动 poll，应 Pending 3 次后 Ready。 */
static int test_poll(void) {
    kputs("[1] single future: poll() advances a state machine\n");
    struct Task t = mk_count(0, 3, 0xABCD); /* label=0：本测不打印交错 */
    int pend = 0, ok = 1;
    for (int i = 0; i < 3; i++) {
        if (t.poll(&t) == POLL_PENDING) pend++; else ok = 0;
    }
    if (t.poll(&t) != POLL_READY) ok = 0;
    if (t.out != 0xABCD) ok = 0;
    kputs("  pended="); kputdec(pend);
    kputs(" out="); kputhex(t.out); console_putchar('\n');
    if (ok && pend == 3) { kputs("POLL_PASS\n"); return 1; }
    kputs("POLL_FAIL\n"); return 0;
}

/* [2] executor 轮询 3 个让出任务：round-robin → 输出严格交错 ABCABC。 */
static int test_exec(void) {
    kputs("[2] executor interleaves 3 tasks: ");
    g_trace_len = 0;
    struct Executor ex; exec_init(&ex);
    struct Task a = mk_count('A', 2, 1);
    struct Task b = mk_count('B', 2, 2);
    struct Task c = mk_count('C', 2, 3);
    exec_spawn(&ex, &a); exec_spawn(&ex, &b); exec_spawn(&ex, &c);
    exec_run(&ex);
    console_putchar('\n');
    g_trace[g_trace_len] = 0;
    /* 交错=ABCABC（非顺序批处理 AABBCC），且三任务都跑完写回 out。 */
    if (streq(g_trace, "ABCABC") && a.out == 1 && b.out == 2 && c.out == 3) {
        kputs("EXEC_PASS\n"); return 1;
    }
    kputs("EXEC_FAIL\n"); return 0;
}

/* [3] 延时 future：等 3 拍时钟。executor 内部 wfi 等中断，reactor 到期唤醒。 */
static int test_timer(void) {
    kputs("[3] timer-driven delay future (waker re-enqueue)\n");
    trap_init();
    set_next_trigger();
    set_timer_irq();
    intr_on();
    uint64_t start = g_ticks;
    struct Executor ex; exec_init(&ex);
    struct Task d;
    d.poll = delay_poll; d.state = 0; d.n = 0; d.label = 0; d.magic = 0;
    d.out = 0; d.want = WAKE_TIMER; d.wake_tick = start + 3;
    exec_spawn(&ex, &d);
    exec_run(&ex); /* 阻塞至延时完成（靠时钟中断推进） */
    intr_off();
    uint64_t elapsed = g_ticks - start;
    kputs("  waited ticks="); kputdec(elapsed);
    kputs(" done_at="); kputdec(d.out); console_putchar('\n');
    if (elapsed >= 3 && d.out >= start + 3) { kputs("TIMER_FUTURE_PASS\n"); return 1; }
    kputs("TIMER_FUTURE_FAIL\n"); return 0;
}

void kmain(void) {
    kputs("\n[S04] embassy-style async runtime: Future/poll + executor\n");
    int ok = 1;
    ok &= test_poll();
    ok &= test_exec();
    ok &= test_timer();
    if (ok) kputs("ALL_PASS\n");
    else    kputs("SOME_TEST_FAILED\n");
}
