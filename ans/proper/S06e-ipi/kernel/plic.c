/* S06e · PLIC 配置 + 多核 claim 仲裁（参考解）。
 * 与 S06c 唯一的不同：同一个 UART 源被「两个 hart 的 S-context」同时使能，
 * 两核在 barrier 处同时读各自的 claim 寄存器 - claim 是原子领取，谁先读谁拿到非零 irq、
 * 把该源移出 pending；另一个核随后读到 0。于是「恰一个 hart」处理该外设中断。 */
#include "ipi.h"

/* 配本 hart 的 S-context：UART 源优先级非 0、在本 ctx 使能该源、阈值清零。 */
void plic_ctx_init(unsigned long hartid) {
    int ctx = PLIC_S_CTX(hartid);
    plic_write(PLIC_PRIORITY(UART0_IRQ), 1);          /* 0=禁用，置 1 才可路由 */
    plic_write(PLIC_ENABLE(ctx), 1u << UART0_IRQ);    /* 本 ctx 放行 UART 源 */
    plic_write(PLIC_THRESHOLD(ctx), 0);               /* 放行所有 priority>0 的源 */
}

/* 本 hart 的一次 claim 仲裁尝试（两核在 barrier 后各调一次）。
 *   - claim：读 PLIC_CLAIM(ctx) 原子取得最高优先级 pending 源、并把它移出 pending；
 *   - 赢家（irq==UART）：读 UART RBR 撤设备中断线、记账、complete（写回 irq）；
 *   - 输家：读到 0，记一笔 zero - 这正是「跨核只一个处理」的硬件仲裁结果。 */
void plic_claim_one(unsigned long hartid) {
    int ctx = PLIC_S_CTX(hartid);

    uint32_t irq = plic_read(PLIC_CLAIM(ctx));        /* claim（仲裁点）*/

    if (irq == UART0_IRQ) {
        g_mbox.claim_byte   = uart_reg_read(UART0_BASE, UART_RBR); /* 读设备清源 */
        g_mbox.claim_winner = (uint64_t)hartid + 1;
        __sync_fetch_and_add(&g_mbox.claim_nonzero, 1);
        plic_write(PLIC_CLAIM(ctx), irq);             /* complete */
    } else if (irq != 0) {
        plic_write(PLIC_CLAIM(ctx), irq);             /* 意外源也要 complete，免卡 gateway */
        __sync_fetch_and_add(&g_mbox.claim_other, 1);
    } else {
        __sync_fetch_and_add(&g_mbox.claim_zero, 1);  /* 输家：仲裁没轮到本核 */
    }
}
