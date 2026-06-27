/* VFS（虚拟文件系统）—— C。
 *
 * 母题：VFS = 让多种文件系统「共存于同一组接口」之下的抽象层。
 * （Sun 1986 为接入 NFS 发明：ext/nfs/proc 在一台机器上都长成同一张 read/write 脸。）
 *
 * VFS 四大对象，本课的极简对应：
 *   superblock = 一个挂好的 FS 实例（这里是 struct Fs，带一张函数指针表）
 *   inode      = FS 内的一个节点（用一个 int 节点号代表）
 *   dentry     = 「名字 → 节点」解析（各 FS 自己的 lookup）
 *   file       = 打开后的句柄（OpenFile{挂载下标, 节点号}）
 *
 * 两个 mock 文件系统，背后行为不同、对外接口一致：
 *   ① RamFs —— 内存里几个真文件（有存储，写得进读得出）。
 *   ② DevFs —— 虚拟设备：/null 吞写、/zero 读出全 0（无存储，纯副作用）。
 * 函数指针表 = xv6 的 struct devsw / Linux 的 struct file_operations。
 *
 * 你只实现两处（标 TODO）：
 *   (1) vfs_resolve —— 按路径找「最长挂载前缀」路由到对应 FS；
 *   (2) ramfs_lookup —— 在文件表里把名字解析成节点号。
 * 下方测试 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define ZERO_READ_LEN 8 /* /dev/zero 一次读出的零字节数 */
#define MAX_FILES 4
#define MAX_MOUNTS 4

/* ── 统一接口：每个 FS 一张函数指针表 + 自带状态 ── */
typedef struct {
    char name[16];
    uint8_t data[32];
    int len;
} RamFile;

typedef struct Fs Fs;
struct Fs {
    const char *name;
    int (*lookup)(Fs *self, const char *rel);                 /* 名字→节点号，-1=没有 */
    int (*read)(Fs *self, int node, uint8_t *buf, int cap);   /* 读出，返回字节数 */
    int (*write)(Fs *self, int node, const uint8_t *d, int n);/* 写入，返回写了几字节 */
    int (*list)(Fs *self, const char **out, int cap);         /* readdir：填名字，返回个数 */
    /* RamFs 状态（DevFs 不用）： */
    RamFile files[MAX_FILES];
    int nfiles;
};

/* ── FS 实例 ①：RamFs（内存里几个真文件，有存储）── */
static int ramfs_lookup(Fs *self, const char *rel) {
    /* TODO(2)：名字解析（dentry→inode）。顺序扫 self->files[0..nfiles)，
     *   找到 strcmp(files[i].name, rel)==0 的下标 i 返回当节点号；扫完没有返回 -1。
     * HINT: for (int i = 0; i < self->nfiles; i++)
     *           if (strcmp(self->files[i].name, rel) == 0) return i;
     *       return -1; */
    (void)self;
    (void)rel;
    return -1; /* ← 占位（永远找不到，VFS_PASS 跑不出来） */
}
static int ramfs_read(Fs *self, int node, uint8_t *buf, int cap) {
    int n = self->files[node].len;
    if (n > cap) n = cap;
    memcpy(buf, self->files[node].data, (size_t)n);
    return n;
}
static int ramfs_write(Fs *self, int node, const uint8_t *d, int n) {
    /* 真存储：存下来，下次 read 读得出。 */
    if (n > (int)sizeof(self->files[node].data)) n = (int)sizeof(self->files[node].data);
    memcpy(self->files[node].data, d, (size_t)n);
    self->files[node].len = n;
    return n;
}
static int ramfs_list(Fs *self, const char **out, int cap) {
    int n = self->nfiles < cap ? self->nfiles : cap;
    for (int i = 0; i < n; i++) out[i] = self->files[i].name;
    return n;
}

