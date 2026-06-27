/* S5 · 内核入口/测试驱动（给定）：上下文切换自检 + 跑 3 任务轮转 + 校验运行序。 */
#include "kernel.h"
#include "sched.h"

void kmain(void) {
    int i;
    kputs("\n[S5] cooperative scheduler: TaskContext + __switch + round-robin\n");

    /* (1) 上下文切换自检：来回切一趟，验证“回来值对”。 */
    if (!switch_selftest()) {
        kputs("switch self-test did not round-trip (implement switch.S / schedule)\n");
        kputs("SWITCH_TODO\n");
        return; /* -> k_shutdown，不打印 ALL_PASS */
    }
    kputs("SWITCH_PASS\n");

    /* (2) 3 个协作式任务，各 yield 数次，调度器轮转。 */
    sched_init();
    run_tasks(); /* 全部任务退出后返回 */

    /* (3) 校验记录到的运行序是否为正确的轮转序。 */
    kputs("run order:");
    for (i = 0; i < run_log_n; i++) {
        console_putchar(' ');
        kputdec((uint64_t)run_log[i]);
    }
    console_putchar('\n');

    if (sched_order_ok()) {
        kputs("SCHED_PASS\n");
        kputs("ALL_PASS\n");
    } else {
        kputs("run order mismatch\n");
    }
}
