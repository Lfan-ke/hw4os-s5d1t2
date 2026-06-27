/* S19 · 可插拔驱动注册表（参考解）。
 * driver_register 把驱动按名字登记进表；driver_find 按名字派发。
 * 学生需实现的：driver_register（找空槽 + 存名字与函数指针）。
 * 注册表查找、字符串比较、收尾测试均给定。 */
#include "kernel.h"
#include "driver.h"

static struct Driver g_drv[DRV_MAX];

/* —— 极简字符串助手（freestanding，无 libc）—— */
static int s_eq(const char *a, const char *b) {
    int i;
    for (i = 0; i < DRV_NAMELEN; i++) {
        if (a[i] != b[i]) return 0;
        if (a[i] == '\0')  return 1;
    }
    return 1;
}
static void s_cpy(char *dst, const char *src) {
    int i;
    for (i = 0; i < DRV_NAMELEN - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

void driver_sys_reset(void) {
    int i;
    for (i = 0; i < DRV_MAX; i++) {
        g_drv[i].used = 0;
        g_drv[i].read = 0;
    }
}

/* ================= 学生实现：驱动注册 ================= */
int driver_register(const char *name, drv_read_fn fn) {
    int i;
    for (i = 0; i < DRV_MAX; i++) {
        if (!g_drv[i].used) {
            g_drv[i].used = 1;
            s_cpy(g_drv[i].name, name);   /* 登记名字 */
            g_drv[i].read = fn;           /* 登记操作 */
            return i;                     /* 返回槽号 */
        }
    }
    return -1;                            /* 表满 */
}

/* ================= 以下给定：查找 + 收尾测试 ================= */
struct Driver *driver_find(const char *name) {
    int i;
    for (i = 0; i < DRV_MAX; i++)
        if (g_drv[i].used && s_eq(g_drv[i].name, name))
            return &g_drv[i];
    return 0;
}

/* 三个示例驱动的 read 实现（纯函数建模设备寄存器读）。 */
static uint32_t uart_read(uint32_t r) { return r * 2u + 1u; }
static uint32_t rtc_read (uint32_t r) { return r + 1000u; }
static uint32_t blk_read (uint32_t r) { return r ^ 0xABCDu; }

/* 收尾测试：注册三个驱动 → 按名字派发并核对 → 未知名查不到 → 表满返回 -1。 */
int run_driver_test(void) {
    struct Driver *d;
    int ok = 1;
    int id0, id1, id2;

    driver_sys_reset();

    id0 = driver_register("uart", uart_read);
    id1 = driver_register("rtc",  rtc_read);
    id2 = driver_register("blk",  blk_read);
    if (id0 < 0 || id1 < 0 || id2 < 0) return 0;

    /* 按名字派发：内核分发逻辑对所有驱动一视同仁。 */
    d = driver_find("rtc");
    if (!d || d->read(7) != rtc_read(7)) ok = 0;
    d = driver_find("uart");
    if (!d || d->read(5) != uart_read(5)) ok = 0;
    d = driver_find("blk");
    if (!d || d->read(3) != blk_read(3)) ok = 0;

    /* 未注册的设备查不到。 */
    if (driver_find("nope") != 0) ok = 0;

    if (ok)
        kputs("[drivers] registered uart/rtc/blk; name-based dispatch OK\n");
    return ok;
}
