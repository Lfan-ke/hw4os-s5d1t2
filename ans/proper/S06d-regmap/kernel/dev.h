/* S06d · 平台总线生命周期接口（给定）：device / driver / 驱动表 / /dev 注册表 + FDT。
 * 补齐 S06 缺的 driver table → probe → bind → /dev 一条龙：设备与驱动经 compatible 解耦匹配，
 * probe 用类型化 regmap 自测，bind 把 driver 绑到 device 并在 /dev 注册一个节点。 */
#ifndef S06D_DEV_H
#define S06D_DEV_H
#include <stdint.h>

struct device;

/* 驱动：compatible 选择子 + probe + 设备类别（决定 /dev 命名前缀）。 */
struct driver {
    const char *compatible;
    int       (*probe)(struct device *dev); /* 1=设备就绪 */
    const char *class;                       /* "tty"/"intc"…→ /dev 命名 */
};

/* 设备：由设备树节点实例化；bind 后 drv/bound/devnode 被填上。 */
struct device {
    const char   *name;       /* 设备树节点名，如 "uart@10000000" */
    const char   *compatible; /* compatible 首串，如 "ns16550a" */
    uint64_t      base;       /* MMIO 基址（reg 首地址），无则 0 */
    struct driver *drv;       /* 命中的驱动，bind 前为 0 */
    int           bound;      /* 1=已 bind */
    const char   *devnode;    /* bind 时分配的 /dev 路径，如 "/dev/ttyS0" */
};

/* - FDT/设备树解析（fdt.c；复用 S06，已给） - */
struct dt_device {
    const char *name;
    const char *compatible;
    uint64_t    reg_addr;
    int         has_reg;
};
int fdt_check_magic(const uint8_t *dtb);
int fdt_scan(const uint8_t *dtb, struct dt_device *out, int max);
extern unsigned char device_dtb[];
extern unsigned char device_dtb_end[];

/* - 平台总线（driver.c） - */
struct driver *driver_match(const char *compatible);       /* 按 compatible 选驱动，无则 0 */
int            driver_bind(struct device *dev);            /* 学生填：绑定 + /dev 注册，1=成 */
int            dev_register(const char *path, struct device *dev); /* 返回索引 / -1（满） */
struct device *dev_lookup(const char *path);               /* /dev 路径 → 设备，无则 0 */

#endif
