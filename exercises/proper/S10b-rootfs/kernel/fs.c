/* S07 · 简易文件系统（易 easy-fs 抽象，全在 RAM 盘上）。
 *
 * 盘上布局（块为单位）：
 *   block 0           : 超级块 superblock
 *   block 1..N        : inode 区（每块 IPB=8 个 dinode）
 *   block data_start..: 数据区（bump 分配）
 *
 * inode 仅用直接块（direct）映射；目录是一串 32 字节目录项。
 * 参考解：ialloc / dir_lookup 已实现；学生版把这两处挖空。 */
#include "fs.h"

static struct superblock g_sb;  /* 缓存的超级块 */

/* —— inode 读写：按 inum 定位到 (块, 块内偏移) —— */
static void iread(uint32_t inum, struct dinode *di) {
    uint8_t b[BSIZE];
    uint32_t blk = g_sb.inode_start + inum / IPB;
    bread(blk, b);
    kmemcpy(di, b + (inum % IPB) * INODE_SIZE, sizeof(*di));
}

static void iwrite(uint32_t inum, const struct dinode *di) {
    uint8_t b[BSIZE];
    uint32_t blk = g_sb.inode_start + inum / IPB;
    bread(blk, b);
    kmemcpy(b + (inum % IPB) * INODE_SIZE, di, sizeof(*di));
    bwrite(blk, b);
}

static void sb_flush(void) {
    uint8_t b[BSIZE];
    kmemset(b, 0, BSIZE);
    kmemcpy(b, &g_sb, sizeof(g_sb));
    bwrite(0, b);
}

/* 数据块 bump 分配器：返回新数据块号，0 视为失败。 */
static uint32_t balloc(void) {
    if (g_sb.next_data >= g_sb.nblocks) return 0;
    uint32_t blk = g_sb.next_data++;
    sb_flush();
    uint8_t z[BSIZE];
    kmemset(z, 0, BSIZE);
    bwrite(blk, z);
    return blk;
}

/* =========================================================================
 * 学生填空点 1：分配一个空闲 inode。
 * 扫描 inode 表，找到 type==T_FREE 的，置成 type 写回，返回其 inum；满了返回 -1。
 * ========================================================================= */
int ialloc(uint32_t type) {
    struct dinode di;
    for (uint32_t inum = 0; inum < g_sb.ninodes; inum++) {
        iread(inum, &di);
        if (di.type == T_FREE) {
            kmemset(&di, 0, sizeof(di));
            di.type = type;
            di.size = 0;
            iwrite(inum, &di);
            return (int)inum;
        }
    }
    return -1;
}

/* =========================================================================
 * 学生填空点 2：在目录 dir_inum 里查找名为 name 的项。
 * 遍历目录的全部目录项（共 size/32 项），名字相等则返回其 inum；查无返回 -1。
 * ========================================================================= */
int dir_lookup(uint32_t dir_inum, const char *name) {
    struct dinode dir;
    iread(dir_inum, &dir);
    uint32_t nent = dir.size / sizeof(struct dirent);
    uint8_t b[BSIZE];
    struct dirent de;
    for (uint32_t i = 0; i < nent; i++) {
        uint32_t blk = dir.addrs[i / DPB];
        if (blk == 0) continue;
        bread(blk, b);
        kmemcpy(&de, b + (i % DPB) * sizeof(struct dirent), sizeof(de));
        if (kstreq(de.name, name)) return (int)de.inum;
    }
    return -1;
}

/* 往目录追加一条目录项（不查重，调用方保证名字唯一）。 */
static int dir_add(uint32_t dir_inum, const char *name, uint32_t inum) {
    struct dinode dir;
    iread(dir_inum, &dir);
    uint32_t idx = dir.size / sizeof(struct dirent);  /* 第 idx 项 */
    uint32_t bi  = idx / DPB;                          /* 落在第 bi 个直接块 */
    if (bi >= NDIRECT) return -1;
    if (dir.addrs[bi] == 0) {
        uint32_t nb = balloc();
        if (nb == 0) return -1;
        dir.addrs[bi] = nb;
    }
    struct dirent de;
    kmemset(&de, 0, sizeof(de));
    de.inum = inum;
    /* 拷贝名字（含结尾 0，截断到 DIRSIZ-1） */
    uint32_t k = 0;
    while (name[k] && k < DIRSIZ - 1) { de.name[k] = name[k]; k++; }
    de.name[k] = 0;

    uint8_t b[BSIZE];
    bread(dir.addrs[bi], b);
    kmemcpy(b + (idx % DPB) * sizeof(struct dirent), &de, sizeof(de));
    bwrite(dir.addrs[bi], b);

    dir.size += sizeof(struct dirent);
    iwrite(dir_inum, &dir);
    return 0;
}

