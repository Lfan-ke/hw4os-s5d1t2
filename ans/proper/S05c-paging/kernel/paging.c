/* S05c · 物理帧分配 + SV39 三级页表建立 + 开启分页（参考解）。 */
#include "kernel.h"
#include "paging.h"

/* ===== ① 极简物理帧分配器（给定） ===== */
/* 一块 4KB 对齐的静态帧池（位于 .bss，链接在内核镜像内 → 会被恒等映射覆盖）。 */
#define NFRAMES 256
static uint8_t frame_pool[NFRAMES * PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
static uint64_t frame_next = 0;

/* 取下一空闲帧、清零、返回其物理地址（恒等映射下物理地址==该指针值）。 */
void *frame_alloc(void) {
    if (frame_next >= NFRAMES) return 0; /* 池耗尽 */
    uint8_t *f = frame_pool + frame_next * PAGE_SIZE;
    frame_next++;
    for (uint64_t i = 0; i < PAGE_SIZE; i++) f[i] = 0;
    return f;
}

/* ===== ② 三级页表 walk + 建中间表 + 写叶 PTE（参考解） ===== */
/* SV39：39 位 VA = [38:30]vpn2 | [29:21]vpn1 | [20:12]vpn0 | [11:0]offset。
 * 每级页表 512 项、每项 8 字节、恰好一页。PTE 的 PPN 在 [53:10]。 */
void map_one(uint64_t *root, uint64_t va, uint64_t pa, uint64_t flags) {
    uint64_t *table = root;
    /* 走前两级（level 2 → 1），缺则用 frame_alloc 建中间表（非叶 PTE：只置 V）。 */
    for (int level = 2; level > 0; level--) {
        uint64_t vpn = (va >> (12 + 9 * level)) & 0x1FF;
        uint64_t pte = table[vpn];
        if (!(pte & PTE_V)) {
            uint64_t *next = (uint64_t *)frame_alloc();
            table[vpn] = (((uint64_t)next >> 12) << 10) | PTE_V;
            table = next;
        } else {
            /* 取出 PPN（[53:10]，44 位）还原下一级页表物理地址。 */
            table = (uint64_t *)(((pte >> 10) & 0xFFFFFFFFFFFUL) << 12);
        }
    }
    /* 第三级：写叶 PTE（带 R/W/X 等权限 + V）。 */
    uint64_t vpn0 = (va >> 12) & 0x1FF;
    table[vpn0] = ((pa >> 12) << 10) | flags | PTE_V;
}

/* ===== ④ 写 satp 开 SV39 + 屏障（参考解） ===== */
void enable_paging(uint64_t *root) {
    uint64_t satp = (8UL << 60) | ((uint64_t)root >> 12); /* MODE=8(SV39) | 根表 PPN */
    asm volatile("sfence.vma");                            /* 换页表前清 TLB */
    asm volatile("csrw satp, %0" : : "r"(satp));           /* 立即生效：靠恒等映射继续跑 */
    asm volatile("sfence.vma");
}
