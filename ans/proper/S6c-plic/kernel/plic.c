/* S6c · PLIC 配置 + 外部中断处理（参考解）。 */
#include "plic.h"

volatile uint32_t g_claim_irq = 0;
volatile int      g_rx_got    = 0;
volatile uint8_t  g_rx_byte   = 0;
volatile int      g_plic_ctx  = 0;

/* 配置当前 hart 的 S-context：给 UART 源设优先级、放行该源、阈值清零。 */
void plic_init(unsigned long hartid) {
    int ctx = PLIC_S_CTX(hartid); /* 当前 hart 的 S-context 号 */
    g_plic_ctx = ctx;             /* 存给中断处理函数用 */

    /* 1) 源优先级非 0（优先级为 0 表示禁用，永不路由）。 */
    plic_write(PLIC_PRIORITY(UART0_IRQ), 1);

    /* 2) 在当前 hart 的 S-context 使能 UART 源：把第 UART0_IRQ 位置 1。 */
    plic_write(PLIC_ENABLE(ctx), 1u << UART0_IRQ);

    /* 3) 阈值=0：放行所有优先级 > 0 的中断。 */
    plic_write(PLIC_THRESHOLD(ctx), 0);
}

/* 外部中断处理：claim 取最高优先级待决源 → 读设备清源 → complete 通知 PLIC。
 * claim/complete 这一对握手是 PLIC 防丢/防重的核心：
 *   - claim（读 claim 寄存器）原子地拿到 irq 号并清掉该源的 pending（进入「服务中」）；
 *   - 读 UART RBR 撤掉设备这一侧的中断线（否则 complete 后会立刻再次 pending）；
 *   - complete（把 irq 号写回 claim 寄存器）告诉 PLIC「这次服务完了」，gateway 方可再次路由。 */
void plic_external_handler(void) {
    uint32_t irq = plic_read(PLIC_CLAIM(g_plic_ctx)); /* claim */
    g_claim_irq = irq;

    if (irq == UART0_IRQ) {
        g_rx_byte = uart_reg_read(UART0_BASE, UART_RBR); /* 读设备：取回字节并清 LSR.DR */
        g_rx_got++;
    }

    if (irq != 0) {
        plic_write(PLIC_CLAIM(g_plic_ctx), irq); /* complete */
    }
}
