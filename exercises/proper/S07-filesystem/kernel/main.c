/* S07 · 内核入口/测试驱动（给定，勿改）。
 * 三步自检：块设备读写 → mkfs+create/write/read/lookup → ls，全过打印 ALL_PASS。 */
#include "kernel.h"
#include "fs.h"

/* 比较两片内存是否逐字节相等。 */
static int mem_eq(const void *a, const void *b, uint32_t n) {
    const uint8_t *x = a, *y = b;
    for (uint32_t i = 0; i < n; i++) if (x[i] != y[i]) return 0;
    return 1;
}

/* —— 子项 1：块设备 bread/bwrite 写进去再读出来逐字节相等 —— */
static int test_blockdev(void) {
    uint8_t out[BSIZE], in[BSIZE];
    /* 在两个不同块写特征图案 */
    for (uint32_t i = 0; i < BSIZE; i++) out[i] = (uint8_t)(i * 7u + 3u);
    bwrite(10, out);
    for (uint32_t i = 0; i < BSIZE; i++) out[i] = (uint8_t)(255u - i);
    bwrite(20, out);
    /* 读回块 10 校验 */
    bread(10, in);
    for (uint32_t i = 0; i < BSIZE; i++)
        if (in[i] != (uint8_t)(i * 7u + 3u)) return 0;
    /* 读回块 20 校验 */
    bread(20, in);
    for (uint32_t i = 0; i < BSIZE; i++)
        if (in[i] != (uint8_t)(255u - i)) return 0;
    return 1;
}

/* —— 子项 2：mkfs + 文件 create/write/read/lookup —— */
static const char *g_names[3] = { "hello.txt", "readme", "data.bin" };
static const char *g_body[3]  = {
    "Hello, RISC-V filesystem!",
    "This file lives entirely in a RAM block device.",
    "0123456789-abcdefghij-block-mapped-content-end"
};

static int test_fs(void) {
    fs_mkfs();
    int inums[3];
    /* 建三个文件并写入内容 */
    for (int k = 0; k < 3; k++) {
        int inum = fs_create(g_names[k]);
        if (inum < 0) return 0;
        inums[k] = inum;
        uint32_t len = 0;
        while (g_body[k][len]) len++;
        if (fs_write((uint32_t)inum, g_body[k], len) != (int)len) return 0;
    }
    /* lookup 命中并读回内容逐字节一致 */
    for (int k = 0; k < 3; k++) {
        int got = fs_lookup(g_names[k]);
        if (got != inums[k]) return 0;
        uint8_t buf[BSIZE * 2];
        uint32_t len = 0;
        while (g_body[k][len]) len++;
        int n = fs_read((uint32_t)got, buf, sizeof(buf));
        if (n != (int)len) return 0;
        if (!mem_eq(buf, g_body[k], len)) return 0;
    }
    /* 查一个不存在的名字应失败 */
    if (fs_lookup("nope") != -1) return 0;
    return 1;
}

/* —— 子项 3：ls 列出全部文件名 —— */
static int test_ls(void) {
    int inums[16];
    char names[16][DIRSIZ];
    int n = fs_ls(inums, names, 16);
    if (n != 3) return 0;
    kputs("  ls /:\n");
    /* 每个建立的名字都应在列表里出现恰好一次 */
    for (int k = 0; k < 3; k++) {
        int seen = 0;
        for (int i = 0; i < n; i++)
            if (kstreq(names[i], g_names[k])) seen++;
        if (seen != 1) return 0;
    }
    for (int i = 0; i < n; i++) {
        kputs("    inode ");
        kputdec((uint64_t)(uint32_t)inums[i]);
        kputs("  ");
        kputs(names[i]);
        console_putchar('\n');
    }
    return 1;
}

void kmain(void) {
    kputs("\n[S07] block device + tiny in-kernel filesystem (RAM disk)\n");

    if (test_blockdev()) kputs("BLK_PASS\n");
    else                 kputs("BLK_done_but_mismatch\n");

    int fs_ok = test_fs();
    if (fs_ok) kputs("FS_PASS\n");
    else       kputs("FS_done_but_mismatch\n");

    if (fs_ok && test_ls()) kputs("LS_PASS\n");
    else                    kputs("LS_done_but_mismatch\n");

    if (test_blockdev() && fs_ok && test_ls())
        kputs("ALL_PASS\n");
}
