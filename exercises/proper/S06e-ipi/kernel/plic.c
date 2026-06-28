/* S06e · PLIC 配置 + 多核 claim 仲裁（学生填空）。
 * plic_ctx_init 已给（同 S06c：把 UART 源使能到本 hart 的 S-context）。
 * 你要补 plic_claim_one：本 hart 在 barrier 后读一次 claim 参与跨核仲裁。
 * 占位实现能编译、能运行、不崩溃、不死循环，但不会记账 → 无 CLAIM_PASS / SMP_PASS。 */
#include "ipi.h"

/* 配本 hart 的 S-context：UART 源优先级非 0、在本 ctx 使能该源、阈值清零。 */
void plic_ctx_init(unsigned long hartid) {
    int ctx = PLIC_S_CTX(hartid);
    plic_write(PLIC_PRIORITY(UART0_IRQ), 1);          /* 0=禁用，置 1 才可路由 */
    plic_write(PLIC_ENABLE(ctx), 1u << UART0_IRQ);    /* 本 ctx 放行 UART 源（两核都使能 → 仲裁）*/
    plic_write(PLIC_THRESHOLD(ctx), 0);               /* 放行所有 priority>0 的源 */
}

/* 本 hart 的一次 claim 仲裁尝试（两核在 barrier 后各调一次）。
 * claim 是「谁先读谁得到」的原子领取：先读到的核拿到非零 irq 并把该源移出 pending，
 * 另一个核随后读到 0 - 这正是「跨核只一个 hart 处理该外设中断」。 */
void plic_claim_one(unsigned long hartid) {
    int ctx = PLIC_S_CTX(hartid);
    (void)ctx;

    /* TODO: 实现一次 claim 仲裁：
     *   uint32_t irq = plic_read(PLIC_CLAIM(ctx));            // claim（仲裁点）
     *   if (irq == UART0_IRQ) {                              // 本核赢得仲裁
     *       g_mbox.claim_byte   = uart_reg_read(UART0_BASE, UART_RBR); // 读设备清源
     *       g_mbox.claim_winner = (uint64_t)hartid + 1;
     *       __sync_fetch_and_add(&g_mbox.claim_nonzero, 1);
     *       plic_write(PLIC_CLAIM(ctx), irq);               // complete
     *   } else if (irq != 0) {
     *       plic_write(PLIC_CLAIM(ctx), irq);               // 意外源也要 complete
     *       __sync_fetch_and_add(&g_mbox.claim_other, 1);
     *   } else {
     *       __sync_fetch_and_add(&g_mbox.claim_zero, 1);    // 本核没轮到（输家）
     *   }
     * 当前占位什么也不做：两核都不记账，CLAIM_PASS 判据不过（引导核轮询超时后判 *_MISS）。 */
    (void)hartid;
}
