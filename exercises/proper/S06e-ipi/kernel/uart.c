/* S06e · NS16550 UART 最小 MMIO 访问 + 中断/回环配置（给定，同 S06c）。
 * 回环自激：写 THR 的字节内部环回成「收到」，置 LSR.DR 并（IER.ERBFI 允许时）
 * 拉高 UART→PLIC 中断线，从而把 PLIC 的 UART 源（src=10）置 pending - 
 * 供两核在 barrier 处一起 claim 仲裁。 */
#include "ipi.h"

uint8_t uart_reg_read(uint64_t base, int off) {
    return *(volatile uint8_t *)(base + (uint64_t)off);
}
void uart_reg_write(uint64_t base, int off, uint8_t v) {
    *(volatile uint8_t *)(base + (uint64_t)off) = v;
}

void uart_irq_loopback_init(uint64_t base) {
    uart_reg_write(base, UART_IER, IER_ERBFI); /* 只开 RX 中断 */
    uart_reg_write(base, UART_MCR, MCR_LOOP);  /* 回环自激（控制台从此静默）*/
}
