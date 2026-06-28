/* S06 · 内核入口/测试驱动（给定）：MMIO UART → 设备树解析 → 驱动 probe。
 *
 * 三关：
 *   ① 直接读写 NS16550 寄存器收发字符（与 SBI 控制台并存做对照） → UART_PASS
 *   ② 解析内嵌 dtb，找到 uart 节点的 reg 基址                    → DT_PASS
 *   ③ 用 compatible 在驱动表里匹配并 probe                       → PROBE_PASS
 * 全过 → ALL_PASS。 */
#include "kernel.h"
#include "dev.h"

static int name_is_uart(const char *n) {
    /* 节点名以 "uart" 开头即认为是串口（如 uart@10000000） */
    return n && n[0] == 'u' && n[1] == 'a' && n[2] == 'r' && n[3] == 't';
}

void kmain(void) {
    int pass_uart = 0, pass_dt = 0, pass_probe = 0;
    kputs("\n[S06] bare-metal MMIO driver + device tree\n");

    /* —— ① 直接 MMIO 操作 NS16550 —— */
    /* 经 SBI 打印一行，再经裸 MMIO 打印一行：两条都该出现在同一串口上。 */
    kputs("[sbi-console] hello via SBI\n");
    uart_puts(UART0_BASE, "[uart-mmio]  hello via NS16550 registers\n");
    if (uart_loopback_selftest(UART0_BASE)) {
        kputs("uart loopback echo matched\n");
        kputs("UART_PASS\n");
        pass_uart = 1;
    } else {
        kputs("uart loopback mismatch\n");
    }

    /* —— ② 解析设备树 —— */
    struct dt_device devs[8];
    int n = 0;
    if (fdt_check_magic(device_dtb)) {
        kputs("fdt magic ok, dtb bytes = ");
        kputdec((uint64_t)(device_dtb_end - device_dtb));
        console_putchar('\n');
        n = fdt_scan(device_dtb, devs, 8);
    } else {
        kputs("fdt magic mismatch\n");
    }
    kputs("dt device nodes = ");
    kputdec((uint64_t)n);
    console_putchar('\n');

    uint64_t dt_uart_base = 0;
    for (int i = 0; i < n; i++) {
        if (name_is_uart(devs[i].name) && devs[i].has_reg) {
            dt_uart_base = devs[i].reg_addr;
            kputs("  node ");
            kputs(devs[i].name);
            kputs(" reg=");
            kputhex(devs[i].reg_addr);
            console_putchar('\n');
        }
    }
    if (dt_uart_base == UART0_BASE) {
        kputs("DT_PASS\n");
        pass_dt = 1;
    }

    /* —— ③ 驱动表 compatible 匹配 → probe —— */
    for (int i = 0; i < n; i++) {
        if (!devs[i].compatible) continue;
        uint64_t base = devs[i].has_reg ? devs[i].reg_addr : 0;
        int r = driver_match_and_probe(devs[i].compatible, base);
        if (r == 1) {
            kputs("  probe ok: compatible=");
            kputs(devs[i].compatible);
            console_putchar('\n');
            pass_probe = 1;
        } else if (r == 0) {
            kputs("  probe returned not-ready for ");
            kputs(devs[i].compatible);
            console_putchar('\n');
        }
    }
    if (pass_probe) kputs("PROBE_PASS\n");

    if (pass_uart && pass_dt && pass_probe) kputs("ALL_PASS\n");
}
