/* S06c · PLIC 配置 + 外部中断处理（学生填空）。
 * 你要补两处：(1) 在当前 hart 的 S-context 使能 UART 源；
 *            (2) 外部中断处理的 claim → 读设备 → complete 三步握手。
 * 占位实现能编译、能跑、不崩、不死循环，但 *_MISS、不会出 ALL_PASS。 */
#include "plic.h"

volatile uint32_t g_claim_irq = 0;
volatile int      g_rx_got    = 0;
volatile uint8_t  g_rx_byte   = 0;
volatile int      g_plic_ctx  = 0;

/* 配置当前 hart 的 S-context：给 UART 源设优先级、放行该源、阈值清零。 */
void plic_init(unsigned long hartid) {
    int ctx = PLIC_S_CTX(hartid); /* 当前 hart 的 S-context 号 */
    g_plic_ctx = ctx;             /* 存给中断处理函数用 */

    /* 1) 源优先级非 0（优先级为 0 表示禁用，永不路由）。已给。 */
    plic_write(PLIC_PRIORITY(UART0_IRQ), 1);

    /* TODO(1): 在当前 hart 的 S-context 使能 UART 源（把第 UART0_IRQ 位置 1）。
     *   提示：plic_write(PLIC_ENABLE(ctx), 1u << UART0_IRQ);
     * 不填则 PLIC 不会把该源路由到本 hart，外部中断永不触发（ENABLE_MISS）。 */

    /* 3) 阈值=0：放行所有优先级 > 0 的中断。已给。 */
    plic_write(PLIC_THRESHOLD(ctx), 0);
}

/* 外部中断处理：claim 取最高优先级待决源 → 读设备清源 → complete 通知 PLIC。
 * 三步缺一不可：
 *   - claim：读 PLIC_CLAIM(g_plic_ctx) 得 irq，并原子清该源 pending；
 *   - 读设备：读 UART RBR 取回字节、撤掉设备中断线（否则 complete 后立刻重新 pending → 风暴）；
 *   - complete：把 irq 写回 PLIC_CLAIM(g_plic_ctx)，gateway 方可再次路由。 */
void plic_external_handler(void) {
    /* TODO(2): 实现 claim → 读设备 → complete：
     *   uint32_t irq = plic_read(PLIC_CLAIM(g_plic_ctx));   // claim
     *   g_claim_irq = irq;
     *   if (irq == UART0_IRQ) {
     *       g_rx_byte = uart_reg_read(UART0_BASE, UART_RBR); // 读设备：取字节、清 LSR.DR
     *       g_rx_got++;
     *   }
     *   if (irq != 0) plic_write(PLIC_CLAIM(g_plic_ctx), irq); // complete
     *
     * 当前占位什么也不做：即使中断触发也不会 claim/清源，
     * 会被 trap.c 的有界守卫兜底屏蔽（不死循环），但 RX/COMPLETE 判据不过。 */
    (void)g_claim_irq;
    (void)g_rx_got;
    (void)g_rx_byte;
}
