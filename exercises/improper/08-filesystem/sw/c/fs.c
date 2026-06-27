/* 文件管理：从裸指针块设备到 easy-fs 风格的简易文件系统 —— C。
 *
 * 母题：磁盘只是「一大片能按块寻址的格子」，文件 / 目录 / KV 记录都是
 * 软件在这片格子上约定出来的字节排布。三段逐题递进，你只填 TODO：
 *   (a) 裸指针 / MMIO 块设备                              -> BDEV_PASS
 *   (b) 按 key 扫描 KV 记录（EMM233 头 / EMM666 尾）       -> KV_PASS
 *   (c) easy-fs 风格 inode / 目录 / 目录项                -> FS_PASS
 * 三段皆过再打印 ALL_PASS。下方测试 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define BLOCK_SIZE 512
#define NUM_BLOCKS 64

/* ── (a) 裸指针 / MMIO 块设备 ──────────────────────────────────────── */
static uint8_t DISK[BLOCK_SIZE * NUM_BLOCKS];

static volatile uint8_t *bd_window(uint32_t block_id) {
    return (volatile uint8_t *)(DISK + (size_t)block_id * BLOCK_SIZE);
}
static void bd_write_block(uint32_t block_id, const uint8_t *buf) {
    volatile uint8_t *w = bd_window(block_id);
    /* TODO: 把 buf[0..BLOCK_SIZE] 写到窗口 w。
     *   // TODO[a] 按字节循环： for(i) w[i]=buf[i];
     *   // ELSE[b] 整块 memcpy： memcpy((void*)w, buf, BLOCK_SIZE);
     */
    (void)w; (void)buf; /* ← 占位：什么都没写 -> 读回不相等 -> BDEV_FAIL */
}
static void bd_read_block(uint32_t block_id, uint8_t *buf) {
    volatile uint8_t *w = bd_window(block_id);
    /* TODO: 把窗口 w 的 BLOCK_SIZE 字节读进 buf。 for(i) buf[i]=w[i]; */
    (void)w; (void)buf; /* ← 占位：没有读出 -> BDEV_FAIL */
}

/* ── (b) KV 记录扫描 ──────────────────────────────────────────────── */
/* 记录布局：EMM233 | key[3] | len(u8) | data[len] | EMM666 */
static int find_by_key(const uint8_t *buf, size_t len, const uint8_t key[3], uint32_t *out_sum) {
    int count = 0;
    uint32_t sum = 0;
    size_t off = 0;
    /* TODO: while off+16<=len:
     *   1) memcmp(buf+off,"EMM233",6)!=0 -> break（记录区结束）
     *   2) rkey=buf+off+6; dl=buf[off+9]; tail=off+10+dl
     *   3) tail+6>len 或 memcmp(buf+tail,"EMM666",6)!=0 -> break
     *   4) memcmp(rkey,key,3)==0 -> count++，把 dl 个 data 字节累加进 sum
     *   5) off = tail+6
     *   // TODO[a] 线性扫描（如上）  // ELSE[b] 先建 key->offset 索引表
     */
    (void)buf; (void)len; (void)key; (void)off; /* ← 占位 */
    *out_sum = sum;
    return count; /* 返回 0 -> KV_FAIL */
}

/* ── (c) easy-fs 风格 inode / 目录 / 目录项 ───────────────────────────
 * 磁盘布局： block 0=SuperBlock；block 1=inode 区；block 2=根目录数据块；block 3..=数据区
 */
#define MAGIC 0x73667A65u
#define INODE_SIZE 32
#define MAX_DIRECT 6
#define DIRENT_SIZE 32
#define NAME_CAP 28

static uint32_t rd_u32(const uint8_t *b, size_t off) {
    return (uint32_t)b[off] | ((uint32_t)b[off + 1] << 8) |
           ((uint32_t)b[off + 2] << 16) | ((uint32_t)b[off + 3] << 24);
}
static void wr_u32(uint8_t *b, size_t off, uint32_t v) {
    b[off] = v & 0xff;
    b[off + 1] = (v >> 8) & 0xff;
    b[off + 2] = (v >> 16) & 0xff;
    b[off + 3] = (v >> 24) & 0xff;
}

