/* S07 · 块设备 + 内核内简易文件系统（RAM 盘）：共享声明。 */
#ifndef S07_FS_H
#define S07_FS_H
#include <stdint.h>

/* —— 块设备几何 —— */
#define BSIZE    512u           /* 每块字节数 */
#define NBLOCKS  128u           /* RAM 盘总块数 */

/* —— 文件系统布局 —— */
#define FS_MAGIC     0x53374653u /* "S7FS" */
#define NINODES      32u         /* inode 总数 */
#define INODE_SIZE   64u         /* 每个 dinode 占 64 字节（含 14 个直接块） */
#define IPB          (BSIZE / INODE_SIZE)  /* 每块容纳的 inode 数 = 8 */
#define NDIRECT      14u         /* 每个 inode 的直接块指针数 */
#define DIRSIZ       28u         /* 目录项文件名最大长度（含结尾 0） */
#define DPB          (BSIZE / 32u) /* 每块容纳的目录项数 = 16（每项 32B） */
#define ROOT_INO     0u          /* 根目录 inode 号 */

/* inode 类型 */
#define T_FREE  0u
#define T_FILE  1u
#define T_DIR   2u

/* 磁盘 inode（精确 64 字节）。 */
struct dinode {
    uint32_t type;            /* T_FREE / T_FILE / T_DIR */
    uint32_t size;            /* 字节数（目录则为目录项总字节数） */
    uint32_t addrs[NDIRECT];  /* 直接块号；0 表示未分配 */
};

/* 超级块（落在 block 0，远小于一块）。 */
struct superblock {
    uint32_t magic;
    uint32_t nblocks;
    uint32_t ninodes;
    uint32_t inode_start;   /* inode 区起始块号 */
    uint32_t data_start;    /* 数据区起始块号 */
    uint32_t next_data;     /* 下一个可分配数据块（bump 分配器） */
};

/* 目录项（精确 32 字节）。 */
struct dirent {
    uint32_t inum;
    char     name[DIRSIZ];
};

/* —— 块设备接口（blockdev.c）—— */
void bread(uint32_t blk, void *buf);
void bwrite(uint32_t blk, const void *buf);

/* —— 极简内存工具（blockdev.c 提供，freestanding 无 libc）—— */
void *kmemcpy(void *dst, const void *src, uint32_t n);
void *kmemset(void *dst, int c, uint32_t n);
int   kstreq(const char *a, const char *b);   /* 以 0 结尾、相等返回 1 */

/* —— 文件系统接口（fs.c）—— */
void    fs_mkfs(void);                                  /* 初始化整盘 */
int     fs_create(const char *name);                    /* 在根目录建文件，返回 inum 或 -1 */
int     fs_write(uint32_t inum, const void *data, uint32_t len); /* 写文件内容 */
int     fs_read(uint32_t inum, void *buf, uint32_t max); /* 读回内容，返回字节数 */
int     fs_lookup(const char *name);                    /* 根目录查名，返回 inum 或 -1 */
int     fs_ls(int *out_inums, char out_names[][DIRSIZ], int max); /* 列根目录，返回项数 */

/* 学生填空点：inode 分配与目录查找（见 README）。 */
int     ialloc(uint32_t type);                          /* 分配空闲 inode */
int     dir_lookup(uint32_t dir_inum, const char *name);/* 在目录里查名 */

#endif
