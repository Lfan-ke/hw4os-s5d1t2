/* S6 · NS16550 UART 裸机 MMIO 驱动（学生填空）。
 * 关键认识：外设寄存器就是一段特殊内存，base+off 即其物理地址，必须用 volatile。 */
#include "dev.h"

/* TODO(1): 寄存器读 —— 返回 MMIO 地址 (base+off) 处的字节。
 *   提示：return *(volatile uint8_t *)(base + (uint64_t)off);
 * 现在的占位返回 0，会让所有状态位读出为 0（THRE/DR 永不就绪），UART 测试无法通过。 */
uint8_t uart_reg_read(uint64_t base, int off) {
    (void)base; (void)off;
    return 0; /* TODO: 改成真正的 volatile 读 */
}

/* TODO(2): 寄存器写 —— 把 v 写入 MMIO 地址 (base+off)。
 *   提示：*(volatile uint8_t *)(base + (uint64_t)off) = v; */
void uart_reg_write(uint64_t base, int off, uint8_t v) {
    (void)base; (void)off; (void)v;
    /* TODO: 改成真正的 volatile 写 */
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

/* 回环自测：置 MCR.LOOP，发若干字节并从 RBR 读回，逐字节比对，最后恢复 MCR。
 * （此函数已给；它依赖上面两个寄存器访问函数被正确实现。） */
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

    uart_reg_write(base, UART_MCR, saved_mcr);
    return ok;
}