/* ── FS 实例 ②：DevFs（虚拟设备，无存储）。节点 0=/null，1=/zero ── */
static int devfs_lookup(Fs *self, const char *rel) {
    (void)self;
    if (strcmp(rel, "/null") == 0) return 0;
    if (strcmp(rel, "/zero") == 0) return 1;
    return -1;
}
static int devfs_read(Fs *self, int node, uint8_t *buf, int cap) {
    (void)self;
    if (node == 1) { /* /dev/zero：读出一串 0 */
        int n = ZERO_READ_LEN < cap ? ZERO_READ_LEN : cap;
        memset(buf, 0, (size_t)n);
        return n;
    }
    return 0; /* /dev/null：读出空 */
}
static int devfs_write(Fs *self, int node, const uint8_t *d, int n) {
    (void)self; (void)node; (void)d;
    return n; /* 吞写：报告写了 n 字节但不存储 */
}
static int devfs_list(Fs *self, const char **out, int cap) {
    (void)self;
    static const char *names[2] = {"/null", "/zero"};
    int n = 2 < cap ? 2 : cap;
    for (int i = 0; i < n; i++) out[i] = names[i];
    return n;
}

/* ════════════════════════════════════════════════════════════════
 * 已给的路径工具（勿改）
 * ════════════════════════════════════════════════════════════════ */

/* path 是否落在挂载点 mount 之下？带边界检查，避免 "/dev" 误配 "/device"。 */
static int is_under(const char *mount, const char *path) {
    if (strcmp(mount, "/") == 0) return path[0] == '/';
    size_t ml = strlen(mount);
    if (strncmp(path, mount, ml) != 0) return 0;
    return path[ml] == '\0' || path[ml] == '/';
}
/* 把挂载点从 path 上剥掉，得到 FS 内相对路径，写入 out。
 * 根挂载("/")返回整条路径；其余剥掉前缀（"/dev/zero" 挂 "/dev" → "/zero"）。 */
static void subpath(const char *mount, const char *path, char *out) {
    if (strcmp(mount, "/") == 0) { strcpy(out, path); return; }
    const char *rest = path + strlen(mount);
    if (rest[0] == '\0') strcpy(out, "/");
    else strcpy(out, rest);
}

/* ════════════════════════════════════════════════════════════════
 * VFS 层：挂载表 + 路径路由 + 统一 open/read/write
 * ════════════════════════════════════════════════════════════════ */
typedef struct {
    const char *mount;
    Fs *fs;
} Mount;
typedef struct {
    Mount mounts[MAX_MOUNTS];
    int n;
} Vfs;

static void vfs_mount(Vfs *v, const char *at, Fs *fs) {
    v->mounts[v->n].mount = at;
    v->mounts[v->n].fs = fs;
    v->n++;
}

/* 【学生填空 (1)】路径路由：在挂载表里找**最长**匹配的挂载前缀，
 * 返回挂载下标并把 FS 内相对路径写入 rel_out；一个都不匹配返回 -1。
 *
 * 为什么要「最长」：路径 "/dev/zero" 同时落在 "/"(长 1) 和 "/dev"(长 4) 之下，
 * 必须选更长的 "/dev"，才能跨过挂载点进入 DevFs，而不是停在根的 RamFs。 */
static int vfs_resolve(Vfs *v, const char *path, char *rel_out) {
    /* TODO(1)：遍历 v->mounts[0..n)，用已给的 is_under(挂载点, path) 判前缀；
     *   在所有命中的挂载里挑**挂载点字符串最长**的那个 best。
     *   都没命中返回 -1；命中则用 subpath(挂载点, path, rel_out) 剥出相对路径，返回 best。
     * HINT:
     *   int best = -1; size_t best_len = 0;
     *   for (int i = 0; i < v->n; i++) {
     *       size_t ml = strlen(v->mounts[i].mount);
     *       if (is_under(v->mounts[i].mount, path) && (best < 0 || ml > best_len)) {
     *           best = i; best_len = ml;
     *       }
     *   }
     *   if (best < 0) return -1;
     *   subpath(v->mounts[best].mount, path, rel_out);
     *   return best; */
    (void)v;
    (void)path;
    (void)rel_out;
    (void)is_under; /* 占位期先按住「已给工具未用」的告警；实现后会真正调用 */
    (void)subpath;
    return -1; /* ← 占位（永远路由失败，MOUNT/DISPATCH 跑不出来） */
}