/* 这两个 inode 低层读写已给好，直接用。 */
static void write_inode(uint32_t ino, uint32_t size, uint32_t typ, const uint32_t *directs) {
    uint8_t blk[BLOCK_SIZE];
    bd_read_block(1, blk);
    size_t base = (size_t)ino * INODE_SIZE;
    wr_u32(blk, base, size);
    wr_u32(blk, base + 4, typ);
    for (int k = 0; k < MAX_DIRECT; k++) wr_u32(blk, base + 8 + 4 * k, directs[k]);
    bd_write_block(1, blk);
}
static void read_inode(uint32_t ino, uint32_t *size, uint32_t *typ, uint32_t *directs) {
    uint8_t blk[BLOCK_SIZE];
    bd_read_block(1, blk);
    size_t base = (size_t)ino * INODE_SIZE;
    *size = rd_u32(blk, base);
    *typ = rd_u32(blk, base + 4);
    for (int k = 0; k < MAX_DIRECT; k++) directs[k] = rd_u32(blk, base + 8 + 4 * k);
}

static void fs_mkfs(void) {
    /* TODO:
     *   1) 清零全部 NUM_BLOCKS 个块
     *   2) block 0 写 SuperBlock：off0=MAGIC, off4=NUM_BLOCKS, off8=1, off12=16,
     *      off16=2, off20=1(next_inode), off24=3(next_block)
     *   3) write_inode(0,0,1, {2,0,0,0,0,0})  根目录 dir，direct[0]=2
     */
    (void)write_inode; (void)read_inode; /* ← 占位：未格式化 -> FS_FAIL（填好后这两个辅助函数会被你用上）*/
}

static uint32_t fs_create(const char *name) {
    /* TODO:
     *   1) 读 block0；ino=next_inode(off20)；off20 写回 ino+1
     *   2) write_inode(ino,0,2, 全 0)  空文件
     *   3) read_inode(0,...) 拿 rsize/rdir；idx=rsize/DIRENT_SIZE
     *   4) 读 rdir[0] 数据块，在第 idx 条目写 name(前 NAME_CAP 字节，不足补 0)+ wr_u32(ino) 到 +NAME_CAP
     *   5) 写回数据块；write_inode(0, rsize+DIRENT_SIZE, 1, rdir)
     */
    (void)name; /* ← 占位 */
    return 0;   /* -> FS_FAIL */
}

static int fs_ls(char names[][32], int maxn) {
    /* TODO: read_inode(0,...) 拿 rsize/rdir；读 rdir[0]；
     *   对 i in 0..(rsize/DIRENT_SIZE)：memcpy names[i]<-NAME_CAP 字节并补 '\0'。返回条数。 */
    (void)names; (void)maxn; /* ← 占位 */
    return 0; /* -> FS_FAIL */
}

static void fs_write(uint32_t ino, const uint8_t *data, size_t len) {
    /* TODO:
     *   1) nblocks=ceil(len/BLOCK_SIZE)
     *   2) 读 block0 拿 next_block(off24)；每块分配一个号填 directs[k] 并写入内容（末块补 0）
     *   3) next_block 写回 block0；write_inode(ino, len, 2, directs)
     */
    (void)ino; (void)data; (void)len; /* ← 占位 */
}

static size_t fs_read(uint32_t ino, uint8_t *out, size_t cap) {
    /* TODO: read_inode 拿 size/directs；按 size 逐块读回拼进 out（末块只取剩余字节）。返回字节数。 */
    (void)ino; (void)out; (void)cap; /* ← 占位 */
    return 0; /* -> FS_FAIL */
}

/* ── 测试 harness（勿改）── */

