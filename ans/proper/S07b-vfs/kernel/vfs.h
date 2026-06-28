/* S07b · 内核 VFS（虚拟文件系统层）：共享声明。
 *
 * 思想：在「具体文件系统」之上加一层间接（indirection）。上层只认 vnode + vfs_ops
 * 这套抽象接口（vtable），不关心背后是 ramfs 还是 devfs。挂载表把不同路径子树绑到
 * 不同 FS，路径解析按「最长前缀」选中挂载点，再把相对路径交给该 FS 的 ops 处理。 */
#ifndef S7B_VFS_H
#define S7B_VFS_H
#include <stdint.h>

/* vnode 类型 */
#define VFREE 0u
#define VFILE 1u
#define VDIR  2u

struct vnode;

/* 文件系统操作表（vtable）：每个 FS 实现一份，所有调用经此分发。 */
struct vfs_ops {
    /* 在目录 dir 下按相对路径 name 解析出 vnode，写入 *out；命中返回 0，否则 -1。 */
    int (*lookup)(struct vnode *dir, const char *name, struct vnode *out);
    /* 读 vn 的内容到 buf（至多 max 字节），返回字节数或 -1。 */
    int (*read)(struct vnode *vn, void *buf, uint32_t max);
    /* 把 len 字节写入 vn，返回写入字节数或 -1。 */
    int (*write)(struct vnode *vn, const void *buf, uint32_t len);
    /* 列目录 vn 下的名字，最多 max 个，返回项数或 -1。 */
    int (*readdir)(struct vnode *vn, char names[][28], int max);
};

/* vnode：FS 无关的文件句柄。ops 指向所属 FS 的 vtable，priv 是该 FS 的私有数据
 * （ramfs：inode 号；devfs：设备号）。多态全靠 ops 这一层间接实现。 */
struct vnode {
    struct vfs_ops *ops;   /* 所属 FS 的操作表 */
    void           *priv;  /* FS 私有：inode 号 / 设备号 等 */
    uint32_t        type;  /* VFILE / VDIR */
};

/* 挂载项：把某个路径前缀（挂载点）绑定到一个 FS 实例。 */
struct mount {
    const char     *path;  /* 挂载点，如 "/" 或 "/dev" */
    struct vfs_ops *ops;    /* 该 FS 的 vtable */
    void           *fs;     /* 该 FS 根目录的私有数据 */
};

/* —— VFS 顶层 API —— */
void vfs_init(void);                                    /* 注册并挂载 ramfs + devfs */
int  vfs_mount(const char *path, struct vfs_ops *ops, void *fs);
int  vfs_nmounts(void);                                 /* 已挂载 FS 个数 */
const char *vfs_mount_path(int i);                      /* 第 i 个挂载点路径 */
int  vfs_open(const char *path, struct vnode *out);     /* 解析路径 → vnode */
int  vfs_read(struct vnode *vn, void *buf, uint32_t max);
int  vfs_write(struct vnode *vn, const void *buf, uint32_t len);

/* —— 两个具体 FS 的初始化 + vtable 访问器（harness 用其校验分发去向）—— */
struct vfs_ops *ramfs_ops(void);
void           *ramfs_root(void);   /* 根目录私有数据 */
void            ramfs_init(void);
struct vfs_ops *devfs_ops(void);
void           *devfs_root(void);
void            devfs_init(void);

/* —— freestanding 极简工具（vfs.c 提供）—— */
void    *kmemcpy(void *dst, const void *src, uint32_t n);
void    *kmemset(void *dst, int c, uint32_t n);
int      kstreq(const char *a, const char *b);   /* 以 0 结尾、相等返回 1 */
uint32_t kstrlen(const char *s);

#endif