/* ── 统一接口：调用者只认 path，不关心背后是哪种 FS ── */
typedef struct {
    int mi;   /* 挂载下标，-1=打开失败 */
    int node; /* 该 FS 内节点号 */
} OpenFile;

static OpenFile vfs_open(Vfs *v, const char *path) {
    OpenFile of = {-1, -1};
    char rel[128];
    int mi = vfs_resolve(v, path, rel);
    if (mi < 0) return of;
    int node = v->mounts[mi].fs->lookup(v->mounts[mi].fs, rel);
    if (node < 0) return of;
    of.mi = mi;
    of.node = node;
    return of;
}
static int vfs_read(Vfs *v, OpenFile of, uint8_t *buf, int cap) {
    Fs *fs = v->mounts[of.mi].fs;
    return fs->read(fs, of.node, buf, cap);
}
static int vfs_write(Vfs *v, OpenFile of, const uint8_t *d, int n) {
    Fs *fs = v->mounts[of.mi].fs;
    return fs->write(fs, of.node, d, n);
}
/* 某路径会被路由到哪个 FS 的名字（用于证明路由正确），失败返回 NULL。 */
static const char *vfs_fsname(Vfs *v, const char *path) {
    char rel[128];
    int mi = vfs_resolve(v, path, rel);
    if (mi < 0) return NULL;
    return v->mounts[mi].fs->name;
}

/* ════════════════════════════════════════════════════════════════
 * 测试 harness（给定，勿改）
 * ════════════════════════════════════════════════════════════════ */

/* 两个 FS 实例 + 一个 VFS，做成文件级静态量供各 check 共享。 */
static Fs g_ramfs = {
    "ramfs", ramfs_lookup, ramfs_read, ramfs_write, ramfs_list,
    {{"/hello", "hi, vfs\n", 8}, {"/motd", "welcome\n", 8}, {{0}, {0}, 0}, {{0}, {0}, 0}},
    2};
static Fs g_devfs = {
    "devfs", devfs_lookup, devfs_read, devfs_write, devfs_list,
    {{{0}, {0}, 0}, {{0}, {0}, 0}, {{0}, {0}, 0}, {{0}, {0}, 0}}, 0};
static Vfs g_vfs;

static void build_vfs(void) {
    g_vfs.n = 0;
    vfs_mount(&g_vfs, "/", &g_ramfs);    /* RamFs 挂根 */
    vfs_mount(&g_vfs, "/dev", &g_devfs); /* DevFs 挂 /dev */
}

static void dump(void) {
    const char *names[8];
    for (int i = 0; i < g_vfs.n; i++) {
        Fs *fs = g_vfs.mounts[i].fs;
        int k = fs->list(fs, names, 8);
        printf("[vfs] %s -> %s(", g_vfs.mounts[i].mount, fs->name);
        for (int j = 0; j < k; j++) printf("%s%s", j ? ", " : "", names[j]);
        printf(")\n");
    }
}

static int check_vfs(void) {
    int ok = 1;
    OpenFile of = vfs_open(&g_vfs, "/hello");
    if (of.mi < 0) {
        printf("VFS_FAIL open(\"/hello\") 失败（resolve 或 ramfs_lookup 没实现？）\n");
        ok = 0;
    } else {
        uint8_t buf[32];
        int n = vfs_read(&g_vfs, of, buf, sizeof(buf));
        if (n != 8 || memcmp(buf, "hi, vfs\n", 8) != 0) {
            printf("VFS_FAIL /hello 内容不符\n");
            ok = 0;
        }
    }
    if (ok) printf("VFS_PASS\n");
    return ok;
}

static int check_mount(void) {
    int ok = 1;
    char rel[128];

    int mi = vfs_resolve(&g_vfs, "/hello", rel);
    if (mi < 0 || strcmp(g_vfs.mounts[mi].fs->name, "ramfs") != 0 || strcmp(rel, "/hello") != 0) {
        printf("MOUNT_BAD /hello 应路由 ramfs rel=/hello\n");
        ok = 0;
    }

    mi = vfs_resolve(&g_vfs, "/dev/zero", rel);
    if (mi < 0 || strcmp(g_vfs.mounts[mi].fs->name, "devfs") != 0) {
        printf("MOUNT_FAIL /dev/zero 没跨过挂载点进入 devfs（最长前缀没选对？）\n");
        ok = 0;
    } else if (strcmp(rel, "/zero") != 0) {
        printf("MOUNT_BAD /dev/zero 在 devfs 内相对路径应=/zero，得 %s\n", rel);
        ok = 0;
    }

    if (ok) printf("MOUNT_PASS\n");
    return ok;
}

