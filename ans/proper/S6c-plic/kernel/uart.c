/* S6c · NS16550 UART 最小 MMIO 访问 + 中断/回环配置（给定）。
 * 与 S6 同：寄存器即一段 volatile 内存。这里额外开 RX 中断与回环，
 * 使「写一个字节」就能在内部环回成「收到一个字节」，从而确定性地自激一次中断。 */
#include "plic.h"

uint8_t uart_reg_read(uint64_t base, int off) {
    return *(volatile uint8_t *)(base + (uint64_t)off);
}
void uart_reg_write(uint64_t base, int off, uint8_t v) {
    *(volatile uint8_t *)(base + (uint64_t)off) = v;
}

/* 开「收到数据」中断（IER.ERBFI）+ 回环（MCR.LOOP）。
 * 回环下：写 THR 的字节会被内部送回接收路径，置 LSR.DR 并（在 IER 允许时）拉高 UART 中断线。 */
void uart_irq_loopback_init(uint64_t base) {
    uart_reg_write(base, UART_IER, IER_ERBFI); /* 只开 RX 中断，避免 TX 空中断干扰 */
    uart_reg_write(base, UART_MCR, MCR_LOOP);  /* 回环自激 */
}
