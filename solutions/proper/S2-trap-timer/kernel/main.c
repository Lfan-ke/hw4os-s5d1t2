/* S2 · 内核入口/测试驱动（给定）：开时钟中断，等若干拍后退出。 */
#include "kernel.h"
#include "riscv.h"

volatile uint64_t g_ticks = 0; /* 由 trap_handler 累加 */

void kmain(void) {
    kputs("\n[S2] trap + timer interrupt\n");
    trap_init();        /* 设 stvec → __alltraps */
    set_next_trigger(); /* 排第一次时钟中断 */
    set_timer_irq();    /* sie.STIE */
    intr_on();          /* sstatus.SIE */
    kputs("waiting for 5 timer ticks...\n");
    while (g_ticks < 5) {
        asm volatile("wfi");
    }
    intr_off();
    kputs("ticks=");
    kputdec(g_ticks);
    console_putchar('\n');
    kputs("TIMER_PASS\n");
    kputs("TRAP_PASS\n");
    kputs("ALL_PASS\n");
}
