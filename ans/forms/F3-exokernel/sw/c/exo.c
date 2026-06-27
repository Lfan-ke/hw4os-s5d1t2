/* 外核（Exokernel）形态认知 demo —— C 参考解。
 *
 * 本质权衡：内核只做「安全多路复用裸资源 + 保护检查」，**不强加抽象**。
 *   - exo 内核只发资源句柄：exo_alloc 把一段不重叠的块发给某个 owner；
 *     越界 / 重叠的请求一律拒绝。内核当「裁判」不当「独裁者」。
 *   - 抽象交给应用自带的 libOS：libA 在自己拿到的块上建「顺序布局」文件系统，
 *     libB 建「分块（scatter）布局」——同一裸磁盘，长出两种文件抽象，都能读写。
 *
 * 对应真实系统：MIT 6.828 jos / Aegis(Engler'95) / Xok。
 *
 * 学生只需填两处：① exo_alloc 边界/重叠校验；② libB 分块布局。harness 勿改。
 */
#include <stdio.h>
#include <string.h>

#define NBLK 16
#define BS 4
#define FREE (-1)

/* 整个系统共享一块物理磁盘；exo 内核把它的「块」分给不同 libOS。 */
static unsigned char disk[NBLK * BS];

/* ════════════════════════════════════════════════════════════════
 * 学生填空区 ①：exo 内核——安全多路复用 + 保护检查（不含任何抽象）
 * ════════════════════════════════════════════════════════════════ */

/* exo 内核唯一职责：把块区间 [start, start+len) 发给 who。
 * 裁判规则（仅校验，不解释用途）：
 *   - 越界：start+len <= NBLK 且 len>0，否则拒绝。
 *   - 重叠：区间内每个块都必须空闲(owner==FREE)，否则拒绝。
 * 通过则标记归属并返回 1；否则返回 0 且不改动。 */
static int exo_alloc(int owner[NBLK], int who, int start, int len) {
    int b;
    /* 越界校验（用减法比较避免溢出）。 */
    if (len <= 0 || start < 0 || start >= NBLK || len > NBLK - start)
        return 0;
    /* 重叠校验：必须整段空闲。 */
    for (b = start; b < start + len; b++)
        if (owner[b] != FREE)
            return 0;
    /* 安全：发放资源句柄。 */
    for (b = start; b < start + len; b++)
        owner[b] = who;
    return 1;
}

/* ════════════════════════════════════════════════════════════════
 * 学生填空区 ②：libB 的「分块（scatter）布局」策略
 * ════════════════════════════════════════════════════════════════ */

/* libB 块放置：在领地 [base, base+len) 里挑 n 个空闲块给一个文件。
 * libB 选择「分块/倒序」布局：从领地尾部往前挑（高块号优先），
 * 把挑中的块号写进 out[]（逻辑顺序=采集顺序），返回放置块数（成功=n，失败=0）。
 * 与 libA「顺序升序」形成对比——同样裸块，不同抽象。块不够则返回 0 不改 used。 */
static int libb_place(int base, int len, int used[NBLK], int n, int out[]) {
    int avail = 0, b, cnt = 0;
    if (n <= 0)
        return 0;
    for (b = base; b < base + len; b++)
        if (!used[b])
            avail++;
    if (avail < n)
        return 0;
    /* 从尾向前采集（高块号优先）：libB 与 libA 不同的布局选择。 */
    for (b = base + len - 1; b >= base && cnt < n; b--) {
        if (!used[b]) {
            used[b] = 1;
            out[cnt++] = b;
        }
    }
    return cnt;
}

/* ════════════════════════════════════════════════════════════════
 * 以下为参考实现 + 测试 harness —— 勿改
 * ════════════════════════════════════════════════════════════════ */

