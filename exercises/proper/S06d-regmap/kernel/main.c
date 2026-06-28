/* S06d · 内核入口 / 测试驱动（给定）。三关把「类型化寄存器图」与「平台总线生命周期」连起来：
 *   ① 类型化寄存器图：NS16550(struct 字节寄存器+union 位段) 回环自测；
 *      PLIC(按区间拆的 typed 子结构) 配置后回读校验。                 → REGMAP_PASS
 *   ② driver table → probe → bind → /dev：解析 dtb 得设备，按 compatible
 *      匹配驱动并 probe（用 ① 的 regmap 自测），bind 后注册 /dev 节点。 → BIND_PASS
 *   ③ 经 /dev 节点回拿设备、用其 regmap 做真实串口 I/O。              → DEV_PASS
 * 全过 → ALL_PASS。 */
#include "kernel.h"
#include "regmap.h"
#include "dev.h"

unsigned long g_boot_hart = 0; /* probe 里 PLIC 需当前 hart 的 S-context */

void kmain(void) {
    /* SBI 经 a0 把当前 hartid 传进来（entry.S 未触碰 a0，首句即可取到）。
     * -smp 4 下启动 hart 不确定，PLIC 是 per-context 的，故按它配置自己的 S-context。 */
    register unsigned long a0 asm("a0");
    unsigned long hartid = a0;
    g_boot_hart = hartid;

    kputs("\n[S06d] typed register map + platform-bus lifecycle\n");
    kputs("boot hartid=");
    kputdec(hartid);
    console_putchar('\n');

    /* - ① 类型化寄存器图：NS16550 + PLIC - */
    struct ns16550_regs *u = ns16550_at(UART0_BASE);
    int uart_ok = ns16550_loopback(u);
    int ctx = PLIC_S_CTX(hartid);
    int plic_ok = plic_regmap_config(ctx, UART0_IRQ);
    if (uart_ok && plic_ok) {
        kputs("REGMAP_PASS\n");
    } else {
        if (!uart_ok) kputs("REGMAP_MISS ns16550-typed-loopback\n");
        if (!plic_ok) kputs("REGMAP_MISS plic-typed-config\n");
    }

    /* - ② driver table → probe → bind → /dev - */
    static struct device devices[8];
    struct dt_device dts[8];
    int n = 0;
    if (fdt_check_magic(device_dtb)) n = fdt_scan(device_dtb, dts, 8);
    kputs("dt nodes = ");
    kputdec((uint64_t)n);
    console_putchar('\n');

    int nd = 0, bound = 0;
    for (int i = 0; i < n && nd < 8; i++) {
        if (!dts[i].compatible) continue;
        struct driver *drv = driver_match(dts[i].compatible);
        if (!drv) continue; /* 无驱动匹配（如根节点） */

        struct device *dev = &devices[nd++];
        dev->name = dts[i].name;
        dev->compatible = dts[i].compatible;
        dev->base = dts[i].has_reg ? dts[i].reg_addr : 0;
        dev->drv = drv;
        dev->bound = 0;
        dev->devnode = 0;

        if (!drv->probe(dev)) {
            kputs("  probe not-ready: ");
            kputs(dev->compatible);
            console_putchar('\n');
            continue;
        }
        if (driver_bind(dev)) {
            bound++;
            kputs("  bind ok: ");
            kputs(dev->compatible);
            kputs(" -> ");
            kputs(dev->devnode);
            console_putchar('\n');
        }
    }

    struct device *ud = dev_lookup("/dev/ttyS0");
    int uart_bound = ud && ud->bound && ud->drv && ud->base == UART0_BASE;
    if (bound >= 1 && uart_bound) {
        kputs("BIND_PASS\n");
    } else {
        kputs("BIND_MISS bound=");
        kputdec((uint64_t)bound);
        console_putchar('\n');
    }

    /* - ③ 经 /dev 节点做真实 I/O - */
    int dev_ok = 0;
    if (ud && ud->base == UART0_BASE) {
        ns16550_emit(ns16550_at(ud->base),
                     "[/dev/ttyS0] hello via bound NS16550 regmap\n");
        dev_ok = 1;
    }
    if (dev_ok) {
        kputs("DEV_PASS\n");
    } else {
        kputs("DEV_MISS\n");
    }

    if (uart_ok && plic_ok && uart_bound && dev_ok) kputs("ALL_PASS\n");
}