static int check_devfs(void) {
    int ok = 1;
    uint8_t buf[32];

    /* /dev/null：写被吞（报告写 5），读出为空。 */
    OpenFile null = vfs_open(&g_vfs, "/dev/null");
    if (null.mi < 0) {
        printf("DEVFS_FAIL open(\"/dev/null\") 失败\n");
        ok = 0;
    } else {
        int w = vfs_write(&g_vfs, null, (const uint8_t *)"hello", 5);
        if (w != 5) { printf("DEVFS_BAD write(/dev/null) 应报告写 5，得 %d\n", w); ok = 0; }
        int r = vfs_read(&g_vfs, null, buf, sizeof(buf));
        if (r != 0) { printf("DEVFS_FAIL /dev/null 应读出空，却读出 %d 字节\n", r); ok = 0; }
    }

    /* /dev/zero：读出恰好 ZERO_READ_LEN 个 0。 */
    OpenFile zero = vfs_open(&g_vfs, "/dev/zero");
    if (zero.mi < 0) {
        printf("DEVFS_FAIL open(\"/dev/zero\") 失败\n");
        ok = 0;
    } else {
        int r = vfs_read(&g_vfs, zero, buf, sizeof(buf));
        int allzero = (r == ZERO_READ_LEN);
        for (int i = 0; i < r; i++) if (buf[i] != 0) allzero = 0;
        if (!allzero) { printf("DEVFS_FAIL /dev/zero 应读出 %d 个全 0，得 %d 字节\n", ZERO_READ_LEN, r); ok = 0; }
    }

    /* 对照：RamFs /hello 真存储——写进去读得出（与 /dev/null 相反）。 */
    OpenFile hello = vfs_open(&g_vfs, "/hello");
    if (hello.mi >= 0) {
        vfs_write(&g_vfs, hello, (const uint8_t *)"changed", 7);
        int r = vfs_read(&g_vfs, hello, buf, sizeof(buf));
        if (r != 7 || memcmp(buf, "changed", 7) != 0) {
            printf("DEVFS_FAIL ramfs /hello 应真存储：写 changed 后应读出 changed\n");
            ok = 0;
        }
    }

    if (ok) printf("DEVFS_PASS\n");
    return ok;
}

static int check_dispatch(void) {
    int ok = 1;
    const char *paths[6] = {"/hello", "/motd", "/dev/null", "/dev/zero", "/dev/missing", "/ghost"};
    const char *want_fs[6] = {"ramfs", "ramfs", "devfs", "devfs", "devfs", "ramfs"};
    int want_exist[6] = {1, 1, 1, 1, 0, 0};

    for (int i = 0; i < 6; i++) {
        const char *got = vfs_fsname(&g_vfs, paths[i]);
        if (got == NULL || strcmp(got, want_fs[i]) != 0) {
            printf("DISPATCH_FAIL %s 应路由到 %s，得 %s\n", paths[i], want_fs[i], got ? got : "(null)");
            ok = 0;
        }
        int exist = vfs_open(&g_vfs, paths[i]).mi >= 0;
        if (exist != want_exist[i]) {
            printf("DISPATCH_BAD %s 存在性应=%d，得=%d\n", paths[i], want_exist[i], exist);
            ok = 0;
        }
    }

    if (ok) printf("DISPATCH_PASS\n");
    return ok;
}

int main(void) {
    build_vfs();
    dump();

    int all = 1;
    all &= check_vfs();
    all &= check_mount();
    all &= check_devfs();
    all &= check_dispatch();

    if (all) {
        printf("ALL_PASS\n");
        return 0;
    }
    return 1;
}
