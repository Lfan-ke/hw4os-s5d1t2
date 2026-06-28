/* S07b · VFS 核心：挂载表 + 路径解析 + vtable 分发（参考解）。
 *
 * 两个学生填空点（见 README）：
 *   A. vfs_open / vfs_read 经 vnode->ops 把调用分发到具体 FS（vtable 多态）。
 *   B. mount_resolve 最长前缀匹配：在所有「挂载点是 path 前缀」的挂载里选最长的，
 *      并算出相对子路径。这是「跨挂载点」路径解析的关键。 */
#include "vfs.h"

/* —— freestanding 极简工具 —— */
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *kmemcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = dst; const uint8_t *s = src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *kmemset(void *dst, int c, uint32_t n) {
    uint8_t *d = dst;
    for (uint32_t i = 0; i < n; i++) d[i] = (uint8_t)c;
    return dst;
}
int kstreq(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *a == *b;
}
uint32_t kstrlen(const char *s) { uint32_t n = 0; while (s[n]) n++; return n; }

/* mp 是否为 path 的「路径前缀」：mp=="/" 总成立；否则要求 path 以 mp 开头，
 * 且 mp 之后紧跟 '/' 或字符串结束（避免 "/dev" 误配 "/device"）。 */
static int path_is_prefix(const char *path, const char *mp) {
    if (mp[0] == '/' && mp[1] == 0) return 1;          /* 根挂载点匹配一切 */
    uint32_t i = 0;
    while (mp[i]) { if (path[i] != mp[i]) return 0; i++; }
    return path[i] == 0 || path[i] == '/';
}

/* —— 挂载表 —— */
#define MAXMNT 8
static struct mount g_mnt[MAXMNT];
static int g_nmnt;

int vfs_mount(const char *path, struct vfs_ops *ops, void *fs) {
    if (g_nmnt >= MAXMNT) return -1;
    g_mnt[g_nmnt].path = path;
    g_mnt[g_nmnt].ops  = ops;
    g_mnt[g_nmnt].fs   = fs;
    g_nmnt++;
    return 0;
}
int vfs_nmounts(void) { return g_nmnt; }
const char *vfs_mount_path(int i) {
    return (i >= 0 && i < g_nmnt) ? g_mnt[i].path : "";
}

/* =========================================================================
 * 学生填空点 B：最长前缀匹配的路径解析。
 * 在所有「挂载点 path 是入参 path 前缀」的挂载里挑挂载点最长的那个（"/dev" 胜过 "/"），
 * 把相对子路径（去掉挂载点前缀与多余 '/'）写入 *rel，返回选中的 mount；无则返回 0。
 * ========================================================================= */
static struct mount *mount_resolve(const char *path, const char **rel) {
    struct mount *best = 0;
    uint32_t bestlen = 0;
    /* TODO(填空 B)：遍历 g_mnt[0..g_nmnt)，用 path_is_prefix(path, g_mnt[i].path) 筛候选，
     * 在候选里挑挂载点字符串最长（kstrlen 最大）的写入 best/bestlen。选中后令
     * *rel 指向相对子路径：path + bestlen，再把开头连续的 '/' 吃掉。无候选返回 0。
     * 占位：直接返回 0（解析失败）——vfs_open 随之失败，不出 PASS，但不崩。 */
    (void)path; (void)best; (void)bestlen; (void)rel; (void)path_is_prefix;
    return 0;
}

/* —— 打开路径：选挂载点 → 在该 FS 根目录上分发 lookup —— */
int vfs_open(const char *path, struct vnode *out) {
    const char *rel = path;
    struct mount *m = mount_resolve(path, &rel);   /* 填空 B 在此被调用 */
    if (!m) return -1;
    struct vnode root;
    root.ops  = m->ops;
    root.priv = m->fs;
    root.type = VDIR;
    /* === 学生填空 A-1：经 vtable 把 lookup 分发到该 FS ===
     * TODO: 返回 m->ops->lookup(&root, rel, out)，即经 vnode 的操作表分发到具体 FS。 */
    (void)root; (void)rel; (void)out;
    return -1;   /* 占位：未分发 → open 失败 → 不出 PASS（不崩） */
}

/* —— 读：分发到 vnode 所属 FS 的 read —— */
int vfs_read(struct vnode *vn, void *buf, uint32_t max) {
    /* === 学生填空 A-2：经 vtable 把 read 分发到该 FS ===
     * TODO: 返回 vn->ops->read(vn, buf, max)。 */
    (void)vn; (void)buf; (void)max;
    return -1;   /* 占位：未分发 → read 失败 → 不出 PASS（不崩） */
}

/* —— 写：分发到 vnode 所属 FS 的 write（给定，已实现）—— */
int vfs_write(struct vnode *vn, const void *buf, uint32_t len) {
    if (!vn->ops->write) return -1;
    return vn->ops->write(vn, buf, len);
}

/* —— 注册并挂载两个文件系统 —— */
void vfs_init(void) {
    ramfs_init();
    devfs_init();
    vfs_mount("/",    ramfs_ops(), ramfs_root());  /* 根：内存 inode FS */
    vfs_mount("/dev", devfs_ops(), devfs_root());  /* /dev：设备 FS */
}
