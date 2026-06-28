/* S06 · 平台总线简化版：驱动表 + compatible 字符匹配 + probe（参考解）。
 * 现代内核里设备由设备树/ACPI 提供 compatible 字符串，内核拿它去驱动表里找 probe。 */
#include "dev.h"

/* compatible 字符串比较：相等返回 0（仿 strcmp 语义）。 */
static int k_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/* ns16550a 的 probe：对该 base 做一次 UART 回环自测，确认设备真的在那。 */
static int ns16550_probe(uint64_t base) {
    if (base == 0) return 0;
    return uart_loopback_selftest(base) ? 1 : 0;
}

struct driver {
    const char *compatible;
    int (*probe)(uint64_t base);
};

static struct driver driver_table[] = {
    { "ns16550a",     ns16550_probe },
    { "sifive,uart0", 0             }, /* 表里可有多个，按 compatible 选 */
};

#define DRIVER_COUNT (int)(sizeof(driver_table) / sizeof(driver_table[0]))

int driver_match_and_probe(const char *compatible, uint64_t base) {
    if (!compatible) return -1;
    for (int i = 0; i < DRIVER_COUNT; i++) {
        if (k_strcmp(driver_table[i].compatible, compatible) == 0) {
            if (!driver_table[i].probe) return 0; /* 匹配但无 probe */
            return driver_table[i].probe(base) ? 1 : 0;
        }
    }
    return -1; /* 无驱动匹配 */
}