/* libA 的「顺序升序」布局：从领地头部找第一段连续 n 个空闲块（给定参照）。 */
static int liba_place(int base, int len, int used[NBLK], int n, int out[]) {
    int i, b, k;
    if (n <= 0 || n > len)
        return 0;
    for (i = base; i + n <= base + len; i++) {
        int all_free = 1;
        for (b = i; b < i + n; b++)
            if (used[b])
                all_free = 0;
        if (all_free) {
            for (k = 0; k < n; k++) {
                used[i + k] = 1;
                out[k] = i + k;
            }
            return n;
        }
    }
    return 0;
}

/* 最朴素的 libOS 文件系统：固定容纳两个文件的 inode。 */
#define MAXF 2
#define MAXB 4
typedef struct {
    int base, len, chunked;
    int used[NBLK];
    int nf;
    char name[MAXF][8];
    int blocks[MAXF][MAXB];
    int nblk[MAXF];
    int blen[MAXF];
} LibFs;

static void fs_init(LibFs *fs, int base, int len, int chunked) {
    memset(fs, 0, sizeof(*fs));
    fs->base = base;
    fs->len = len;
    fs->chunked = chunked;
}

static int fs_write(LibFs *fs, const char *name, const unsigned char *data, int dlen) {
    int n = (dlen + BS - 1) / BS;
    int out[MAXB];
    int got, i, fi;
    if (fs->nf >= MAXF || n > MAXB)
        return 0;
    if (fs->chunked)
        got = libb_place(fs->base, fs->len, fs->used, n, out);
    else
        got = liba_place(fs->base, fs->len, fs->used, n, out);
    if (got != n)
        return 0;
    fi = fs->nf++;
    strncpy(fs->name[fi], name, 7);
    fs->nblk[fi] = n;
    fs->blen[fi] = dlen;
    for (i = 0; i < n; i++)
        fs->blocks[fi][i] = out[i];
    for (i = 0; i < dlen; i++)
        disk[out[i / BS] * BS + i % BS] = data[i];
    return 1;
}

static int fs_find(LibFs *fs, const char *name) {
    int i;
    for (i = 0; i < fs->nf; i++)
        if (strcmp(fs->name[i], name) == 0)
            return i;
    return -1;
}

/* 读文件到 out[]，返回字节数；找不到返回 -1。 */
static int fs_read(LibFs *fs, const char *name, unsigned char *out) {
    int fi = fs_find(fs, name), i;
    if (fi < 0)
        return -1;
    for (i = 0; i < fs->blen[fi]; i++)
        out[i] = disk[fs->blocks[fi][i / BS] * BS + i % BS];
    return fs->blen[fi];
}

static int check_exo(void) {
    int owner[NBLK];
    int ok = 1, i, b;
    int reqs[8][4] = {
        /* who, start, len, 期望准许 */
        {0, 0, 6, 1},   {1, 6, 6, 1},   {2, 4, 3, 0},   {2, 14, 4, 0},
        {2, 12, 4, 1},  {3, 0, 1, 0},   {3, 12, 1, 0},  {3, 16, 1, 0},
    };
    for (i = 0; i < NBLK; i++)
        owner[i] = FREE;

    for (i = 0; i < 8; i++) {
        int got = exo_alloc(owner, reqs[i][0], reqs[i][1], reqs[i][2]);
        if (got != reqs[i][3]) {
            printf("EXO_BAD 请求#%d (who=%d start=%d len=%d) 期望准许=%d 实得=%d\n",
                   i, reqs[i][0], reqs[i][1], reqs[i][2], reqs[i][3], got);
            ok = 0;
        }
    }
    for (b = 0; b < NBLK; b++) {
        int want = (b < 6) ? 0 : (b < 12) ? 1 : 2;
        if (owner[b] != want) {
            printf("EXO_BAD 块%d 归属=%d 期望=%d\n", b, owner[b], want);
            ok = 0;
        }
    }
    if (ok)
        printf("EXO_PASS exo 内核安全多路复用：越界/重叠请求被拒，合法请求获句柄\n");
    return ok;
}

