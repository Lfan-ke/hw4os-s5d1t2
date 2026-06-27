/* S6 · NS16550 UART 裸机 MMIO 驱动（参考解）。
 * 核心就是把寄存器当成内存读写：volatile 防止编译器优化掉。 */
#include "dev.h"

/* 寄存器读写：base+off 是 MMIO 地址，必须 volatile。 */
uint8_t uart_reg_read(uint64_t base, int off) {
    return *(volatile uint8_t *)(base + (uint64_t)off);
}
void uart_reg_write(uint64_t base, int off, uint8_t v) {
    *(volatile uint8_t *)(base + (uint64_t)off) = v;
}

/* 发一个字符：先等 LSR.THRE（发送保持寄存器空），再写 THR。带自旋上限防卡死。 */
void uart_putc(uint64_t base, char c) {
    for (int spin = 0; spin < 2000000; spin++) {
        if (uart_reg_read(base, UART_LSR) & LSR_THRE) break;
    }
    uart_reg_write(base, UART_THR, (uint8_t)c);
}

void uart_puts(uint64_t base, const char *s) {
    while (*s) uart_putc(base, *s++);
}

/* 回环自测：置 MCR.LOOP，发若干字节并从 RBR 读回，逐字节比对，最后恢复 MCR。 */
int uart_loopback_selftest(uint64_t base) {
    const char *msg = "RV64";
    uint8_t saved_mcr = uart_reg_read(base, UART_MCR);
    uart_reg_write(base, UART_MCR, (uint8_t)(saved_mcr | MCR_LOOP));

    int ok = 1;
    for (int i = 0; msg[i]; i++) {
        int spin = 0;
        while (!(uart_reg_read(base, UART_LSR) & LSR_THRE)) {
            if (++spin > 2000000) { ok = 0; break; }
        }
        uart_reg_write(base, UART_THR, (uint8_t)msg[i]);
        spin = 0;
        while (!(uart_reg_read(base, UART_LSR) & LSR_DR)) {
            if (++spin > 2000000) { ok = 0; break; }
        }
        uint8_t got = uart_reg_read(base, UART_RBR);
        if (got != (uint8_t)msg[i]) ok = 0;
    }

    uart_reg_write(base, UART_MCR, saved_mcr); /* 恢复，避免影响后续控制台 */
    return ok;
}
