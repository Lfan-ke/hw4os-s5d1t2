/* S07b · devfs：设备文件系统（给定），挂在 "/dev"。
 *   /dev/zero —— 读出全 0（无穷零源）
 *   /dev/null —— 吞掉一切写入；读立即 EOF
 * vnode.priv 存设备号。devfs 同样实现一份 vfs_ops。 */
#include "vfs.h"

#define DEV_ZERO 0
#define DEV_NULL 1

/* lookup：把名字映射到设备号。 */
static int devfs_lookup(struct vnode *dir, const char *name, struct vnode *out) {
    (void)dir;
    int dev;
    if (kstreq(name, "zero"))      dev = DEV_ZERO;
    else if (kstreq(name, "null")) dev = DEV_NULL;
    else return -1;
    out->ops  = devfs_ops();
    out->priv = (void *)(uintptr_t)dev;
    out->type = VFILE;
    return 0;
}

static int devfs_read(struct vnode *vn, void *buf, uint32_t max) {
    int dev = (int)(uintptr_t)vn->priv;
    if (dev == DEV_ZERO) { kmemset(buf, 0, max); return (int)max; }  /* 全 0 */
    if (dev == DEV_NULL) { return 0; }                               /* EOF */
    return -1;
}

static int devfs_write(struct vnode *vn, const void *buf, uint32_t len) {
    int dev = (int)(uintptr_t)vn->priv;
    (void)buf;
    if (dev == DEV_NULL) return (int)len;   /* 吞掉，假装全写成功 */
    if (dev == DEV_ZERO) return (int)len;   /* zero 也容忍写（丢弃） */
    return -1;
}

static int devfs_readdir(struct vnode *vn, char names[][28], int max) {
    (void)vn;
    static const char *devs[2] = { "zero", "null" };
    int n = 0;
    for (int i = 0; i < 2 && n < max; i++) {
        kmemset(names[n], 0, 28);
        uint32_t k = 0; while (devs[i][k]) { names[n][k] = devs[i][k]; k++; }
        n++;
    }
    return n;
}

static struct vfs_ops g_devfs_ops = {
    .lookup  = devfs_lookup,
    .read    = devfs_read,
    .write   = devfs_write,
    .readdir = devfs_readdir,
};

struct vfs_ops *devfs_ops(void)  { return &g_devfs_ops; }
void           *devfs_root(void) { return (void *)0; }
void            devfs_init(void) { /* 设备表是静态的，无需初始化 */ }
