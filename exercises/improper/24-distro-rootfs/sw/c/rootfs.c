/* 发行版与根文件系统：从光秃秃的内核到能进 shell 的 rootfs —— 学生填空版。
 *
 * 母题：内核只是引擎，光有它进不了 shell。要一个根文件系统(rootfs)——
 * 装上 init + 一堆工具——开机才有 userspace 可用。这就是「发行版」雏形。
 * 四段逐题递进，全在一棵「内存文件树」上演练：
 *   (a) FHS 目录树：/bin /etc /dev /proc /sys /lib 摆对位置       -> FHS_PASS
 *   (b) busybox 多合一二进制：同一程序按 argv[0] 分发成 ls/cat/.. -> BUSYBOX_PASS
 *   (c) mock init(PID 1)：挂 /proc /sys、读 inittab、起 shell     -> INIT_PASS
 *   (d) cpio 打包->解包还原整棵树（initramfs 概念）               -> INITRAMFS_PASS
 * 四段皆过再打印 ALL_PASS。
 *
 * 你要填两处：busybox 的 argv[0] 分发 + cpio 解包（解析头、重建文件）。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ── 内存文件树：路径 -> 节点 ──────────────────────────────────────── */
#define MAX_ENTRIES 64
#define PATH_CAP 64
#define BODY_CAP 256

typedef struct {
    char type;                  /* 'd' 目录 / 'f' 文件 / 'l' 符号链接 */
    char path[PATH_CAP];
    unsigned char body[BODY_CAP]; /* 文件内容 或 链接目标 */
    size_t bodylen;
} Entry;

typedef struct {
    Entry e[MAX_ENTRIES];
    int n;
} Tree;

static Entry *tree_find(Tree *t, const char *path) {
    for (int i = 0; i < t->n; i++)
        if (strcmp(t->e[i].path, path) == 0) return &t->e[i];
    return NULL;
}
static void tree_set(Tree *t, char type, const char *path,
                     const unsigned char *body, size_t bodylen) {
    Entry *e = tree_find(t, path);
    if (!e) {
        if (t->n >= MAX_ENTRIES) return;
        e = &t->e[t->n++];
    }
    e->type = type;
    strncpy(e->path, path, PATH_CAP - 1);
    e->path[PATH_CAP - 1] = 0;
    if (bodylen > BODY_CAP) bodylen = BODY_CAP;
    e->bodylen = bodylen;
    if (bodylen) memcpy(e->body, body, bodylen);
}

/* ── (a) FHS 目录树（已给）────────────────────────────────────────── */
static void build_rootfs(Tree *t) {
    t->n = 0;
    const char *dirs[] = {"/bin", "/etc", "/dev", "/proc", "/sys", "/lib"};
    for (int i = 0; i < 6; i++) tree_set(t, 'd', dirs[i], NULL, 0);
    tree_set(t, 'f', "/bin/busybox",
             (const unsigned char *)"<busybox multi-call binary>", 27);
    const char *apps[] = {"ls", "cat", "echo", "mount"};
    for (int i = 0; i < 4; i++) {
        char p[PATH_CAP];
        snprintf(p, sizeof p, "/bin/%s", apps[i]);
        tree_set(t, 'l', p, (const unsigned char *)"/bin/busybox", 12);
    }
    const char *itab =
        "::sysinit:/bin/mount -t proc proc /proc\n"
        "::sysinit:/bin/mount -t sysfs sysfs /sys\n"
        "::askfirst:/bin/sh\n";
    tree_set(t, 'f', "/etc/inittab", (const unsigned char *)itab, strlen(itab));
    tree_set(t, 'f', "/dev/null", (const unsigned char *)"", 0);
    tree_set(t, 'f', "/lib/libc.so", (const unsigned char *)"<libc>", 6);
}

static int check_fhs(void) {
    Tree t;
    build_rootfs(&t);
    const char *dirs[] = {"/bin", "/etc", "/dev", "/proc", "/sys", "/lib"};
    for (int i = 0; i < 6; i++) {
        Entry *e = tree_find(&t, dirs[i]);
        if (!e || e->type != 'd') {
            printf("FHS_FAIL 缺目录 %s\n", dirs[i]);
            return 0;
        }
    }
    Entry *bb = tree_find(&t, "/bin/busybox");
    if (!bb || bb->type != 'f') {
        printf("FHS_FAIL /bin/busybox 不是文件\n");
        return 0;
    }
    const char *apps[] = {"ls", "cat", "echo", "mount"};
    for (int i = 0; i < 4; i++) {
        char p[PATH_CAP];
        snprintf(p, sizeof p, "/bin/%s", apps[i]);
        Entry *e = tree_find(&t, p);
        if (!e || e->type != 'l' || e->bodylen != 12 ||
            memcmp(e->body, "/bin/busybox", 12) != 0) {
            printf("FHS_FAIL /bin/%s 不是指向 busybox 的符号链接\n", apps[i]);
            return 0;
        }
    }
    printf("FHS_PASS\n");
    return 1;
}

