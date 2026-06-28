/* S07b · ramfs：内存 inode/目录文件系统（复用 S07 思路，给定）。
 *
 * 简化自 S07：不走块设备，直接用一组内存 inode（含内联数据）当一棵扁平根目录。
 * 每个 inode = 名字 + 类型 + 一段数据。ramfs 实现 vfs_ops 四个方法，挂在 "/"。
 * vnode.priv 存 inode 下标。 */
#include "vfs.h"

#define RAMFS_NINODE 8
#define RAMFS_DATA   256

struct rinode {
    uint32_t type;              /* VFREE / VFILE */
    char     name[28];
    uint8_t  data[RAMFS_DATA];
    uint32_t size;
};
static struct rinode g_ino[RAMFS_NINODE];

/* 建一个文件（内核内预置内容用）。返回 inode 下标或 -1。 */
static int ramfs_create(const char *name, const char *body) {
    for (int i = 0; i < RAMFS_NINODE; i++) {
        if (g_ino[i].type != VFREE) continue;
        kmemset(&g_ino[i], 0, sizeof(g_ino[i]));
        g_ino[i].type = VFILE;
        uint32_t k = 0;
        while (name[k] && k < 27) { g_ino[i].name[k] = name[k]; k++; }
        g_ino[i].name[k] = 0;
        uint32_t n = 0;
        while (body[n] && n < RAMFS_DATA) { g_ino[i].data[n] = (uint8_t)body[n]; n++; }
        g_ino[i].size = n;
        return i;
    }
    return -1;
}

/* —— vfs_ops 实现 —— */

/* lookup：在扁平根目录里按名字找文件，命中则填出 vnode。 */
static int ramfs_lookup(struct vnode *dir, const char *name, struct vnode *out) {
    (void)dir;
    for (int i = 0; i < RAMFS_NINODE; i++) {
        if (g_ino[i].type == VFILE && kstreq(g_ino[i].name, name)) {
            out->ops  = ramfs_ops();
            out->priv = (void *)(uintptr_t)i;   /* 私有 = inode 下标 */
            out->type = VFILE;
            return 0;
        }
    }
    return -1;
}

static int ramfs_read(struct vnode *vn, void *buf, uint32_t max) {
    int i = (int)(uintptr_t)vn->priv;
    if (i < 0 || i >= RAMFS_NINODE || g_ino[i].type != VFILE) return -1;
    uint32_t n = g_ino[i].size;
    if (n > max) n = max;
    kmemcpy(buf, g_ino[i].data, n);
    return (int)n;
}

static int ramfs_write(struct vnode *vn, const void *buf, uint32_t len) {
    int i = (int)(uintptr_t)vn->priv;
    if (i < 0 || i >= RAMFS_NINODE || g_ino[i].type != VFILE) return -1;
    if (len > RAMFS_DATA) len = RAMFS_DATA;
    kmemcpy(g_ino[i].data, buf, len);
    g_ino[i].size = len;
    return (int)len;
}

static int ramfs_readdir(struct vnode *vn, char names[][28], int max) {
    (void)vn;
    int n = 0;
    for (int i = 0; i < RAMFS_NINODE && n < max; i++) {
        if (g_ino[i].type != VFILE) continue;
        kmemcpy(names[n], g_ino[i].name, 28);
        n++;
    }
    return n;
}

static struct vfs_ops g_ramfs_ops = {
    .lookup  = ramfs_lookup,
    .read    = ramfs_read,
    .write   = ramfs_write,
    .readdir = ramfs_readdir,
};

struct vfs_ops *ramfs_ops(void)  { return &g_ramfs_ops; }
void           *ramfs_root(void) { return (void *)0; }   /* 根目录无额外私有数据 */

void ramfs_init(void) {
    for (int i = 0; i < RAMFS_NINODE; i++) g_ino[i].type = VFREE;
    ramfs_create("hello.txt", "hello from ramfs");
    ramfs_create("readme",    "vfs over a flat ram inode store");
    ramfs_create("data.bin",  "0123456789-ramfs-content-end");
}