static int check_libos(void) {
    int owner[NBLK];
    int ok = 1, i;
    unsigned char f1[5] = {10, 11, 12, 13, 14}; /* 2 块 */
    unsigned char f2[3] = {20, 21, 22};         /* 1 块 */
    unsigned char buf[8];
    LibFs liba, libb;
    LibFs *both[2];

    for (i = 0; i < NBLK; i++)
        owner[i] = FREE;
    if (!exo_alloc(owner, 0, 0, 6) || !exo_alloc(owner, 1, 6, 6)) {
        printf("LIBOS_BAD libOS 没拿到资源句柄（exo_alloc 校验未实现?）\n");
        return 0;
    }

    fs_init(&liba, 0, 6, 0); /* 顺序布局 */
    fs_init(&libb, 6, 6, 1); /* 分块布局 */
    both[0] = &liba;
    both[1] = &libb;
    for (i = 0; i < 2; i++) {
        if (!fs_write(both[i], "f1", f1, 5) || !fs_write(both[i], "f2", f2, 3)) {
            printf("LIBOS_BAD 某 libOS 写文件失败（布局策略未实现?）\n");
            return 0;
        }
    }

    /* (a) 两种布局都能正确 round-trip 同一份数据。 */
    for (i = 0; i < 2; i++) {
        const char *tag = (i == 0) ? "libA" : "libB";
        int n1 = fs_read(both[i], "f1", buf);
        if (n1 != 5 || memcmp(buf, f1, 5) != 0) {
            printf("LIBOS_BAD %s 读回 f1 与写入不一致\n", tag);
            ok = 0;
        }
        int n2 = fs_read(both[i], "f2", buf);
        if (n2 != 3 || memcmp(buf, f2, 3) != 0) {
            printf("LIBOS_BAD %s 读回 f2 与写入不一致\n", tag);
            ok = 0;
        }
    }

    /* (b) 同份数据，两种布局物理映射不同 → 证明「抽象不同」。 */
    int ai = fs_find(&liba, "f1"), bi = fs_find(&libb, "f1");
    int *ba = liba.blocks[ai], *bb = libb.blocks[bi];
    if (ba[0] == bb[0] && ba[1] == bb[1]) {
        printf("LIBOS_BAD 两个 libOS 的块布局相同，没体现「不同抽象」\n");
        ok = 0;
    }
    if (!(ba[0] == 0 && ba[1] == 1)) {
        printf("LIBOS_BAD libA f1 布局非「顺序升序」: [%d,%d]\n", ba[0], ba[1]);
        ok = 0;
    }
    if (!(bb[0] == bb[1] + 1)) {
        printf("LIBOS_BAD libB f1 布局非「分块倒序」: [%d,%d]\n", bb[0], bb[1]);
        ok = 0;
    }

    /* (c) 隔离：每个 libOS 只碰自己被发放的块。 */
    for (i = 0; i < 2; i++) {
        LibFs *fs = both[i];
        int lo = fs->base, hi = fs->base + fs->len, fi, k;
        const char *tag = (i == 0) ? "libA" : "libB";
        for (fi = 0; fi < fs->nf; fi++)
            for (k = 0; k < fs->nblk[fi]; k++)
                if (fs->blocks[fi][k] < lo || fs->blocks[fi][k] >= hi) {
                    printf("LIBOS_BAD %s 越界访问了不属于自己的块\n", tag);
                    ok = 0;
                }
    }

    if (ok)
        printf("LIBOS_PASS 同一裸磁盘、同一套块资源：libA=[%d,%d](顺序) vs "
               "libB=[%d,%d](分块)，都正确读写\n",
               ba[0], ba[1], bb[0], bb[1]);
    return ok;
}

int main(void) {
    int all = 1;
    all &= check_exo();
    all &= check_libos();
    if (all) {
        printf("ALL_PASS\n");
        return 0;
    }
    return 1;
}
