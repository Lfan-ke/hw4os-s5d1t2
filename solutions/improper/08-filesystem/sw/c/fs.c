/* 文件管理：从裸指针块设备到 easy-fs 风格的简易文件系统 —— C 参考解。
 *
 * 母题：磁盘只是「一大片能按块寻址的格子」，文件 / 目录 / KV 记录都是
 * 软件在这片格子上约定出来的字节排布。三段逐题递进：
 *   (a) 裸指针 / MMIO 块设备：写进去再读出来逐字节相等   -> BDEV_PASS
 *   (b) 按 key 扫描 KV 记录（EMM233 头 / EMM666 尾）       -> KV_PASS
 *   (c) easy-fs 风格 inode / 目录 / 目录项                -> FS_PASS
 * 三段皆过再打印 ALL_PASS。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define BLOCK_SIZE 512
#define NUM_BLOCKS 64

/* ── (a) 裸指针 / MMIO 块设备 ──────────────────────────────────────── */
/* 块设备 = 一大片字节 RAM；块号 × 块大小 = MMIO 数据窗口基址。 */
static uint8_t DISK[BLOCK_SIZE * NUM_BLOCKS];

static volatile uint8_t *bd_window(uint32_t block_id) {
    return (volatile uint8_t *)(DISK + (size_t)block_id * BLOCK_SIZE);
}
static void bd_write_block(uint32_t block_id, const uint8_t *buf) {
    volatile uint8_t *w = bd_window(block_id);
    for (size_t i = 0; i < BLOCK_SIZE; i++) w[i] = buf[i];
}
static void bd_read_block(uint32_t block_id, uint8_t *buf) {
    volatile uint8_t *w = bd_window(block_id);
    for (size_t i = 0; i < BLOCK_SIZE; i++) buf[i] = w[i];
}

/* ── (b) KV 记录扫描 ──────────────────────────────────────────────── */
/* 记录布局：EMM233 | key[3] | len(u8) | data[len] | EMM666 */
/* 返回命中记录数；*out_sum 累加命中记录的数据字节校验和。 */
static int find_by_key(const uint8_t *buf, size_t len, const uint8_t key[3], uint32_t *out_sum) {
    int count = 0;
    uint32_t sum = 0;
    size_t off = 0;
    while (off + 16 <= len) {
        if (memcmp(buf + off, "EMM233", 6) != 0) break; /* 头不匹配 = 结束 */
        const uint8_t *rkey = buf + off + 6;
        size_t dl = buf[off + 9];
        size_t tail = off + 10 + dl;
        if (tail + 6 > len || memcmp(buf + tail, "EMM666", 6) != 0) break; /* 尾损坏 */
        if (memcmp(rkey, key, 3) == 0) {
            count++;
            for (size_t i = 0; i < dl; i++) sum += buf[off + 10 + i];
        }
        off = tail + 6;
    }
    *out_sum = sum;
    return count;
}

/* ── (c) easy-fs 风格 inode / 目录 / 目录项 ───────────────────────────
 * 磁盘布局：
 *   block 0      : SuperBlock
 *   block 1      : inode 区（16 个 DiskInode，每个 32 字节）
 *   block 2..    : 数据区（块 2 预留给根目录的目录项）
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
    uint8_t zero[BLOCK_SIZE];
    memset(zero, 0, sizeof zero);
    for (uint32_t b = 0; b < NUM_BLOCKS; b++) bd_write_block(b, zero);
    uint8_t sb[BLOCK_SIZE];
    memset(sb, 0, sizeof sb);
    wr_u32(sb, 0, MAGIC);
    wr_u32(sb, 4, NUM_BLOCKS);
    wr_u32(sb, 8, 1);   /* inode_start */
    wr_u32(sb, 12, 16); /* inode_count_max */
    wr_u32(sb, 16, 2);  /* data_start */
    wr_u32(sb, 20, 1);  /* next_inode */
    wr_u32(sb, 24, 3);  /* next_block */
    bd_write_block(0, sb);
    uint32_t root_dir[MAX_DIRECT] = {2, 0, 0, 0, 0, 0};
    write_inode(0, 0, 1, root_dir); /* 根目录 inode 0：dir */
}

static uint32_t fs_create(const char *name) {
    uint8_t sb[BLOCK_SIZE];
    bd_read_block(0, sb);
    uint32_t ino = rd_u32(sb, 20);
    wr_u32(sb, 20, ino + 1);
    bd_write_block(0, sb);
    uint32_t empty[MAX_DIRECT] = {0, 0, 0, 0, 0, 0};
    write_inode(ino, 0, 2, empty); /* 空文件 */
    uint32_t rsize, rtyp, rdir[MAX_DIRECT];
    read_inode(0, &rsize, &rtyp, rdir);
    size_t idx = rsize / DIRENT_SIZE;
    uint8_t blk[BLOCK_SIZE];
    bd_read_block(rdir[0], blk);
    size_t de = idx * DIRENT_SIZE;
    size_t nlen = strlen(name);
    for (size_t j = 0; j < NAME_CAP; j++) blk[de + j] = (j < nlen) ? (uint8_t)name[j] : 0;
    wr_u32(blk, de + NAME_CAP, ino);
    bd_write_block(rdir[0], blk);
    write_inode(0, rsize + DIRENT_SIZE, 1, rdir);
    return ino;
}

static int fs_ls(char names[][32], int maxn) {
    uint32_t rsize, rtyp, rdir[MAX_DIRECT];
    read_inode(0, &rsize, &rtyp, rdir);
    uint8_t blk[BLOCK_SIZE];
    bd_read_block(rdir[0], blk);
    int n = (int)(rsize / DIRENT_SIZE);
    if (n > maxn) n = maxn;
    for (int i = 0; i < n; i++) {
        size_t de = (size_t)i * DIRENT_SIZE;
        memcpy(names[i], blk + de, NAME_CAP);
        names[i][NAME_CAP] = 0;
    }
    return n;
}

static void fs_write(uint32_t ino, const uint8_t *data, size_t len) {
    size_t nblocks = (len + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint8_t sb[BLOCK_SIZE];
    bd_read_block(0, sb);
    uint32_t nextb = rd_u32(sb, 24);
    uint32_t directs[MAX_DIRECT] = {0, 0, 0, 0, 0, 0};
    for (size_t k = 0; k < nblocks; k++) {
        directs[k] = nextb++;
        uint8_t blk[BLOCK_SIZE];
        memset(blk, 0, sizeof blk);
        size_t s = k * BLOCK_SIZE;
        size_t e = s + BLOCK_SIZE;
        if (e > len) e = len;
        memcpy(blk, data + s, e - s);
        bd_write_block(directs[k], blk);
    }
    wr_u32(sb, 24, nextb);
    bd_write_block(0, sb);
    write_inode(ino, (uint32_t)len, 2, directs);
}

static size_t fs_read(uint32_t ino, uint8_t *out, size_t cap) {
    uint32_t size, typ, directs[MAX_DIRECT];
    read_inode(ino, &size, &typ, directs);
    size_t remaining = size, k = 0, pos = 0;
    while (remaining > 0 && pos < cap) {
        uint8_t blk[BLOCK_SIZE];
        bd_read_block(directs[k], blk);
        size_t step = remaining < BLOCK_SIZE ? remaining : BLOCK_SIZE;
        size_t take = step;
        if (pos + take > cap) take = cap - pos;
        memcpy(out + pos, blk, take);
        pos += take;
        remaining -= step;
        k++;
    }
    return pos;
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
    /* 其余字节保持 0 作终止哨兵 */
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
