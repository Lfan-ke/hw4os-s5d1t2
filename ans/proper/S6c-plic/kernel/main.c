/* S6c · 内核入口 / 测试驱动（给定）。
 * 流程：配 trap → 配 PLIC → 配 UART(开 RX 中断+回环) → 开 S 态外部中断
 *      → 向 UART 写一字节(回环→收到→触发中断) → 经 PLIC 进入 S 态外部中断
 *      → claim/读回字节/complete → 关回环恢复控制台 → 校验四项判据。
 *
 * 注意：本实验复用「同一个 UART」做回环自激，而 SBI 控制台也用它输出。
 * 一旦置上 MCR.LOOP，UART 发送被内部环回、不再出到终端，故所有打印都
 * 推迟到「关掉回环、恢复控制台」之后统一输出（测试期间只记录、不打印）。
 *
 * 全程有界：等待中断用计数自旋（非 wfi 死等），中断风暴由 trap.c 兜底屏蔽。 */
#include "kernel.h"
#include "riscv.h"
#include "plic.h"

#define TEST_BYTE 'A'
#define WAIT_SPIN 2000000

static int wait_until_traps(uint64_t target) {
    for (int i = 0; i < WAIT_SPIN; i++) {
        if (g_ext_traps >= target) return 1;
        asm volatile("nop");
    }
    return 0;
}

void kmain(void) {
    /* SBI 经 a0 把当前 hartid 传进来（entry.S 未触碰 a0，故首句即可取到）。
     * -smp 4 下启动 hart 不确定，必须按它配置自己的 PLIC S-context。 */
    register unsigned long a0 asm("a0");
    unsigned long hartid = a0;

    kputs("\n[S6c] PLIC external interrupt (UART loopback self-excite)\n");
    kputs("boot hartid=");
    kputdec(hartid);
    console_putchar('\n');

    /* —— 1. 配置（先 trap/PLIC，再开 UART 回环；回环一开控制台即静默）—— */
    trap_init();                        /* stvec → __alltraps（复用 S2）*/
    plic_init(hartid);                  /* PLIC：当前 hart S-context 的优先级/使能/阈值 */
    uart_irq_loopback_init(UART0_BASE); /* UART：开 RX 中断 + 回环（控制台从此静默）*/
    int ctx = PLIC_S_CTX(hartid);

    /* 判据 1：寄存器配置正确（先记录，稍后统一打印）。 */
    int prio_ok   = (plic_read(PLIC_PRIORITY(UART0_IRQ)) == 1);
    int enable_ok = (plic_read(PLIC_ENABLE(ctx)) & (1u << UART0_IRQ)) != 0;
    int thresh_ok = (plic_read(PLIC_THRESHOLD(ctx)) == 0);
    int ier_ok    = (uart_reg_read(UART0_BASE, UART_IER) & IER_ERBFI) != 0;
    int mcr_ok    = (uart_reg_read(UART0_BASE, UART_MCR) & MCR_LOOP) != 0;
    int setup_ok  = prio_ok && enable_ok && thresh_ok && ier_ok && mcr_ok;

    /* —— 2. 开 S 态外部中断 —— */
    ext_irq_on(); /* sie.SEIE */
    intr_on();    /* sstatus.SIE */

    /* —— 3. 自激一次中断：写 THR，回环成 RX；中断随即异步触发 —— */
    uart_reg_write(UART0_BASE, UART_THR, TEST_BYTE);
    int fired = wait_until_traps(1);

    /* —— 4. 再观察一段时间，确认 complete 后不再重复触发 —— */
    uint64_t after_first = g_ext_traps;
    for (int i = 0; i < WAIT_SPIN; i++) asm volatile("nop");

    /* —— 5. 关回环 + 关中断，恢复 SBI 控制台 —— */
    ext_irq_off();
    intr_off();
    uart_reg_write(UART0_BASE, UART_IER, 0);
    uart_reg_write(UART0_BASE, UART_MCR, 0);

    /* —— 6. 统一输出判据 —— */
    if (setup_ok) {
        kputs("PLIC_SETUP_PASS\n");
    } else {
        if (!prio_ok)   kputs("PRIO_MISS\n");
        if (!enable_ok) kputs("ENABLE_MISS\n");
        if (!thresh_ok) kputs("THRESH_MISS\n");
        if (!ier_ok)    kputs("IER_MISS\n");
        if (!mcr_ok)    kputs("MCR_MISS\n");
    }

    int irq_ok = fired && (g_last_scause & SCAUSE_INT_BIT) &&
                 (g_last_scause & 0xff) == SCAUSE_S_EXTERNAL;
    if (irq_ok) {
        kputs("IRQ_PASS\n");
    } else {
        kputs("IRQ_MISS scause=");
        kputhex(g_last_scause);
        console_putchar('\n');
    }

    int rx_ok = (g_claim_irq == UART0_IRQ) && (g_rx_got >= 1) &&
                (g_rx_byte == (uint8_t)TEST_BYTE);
    if (rx_ok) {
        kputs("RX_PASS\n");
    } else {
        kputs("RX_MISS irq=");
        kputdec(g_claim_irq);
        kputs(" byte=");
        kputhex(g_rx_byte);
        console_putchar('\n');
    }

    int complete_ok = (after_first == 1) && (g_ext_traps == 1) && (g_rx_got == 1);
    if (complete_ok) {
        kputs("COMPLETE_PASS\n");
    } else {
        kputs("COMPLETE_MISS traps=");
        kputdec(g_ext_traps);
        console_putchar('\n');
    }

    if (setup_ok && irq_ok && rx_ok && complete_ok) {
        kputs("ALL_PASS\n");
    }
}
