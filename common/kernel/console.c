/* 正经赛道共享控制台：基于 console_putchar 的最小输出工具。 */
#include "kernel.h"

void kputs(const char *s) {
    while (*s) console_putchar((unsigned char)*s++);
}

void kputhex(uint64_t x) {
    kputs("0x");
    for (int i = 60; i >= 0; i -= 4) {
        int d = (int)((x >> i) & 0xF);
        console_putchar(d < 10 ? '0' + d : 'a' + d - 10);
    }
}

void kputdec(uint64_t x) {
    char buf[21];
    int n = 0;
    if (x == 0) { console_putchar('0'); return; }
    while (x > 0) { buf[n++] = '0' + (int)(x % 10); x /= 10; }
    while (n > 0) console_putchar(buf[--n]);
}
