/* S07b · 内核入口/测试驱动（给定，勿改）。
 * 四步自检：注册/挂载两个 FS → vtable 分发 → 跨挂载点路径解析 → devfs 语义，
 * 全过打印 ALL_PASS。失败打印 *_MISS 诊断（不含 FAIL/panic）。 */
#include "kernel.h"
#include "vfs.h"

static int mem_eq(const void *a, const void *b, uint32_t n) {
    const uint8_t *x = a, *y = b;
    for (uint32_t i = 0; i < n; i++) if (x[i] != y[i]) return 0;
    return 1;
}

/* —— 1：注册并挂载了两个 FS（"/" ramfs 与 "/dev" devfs）—— */
static int test_reg(void) {
    if (vfs_nmounts() != 2) return 0;
    int has_root = 0, has_dev = 0;
    for (int i = 0; i < vfs_nmounts(); i++) {
        if (kstreq(vfs_mount_path(i), "/"))    has_root = 1;
        if (kstreq(vfs_mount_path(i), "/dev")) has_dev = 1;
    }
    return has_root && has_dev;
}

/* —— 2：open/read 经 vtable 分发到正确 FS —— */
static int test_dispatch(void) {
    struct vnode vn;
    if (vfs_open("/hello.txt", &vn) != 0) return 0;
    if (vn.ops != ramfs_ops()) return 0;          /* 必须分发到 ramfs */
    char buf[64];
    int n = vfs_read(&vn, buf, sizeof(buf));
    if (n != 16) return 0;
    return mem_eq(buf, "hello from ramfs", 16);
}

/* —— 3：路径解析跨过挂载点（最长前缀匹配）—— */
static int test_crossmnt(void) {
    struct vnode d;
    if (vfs_open("/dev/zero", &d) != 0) return 0;
    if (d.ops != devfs_ops()) return 0;           /* "/dev" 胜过 "/" */
    struct vnode r;
    if (vfs_open("/readme", &r) != 0) return 0;
    if (r.ops != ramfs_ops()) return 0;           /* 同时根路径仍归 ramfs */
    return 1;
}

/* —— 4：devfs 语义：/dev/zero 读全 0、/dev/null 吞写 —— */
static int test_devfs(void) {
    struct vnode z;
    if (vfs_open("/dev/zero", &z) != 0) return 0;
    char buf[32];
    for (int i = 0; i < 32; i++) buf[i] = (char)0xAA;
    if (vfs_read(&z, buf, 32) != 32) return 0;
    for (int i = 0; i < 32; i++) if (buf[i] != 0) return 0;   /* 全 0 */

    struct vnode nul;
    if (vfs_open("/dev/null", &nul) != 0) return 0;
    if (vfs_write(&nul, "discard me", 10) != 10) return 0;     /* 吞写 */
    char rb[8];
    if (vfs_read(&nul, rb, 8) != 0) return 0;                  /* 读 EOF */
    return 1;
}

void kmain(void) {
    kputs("\n[S07b] kernel VFS: vtable dispatch over ramfs + devfs\n");
    vfs_init();

    int r = test_reg();
    if (r) kputs("VFS_REG_PASS\n"); else kputs("VFS_REG_MISS\n");

    int d = test_dispatch();
    if (d) kputs("DISPATCH_PASS\n"); else kputs("DISPATCH_MISS\n");

    int c = test_crossmnt();
    if (c) kputs("CROSSMNT_PASS\n"); else kputs("CROSSMNT_MISS\n");

    int v = test_devfs();
    if (v) kputs("DEVFS_PASS\n"); else kputs("DEVFS_MISS\n");

    if (r && d && c && v) kputs("ALL_PASS\n");
}