static int check_bdev(void) {
    uint32_t blocks[] = {3, 7, 40, 63};
    for (int t = 0; t < 4; t++) {
        uint32_t b = blocks[t];
        uint8_t buf[BLOCK_SIZE];
        for (size_t i = 0; i < BLOCK_SIZE; i++) buf[i] = (uint8_t)((b * 7 + i) & 0xff);
        bd_write_block(b, buf);
    }
    for (int t = 0; t < 4; t++) {
        uint32_t b = blocks[t];
        uint8_t buf[BLOCK_SIZE] = {0};
        bd_read_block(b, buf);
        for (size_t i = 0; i < BLOCK_SIZE; i++) {
            uint8_t want = (uint8_t)((b * 7 + i) & 0xff);
            if (buf[i] != want) {
                printf("BDEV_FAIL blk=%u i=%zu got=%u want=%u\n", b, i, buf[i], want);
                return 0;
            }
        }
    }
    printf("BDEV_PASS\n");
    return 1;
}

static size_t push_rec(uint8_t *buf, size_t off, const uint8_t key[3], const uint8_t *data, uint8_t dl) {
    memcpy(buf + off, "EMM233", 6); off += 6;
    memcpy(buf + off, key, 3); off += 3;
    buf[off++] = dl;
    memcpy(buf + off, data, dl); off += dl;
    memcpy(buf + off, "EMM666", 6); off += 6;
    return off;
}

static int check_kv(void) {
    uint8_t buf[256];
    memset(buf, 0, sizeof buf);
    uint8_t d1[] = {1, 2, 3, 4};
    uint8_t d2[] = {10, 20, 30};
    uint8_t d3[] = {5, 6, 7, 8, 9};
    uint8_t d4[] = {100};
    size_t off = 0;
    off = push_rec(buf, off, (const uint8_t *)"log", d1, 4);
    off = push_rec(buf, off, (const uint8_t *)"cfg", d2, 3);
    off = push_rec(buf, off, (const uint8_t *)"log", d3, 5);
    off = push_rec(buf, off, (const uint8_t *)"usr", d4, 1);
    uint32_t s1 = 0, s2 = 0;
    int c1 = find_by_key(buf, sizeof buf, (const uint8_t *)"log", &s1);
    if (c1 != 2 || s1 != 45) {
        printf("KV_FAIL key=log got=(%d,%u) want=(2,45)\n", c1, s1);
        return 0;
    }
    int c2 = find_by_key(buf, sizeof buf, (const uint8_t *)"cfg", &s2);
    if (c2 != 1 || s2 != 60) {
        printf("KV_FAIL key=cfg got=(%d,%u) want=(1,60)\n", c2, s2);
        return 0;
    }
    printf("KV_PASS\n");
    return 1;
}

static int check_fs(void) {
    fs_mkfs();
    uint32_t i1 = fs_create("alpha.txt");
    fs_create("beta.bin");
    fs_create("gamma");
    char names[16][32];
    int n = fs_ls(names, 16);
    const char *want[] = {"alpha.txt", "beta.bin", "gamma"};
    for (int w = 0; w < 3; w++) {
        int found = 0;
        for (int i = 0; i < n; i++)
            if (strcmp(names[i], want[w]) == 0) found = 1;
        if (!found) {
            printf("FS_FAIL ls 缺少 %s\n", want[w]);
            return 0;
        }
    }
    if (n != 3) {
        printf("FS_FAIL ls 应有 3 项，实得 %d\n", n);
        return 0;
    }
    uint8_t content[700];
    for (uint32_t i = 0; i < 700; i++) content[i] = (uint8_t)(i * 31 + 7);
    fs_write(i1, content, sizeof content);
    uint8_t got[700];
    size_t glen = fs_read(i1, got, sizeof got);
    if (glen != sizeof content || memcmp(got, content, sizeof content) != 0) {
        printf("FS_FAIL 读回不一致 got_len=%zu want_len=%zu\n", glen, sizeof content);
        return 0;
    }
    printf("FS_PASS\n");
    return 1;
}

int main(void) {
    int all = 1;
    all &= check_bdev();
    all &= check_kv();
    all &= check_fs();
    if (all) {
        printf("ALL_PASS\n");
        return 0;
    }
    return 1;
}