/* —— mkfs：清盘 + 写超级块 + 建空根目录 —— */
void fs_mkfs(void) {
    /* 清整盘 */
    uint8_t z[BSIZE];
    kmemset(z, 0, BSIZE);
    for (uint32_t i = 0; i < NBLOCKS; i++) bwrite(i, z);

    /* 计算布局：inode 区从块 1 起，占 ceil(NINODES/IPB) 块 */
    uint32_t inode_blocks = (NINODES + IPB - 1) / IPB;
    g_sb.magic       = FS_MAGIC;
    g_sb.nblocks     = NBLOCKS;
    g_sb.ninodes     = NINODES;
    g_sb.inode_start = 1;
    g_sb.data_start  = 1 + inode_blocks;
    g_sb.next_data   = g_sb.data_start;
    sb_flush();

    /* 所有 inode 已被清零 = T_FREE。分配 inode 0 作根目录。 */
    int r = ialloc(T_DIR);
    (void)r; /* 根目录应得到 inum 0 */
}

/* —— 在根目录建文件 —— */
int fs_create(const char *name) {
    if (dir_lookup(ROOT_INO, name) >= 0) return -1; /* 重名 */
    int inum = ialloc(T_FILE);
    if (inum < 0) return -1;
    if (dir_add(ROOT_INO, name, (uint32_t)inum) < 0) return -1;
    return inum;
}

/* —— 写文件内容（覆盖式，按直接块分配）—— */
int fs_write(uint32_t inum, const void *data, uint32_t len) {
    struct dinode in;
    iread(inum, &in);
    if (in.type != T_FILE) return -1;
    const uint8_t *p = (const uint8_t *)data;
    uint32_t nblk = (len + BSIZE - 1) / BSIZE;
    if (nblk > NDIRECT) return -1;
    for (uint32_t i = 0; i < nblk; i++) {
        if (in.addrs[i] == 0) {
            uint32_t nb = balloc();
            if (nb == 0) return -1;
            in.addrs[i] = nb;
        }
        uint8_t b[BSIZE];
        kmemset(b, 0, BSIZE);
        uint32_t chunk = len - i * BSIZE;
        if (chunk > BSIZE) chunk = BSIZE;
        kmemcpy(b, p + i * BSIZE, chunk);
        bwrite(in.addrs[i], b);
    }
    in.size = len;
    iwrite(inum, &in);
    return (int)len;
}

/* —— 读回文件内容 —— */
int fs_read(uint32_t inum, void *buf, uint32_t max) {
    struct dinode in;
    iread(inum, &in);
    if (in.type != T_FILE) return -1;
    uint32_t len = in.size;
    if (len > max) len = max;
    uint8_t *p = (uint8_t *)buf;
    uint32_t nblk = (len + BSIZE - 1) / BSIZE;
    for (uint32_t i = 0; i < nblk; i++) {
        uint8_t b[BSIZE];
        bread(in.addrs[i], b);
        uint32_t chunk = len - i * BSIZE;
        if (chunk > BSIZE) chunk = BSIZE;
        kmemcpy(p + i * BSIZE, b, chunk);
    }
    return (int)len;
}

/* —— 根目录查名 —— */
int fs_lookup(const char *name) {
    return dir_lookup(ROOT_INO, name);
}

/* —— 列根目录（跳过 "." 占位）；返回项数 —— */
int fs_ls(int *out_inums, char out_names[][DIRSIZ], int max) {
    struct dinode dir;
    iread(ROOT_INO, &dir);
    uint32_t nent = dir.size / sizeof(struct dirent);
    uint8_t b[BSIZE];
    struct dirent de;
    int n = 0;
    for (uint32_t i = 0; i < nent && n < max; i++) {
        uint32_t blk = dir.addrs[i / DPB];
        if (blk == 0) continue;
        bread(blk, b);
        kmemcpy(&de, b + (i % DPB) * sizeof(struct dirent), sizeof(de));
        out_inums[n] = (int)de.inum;
        kmemcpy(out_names[n], de.name, DIRSIZ);
        n++;
    }
    return n;
}
