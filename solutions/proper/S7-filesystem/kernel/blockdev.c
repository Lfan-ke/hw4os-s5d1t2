/* S7 · 块设备：内核静态数组当作 RAM 盘，bread/bwrite 按块搬运（给定）。
 * 真实系统里这层背后是 virtio-blk MMIO；此处用一片内存模拟，接口完全一致：
 * 上层 FS 只依赖「按块读/写」，不关心底层是磁盘还是内存。 */
#include "fs.h"

/* RAM 盘：NBLOCKS 块、每块 BSIZE 字节。 */
static uint8_t g_disk[NBLOCKS][BSIZE];

void bread(uint32_t blk, void *buf) {
    if (blk >= NBLOCKS) { kmemset(buf, 0, BSIZE); return; }
    kmemcpy(buf, g_disk[blk], BSIZE);
}

void bwrite(uint32_t blk, const void *buf) {
    if (blk >= NBLOCKS) return;
    kmemcpy(g_disk[blk], buf, BSIZE);
}

/* —— freestanding 极简工具；加属性避免被优化成对自身的 memcpy/memset 递归调用 —— */
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *kmemcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *kmemset(void *dst, int c, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    for (uint32_t i = 0; i < n; i++) d[i] = (uint8_t)c;
    return dst;
}

int kstreq(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *a == *b;
}
