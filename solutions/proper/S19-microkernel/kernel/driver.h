/* S19 · 可插拔驱动注册（驱动系统收尾）。
 * 一张驱动注册表：每个驱动用名字注册自己的 read 操作，内核按名字查找并派发。
 * 这就是“驱动模型”的最小骨架——新增设备只需注册，内核分发逻辑不改（开闭原则）。 */
#ifndef S19_DRIVER_H
#define S19_DRIVER_H
#include <stdint.h>

#define DRV_MAX     8
#define DRV_NAMELEN 12

/* 驱动的读寄存器操作：给寄存器号，返回读到的值（这里用纯函数建模真实寄存器读）。 */
typedef uint32_t (*drv_read_fn)(uint32_t reg);

struct Driver {
    int         used;
    char        name[DRV_NAMELEN];
    drv_read_fn read;
};

/* 清空注册表。 */
void driver_sys_reset(void);

/* 注册一个驱动：占第一个空槽存 (name, fn)，返回槽号(>=0)；表满返回 -1。 */
int  driver_register(const char *name, drv_read_fn fn);

/* 按名字查找已注册驱动：命中返回指针，未注册返回 0(NULL)。 */
struct Driver *driver_find(const char *name);

#endif