/* ── (b) busybox 多合一二进制 ──────────────────────────────────────── */
static const char *basename2(const char *p) {
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

/* 把 argv[1..] 用 sep 连接写进 out；sort!=0 时先按字典序排序（ls 用）。 */
static void join_args(char *out, size_t cap, int argc, const char **argv,
                      const char *sep, int sort) {
    const char *items[16];
    int m = 0;
    for (int i = 1; i < argc && m < 16; i++) items[m++] = argv[i];
    if (sort) {
        for (int a = 0; a < m; a++)
            for (int b = a + 1; b < m; b++)
                if (strcmp(items[a], items[b]) > 0) {
                    const char *tmp = items[a];
                    items[a] = items[b];
                    items[b] = tmp;
                }
    }
    out[0] = 0;
    size_t len = 0, sl = strlen(sep);
    for (int i = 0; i < m; i++) {
        if (i && len + sl < cap) {
            memcpy(out + len, sep, sl);
            len += sl;
            out[len] = 0;
        }
        size_t il = strlen(items[i]);
        if (len + il < cap) {
            memcpy(out + len, items[i], il);
            len += il;
            out[len] = 0;
        }
    }
}

/* 多合一入口：按 argv[0] 的 basename 分发，结果写进 out。 */
static void busybox_main(int argc, const char **argv, char *out, size_t cap) {
    const char *cmd = basename2(argv[0]);
    out[0] = 0;
    /* TODO: 按 argv[0] basename 分发（busybox 多合一）：
     *   "echo"  -> 参数以空格连接：     join_args(out, cap, argc, argv, " ", 0);
     *   "ls"    -> 参数排序后以 '\n' 连接：join_args(out, cap, argc, argv, "\n", 1);
     *   "cat"   -> 参数以 '\n' 连接：    join_args(out, cap, argc, argv, "\n", 0);
     *   "mount" -> "mounted " + 参数以空格连接（可借助一个临时 buffer + snprintf）
     *   其它    -> snprintf(out, cap, "busybox: applet not found: %s", cmd);
     *   // TODO[a] if/else 直接分发   // ELSE[b] 查一张 name->函数指针 表
     */
    (void)argc; (void)argv; (void)cmd; (void)cap; /* ← 占位：out 空 -> BUSYBOX_FAIL */
    (void)join_args; /* 填空时会用到的辅助函数（避免未使用告警） */
}

static int check_busybox(void) {
    char out[256];
    const char *a1[] = {"/bin/echo", "hello", "world"};
    busybox_main(3, a1, out, sizeof out);
    if (strcmp(out, "hello world") != 0) {
        printf("BUSYBOX_FAIL argv0=%s got=\"%s\" want=\"hello world\"\n", a1[0], out);
        return 0;
    }
    const char *a2[] = {"/bin/ls", "beta", "alpha", "gamma"};
    busybox_main(4, a2, out, sizeof out);
    if (strcmp(out, "alpha\nbeta\ngamma") != 0) {
        printf("BUSYBOX_FAIL argv0=%s got=\"%s\"\n", a2[0], out);
        return 0;
    }
    const char *a3[] = {"/bin/cat", "line1", "line2"};
    busybox_main(3, a3, out, sizeof out);
    if (strcmp(out, "line1\nline2") != 0) {
        printf("BUSYBOX_FAIL argv0=%s got=\"%s\"\n", a3[0], out);
        return 0;
    }
    const char *a4[] = {"/bin/mount", "-t", "proc", "proc", "/proc"};
    busybox_main(5, a4, out, sizeof out);
    if (strcmp(out, "mounted -t proc proc /proc") != 0) {
        printf("BUSYBOX_FAIL argv0=%s got=\"%s\"\n", a4[0], out);
        return 0;
    }
    const char *a5[] = {"/bin/nope"};
    busybox_main(1, a5, out, sizeof out);
    if (strstr(out, "not found") == NULL) {
        printf("BUSYBOX_FAIL 未知 applet 应报 not found，实得 \"%s\"\n", out);
        return 0;
    }
    printf("BUSYBOX_PASS\n");
    return 1;
}

/* ── (c) mock init（PID 1）（已给）─────────────────────────────────── */
#define LOG_CAP 16
typedef struct {
    char line[LOG_CAP][128];
    int n;
} Log;
static void logp(Log *l, const char *s) {
    if (l->n < LOG_CAP) {
        strncpy(l->line[l->n], s, 127);
        l->line[l->n][127] = 0;
        l->n++;
    }
}

static void mock_init(Tree *t, Log *log) {
    log->n = 0;
    logp(log, "init: I am PID 1");
    Entry *it = tree_find(t, "/etc/inittab");
    if (!it) {
        logp(log, "init: no /etc/inittab");
        return;
    }
    char buf[512];
    size_t bl = it->bodylen;
    if (bl >= sizeof buf) bl = sizeof buf - 1;
    memcpy(buf, it->body, bl);
    buf[bl] = 0;
    size_t i = 0;
    while (i < bl) {
        size_t j = i;
        while (j < bl && buf[j] != '\n') j++;
        char line[160];
        size_t ll = j - i;
        if (ll >= sizeof line) ll = sizeof line - 1;
        memcpy(line, buf + i, ll);
        line[ll] = 0;
        i = j + 1;
        if (ll > 2 && line[0] == ':' && line[1] == ':') {
            char *rest = line + 2;
            char *colon = strchr(rest, ':');
            if (colon) {
                *colon = 0;
                const char *action = rest;
                const char *cmd = colon + 1;
                char tmp[200];
                if (strcmp(action, "sysinit") == 0) {
                    snprintf(tmp, sizeof tmp, "init: sysinit %s", cmd);
                    logp(log, tmp);
                    if (strstr(cmd, "/proc"))
                        tree_set(t, 'f', "/proc/uptime",
                                 (const unsigned char *)"0.00 0.00", 9);
                    if (strstr(cmd, "/sys"))
                        tree_set(t, 'f', "/sys/kernel/ostype",
                                 (const unsigned char *)"EMMos", 5);
                } else if (strcmp(action, "askfirst") == 0 ||
                           strcmp(action, "respawn") == 0) {
                    snprintf(tmp, sizeof tmp, "init: spawn shell %s", cmd);
                    logp(log, tmp);
                } else {
                    snprintf(tmp, sizeof tmp, "init: skip %s", action);
                    logp(log, tmp);
                }
            }
        }
    }
}

static int log_find(const Log *l, const char *needle) {
    for (int i = 0; i < l->n; i++)
        if (strstr(l->line[i], needle)) return i;
    return -1;
}

static int check_init(void) {
    Tree t;
    build_rootfs(&t);
    Log log;
    mock_init(&t, &log);
    if (log.n == 0 || strcmp(log.line[0], "init: I am PID 1") != 0) {
        printf("INIT_FAIL 第一步应宣告 PID 1\n");
        return 0;
    }
    int p_sysinit = log_find(&log, "sysinit");
    int p_shell = log_find(&log, "spawn shell");
    if (p_sysinit < 0) {
        printf("INIT_FAIL 未执行 sysinit 挂载\n");
        return 0;
    }
    if (p_shell < 0) {
        printf("INIT_FAIL 未起 shell\n");
        return 0;
    }
    if (!tree_find(&t, "/proc/uptime")) {
        printf("INIT_FAIL /proc 未挂载\n");
        return 0;
    }
    if (!tree_find(&t, "/sys/kernel/ostype")) {
        printf("INIT_FAIL /sys 未挂载\n");
        return 0;
    }
    if (p_shell <= p_sysinit) {
        printf("INIT_FAIL shell 应在挂载之后才起\n");
        return 0;
    }
    printf("INIT_PASS\n");
    return 1;
}

/* ── (d) cpio 打包 / 解包（initramfs）──────────────────────────────────
 * 记录布局：magic[4]="0707" | type(1) | namelen(LE32) | bodylen(LE32) | name | body
 * 末尾一条 name=="TRAILER!!!" 的哨兵记录表示归档结束。
 */
static const unsigned char CPIO_MAGIC[4] = {'0', '7', '0', '7'};
#define CPIO_TRAILER "TRAILER!!!"
#define CPIO_HDR 13 /* magic4 + type1 + namelen4 + bodylen4 */

static void w32(unsigned char *p, uint32_t v) {
    p[0] = v & 0xff;
    p[1] = (v >> 8) & 0xff;
    p[2] = (v >> 16) & 0xff;
    p[3] = (v >> 24) & 0xff;
}
static uint32_t r32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static size_t cpio_pack(const Tree *t, unsigned char *out, size_t cap) {
    size_t off = 0;
    for (int i = 0; i < t->n; i++) {
        const Entry *e = &t->e[i];
        size_t namelen = strlen(e->path);
        size_t bodylen = e->bodylen;
        if (off + CPIO_HDR + namelen + bodylen > cap) break;
        memcpy(out + off, CPIO_MAGIC, 4);
        off += 4;
        out[off++] = (unsigned char)e->type;
        w32(out + off, (uint32_t)namelen);
        off += 4;
        w32(out + off, (uint32_t)bodylen);
        off += 4;
        memcpy(out + off, e->path, namelen);
        off += namelen;
        if (bodylen) {
            memcpy(out + off, e->body, bodylen);
            off += bodylen;
        }
    }
    size_t tl = strlen(CPIO_TRAILER);
    if (off + CPIO_HDR + tl <= cap) {
        memcpy(out + off, CPIO_MAGIC, 4);
        off += 4;
        out[off++] = 'd';
        w32(out + off, (uint32_t)tl);
        off += 4;
        w32(out + off, 0);
        off += 4;
        memcpy(out + off, CPIO_TRAILER, tl);
        off += tl;
    }
    return off;
}

/* 解析 cpio 归档，逐条还原成文件树。成功返回 0，出错返回 -1。 */
static int cpio_unpack(const unsigned char *arc, size_t len, Tree *out) {
    out->n = 0;
    size_t off = 0;
    /* TODO: 循环解析 cpio 记录，逐条还原节点：
     *   1) 边界检查 off + CPIO_HDR <= len，否则 return -1
     *   2) memcmp(arc+off, CPIO_MAGIC, 4) != 0 -> return -1
     *   3) type = arc[off+4]
     *      namelen = r32(arc+off+5)；bodylen = r32(arc+off+9)
     *   4) name = arc[off+CPIO_HDR .. +namelen]（拷进局部 buffer 并补 '\0'）
     *      若 strcmp(name, CPIO_TRAILER)==0 则 break
     *   5) body = 紧随其后的 bodylen 字节；tree_set(out, type, name, body, bodylen)
     *   6) off 前进到 body 之后，继续循环
     */
    (void)arc; (void)len; (void)off; /* ← 占位 */
    (void)r32; /* 填空时会用到的辅助函数（避免未使用告警） */
    return -1; /* 未实现 -> INITRAMFS_FAIL */
}

static int tree_equal(const Tree *a, const Tree *b) {
    if (a->n != b->n) return 0;
    for (int i = 0; i < a->n; i++) {
        const Entry *ea = &a->e[i];
        const Entry *eb = NULL;
        for (int j = 0; j < b->n; j++)
            if (strcmp(b->e[j].path, ea->path) == 0) {
                eb = &b->e[j];
                break;
            }
        if (!eb || eb->type != ea->type || eb->bodylen != ea->bodylen) return 0;
        if (ea->bodylen && memcmp(eb->body, ea->body, ea->bodylen) != 0) return 0;
    }
    return 1;
}

static int check_initramfs(void) {
    Tree t;
    build_rootfs(&t);
    Log log;
    mock_init(&t, &log); /* 让 /proc /sys 虚拟条目也进树 */
    unsigned char arc[8192];
    size_t alen = cpio_pack(&t, arc, sizeof arc);
    Tree r;
    if (cpio_unpack(arc, alen, &r) != 0) {
        printf("INITRAMFS_FAIL 解包出错\n");
        return 0;
    }
    if (!tree_equal(&t, &r)) {
        printf("INITRAMFS_FAIL 解包还原与原树不一致 (orig=%d, got=%d)\n", t.n, r.n);
        return 0;
    }
    printf("INITRAMFS_PASS\n");
    return 1;
}

/* ── 测试 harness（勿改）── */
int main(void) {
    int all = 1;
    all &= check_fhs();
    all &= check_busybox();
    all &= check_init();
    all &= check_initramfs();
    if (all) {
        printf("ALL_PASS\n");
        return 0;
    }
    return 1;
}
