/* S06d · 平台总线：驱动表 + compatible 匹配 + probe + bind + /dev 注册（学生填空）。
 * 补齐 S06 缺的后半生命周期：S06 到 probe 为止，这里继续 bind（driver↔device）并注册 /dev。
 * 驱动表 / probe / driver_match / dev_register / dev_lookup 已给，你只填 driver_bind。 */
#include "regmap.h"
#include "dev.h"

extern unsigned long g_boot_hart; /* main.c：当前 hartid，PLIC 取 S-context 用 */

static int k_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
static int k_streq(const char *a, const char *b) { return k_strcmp(a, b) == 0; }

/* - probe：用类型化 regmap 自测设备真在那（已给） - */
static int ns16550_probe(struct device *dev) {
    if (!dev->base) return 0;
    return ns16550_loopback(ns16550_at(dev->base)) ? 1 : 0;
}
static int plic_probe(struct device *dev) {
    if (dev->base != PLIC_BASE) return 0;
    return plic_regmap_config(PLIC_S_CTX(g_boot_hart), UART0_IRQ) ? 1 : 0;
}

/* - 驱动表：设备(compatible) 与驱动(probe) 经字符串解耦匹配（已给） - */
static struct driver driver_table[] = {
    { "ns16550a",          ns16550_probe, "tty"  },
    { "sifive,plic-1.0.0", plic_probe,    "intc" },
};
#define DRIVER_COUNT (int)(sizeof(driver_table) / sizeof(driver_table[0]))

struct driver *driver_match(const char *compatible) {
    if (!compatible) return 0;
    for (int i = 0; i < DRIVER_COUNT; i++) {
        if (k_streq(driver_table[i].compatible, compatible)) return &driver_table[i];
    }
    return 0;
}

/* - /dev 注册表（已给） - */
#define MAX_DEV 8
static struct { const char *path; struct device *dev; } dev_table[MAX_DEV];
static int dev_count = 0;

int dev_register(const char *path, struct device *dev) {
    if (dev_count >= MAX_DEV) return -1;
    dev->devnode = path;
    dev_table[dev_count].path = path;
    dev_table[dev_count].dev = dev;
    return dev_count++;
}

struct device *dev_lookup(const char *path) {
    for (int i = 0; i < dev_count; i++) {
        if (k_streq(dev_table[i].path, path)) return dev_table[i].dev;
    }
    return 0;
}

/* 设备类别 → /dev 节点名（每类一个实例，够本实验用；已给）。 */
static const char *class_to_node(const char *cls) {
    if (k_streq(cls, "tty"))  return "/dev/ttyS0";
    if (k_streq(cls, "intc")) return "/dev/intc0";
    return "/dev/dev0";
}

/* TODO(3): bind - 把已匹配的 driver 绑到 device，并在 /dev 注册一个节点。
 *   步骤：
 *     · dev 或 dev->drv 为空 → 返回 0；
 *     · 置 dev->bound = 1；
 *     · 用 class_to_node(dev->drv->class) 得到 /dev 路径，调 dev_register(path, dev)；
 *       注册成功（返回值 >= 0）则返回 1，否则 0。
 *   现在的占位恒返回 0：设备永不 bind、/dev 空 → BIND_MISS / DEV_MISS。 */
int driver_bind(struct device *dev) {
    (void)dev;
    (void)class_to_node;
    return 0; /* TODO: 置 bound + dev_register，绑定并注册 /dev 节点 */
}
