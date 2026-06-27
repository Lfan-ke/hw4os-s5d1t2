/* S8b · 物理帧分配 + SV39 三级页表（沿用 S5c，给定）。本实验不要求改这里。 */
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

uint64_t frame_used(void) { return frame_next; }

/* ===== ② 三级页表 walk + 建中间表 + 写叶 PTE（沿用 S5c，给定） ===== */
void map_one(uint64_t *root, uint64_t va, uint64_t pa, uint64_t flags) {
    uint64_t *table = root;
    for (int level = 2; level > 0; level--) {
        uint64_t vpn = (va >> (12 + 9 * level)) & 0x1FF;
        uint64_t pte = table[vpn];
        if (!(pte & PTE_V)) {
            uint64_t *next = (uint64_t *)frame_alloc();
            table[vpn] = (((uint64_t)next >> 12) << 10) | PTE_V; /* 非叶：只置 V */
            table = next;
        } else {
            table = (uint64_t *)(((pte >> 10) & 0xFFFFFFFFFFFUL) << 12);
        }
    }
    uint64_t vpn0 = (va >> 12) & 0x1FF;
    table[vpn0] = ((pa >> 12) << 10) | flags | PTE_V; /* 叶 PTE */
}

/* ===== ③ 查叶 PTE（给定）：缺中间表则返回 0，绝不解引用空表 ===== */
uint64_t pte_lookup(uint64_t *root, uint64_t va) {
    uint64_t *table = root;
    for (int level = 2; level > 0; level--) {
        uint64_t vpn = (va >> (12 + 9 * level)) & 0x1FF;
        uint64_t pte = table[vpn];
        if (!(pte & PTE_V)) return 0; /* 中间级缺失 → 整条未映射 */
        table = (uint64_t *)(((pte >> 10) & 0xFFFFFFFFFFFUL) << 12);
    }
    uint64_t vpn0 = (va >> 12) & 0x1FF;
    return table[vpn0]; /* 叶 PTE 值（低位含 V），未映射则为 0 */
}

/* ===== ④ 拆一条映射（给定）：清叶 PTE，使该 VA 再访问必缺页 ===== */
void unmap_one(uint64_t *root, uint64_t va) {
    uint64_t *table = root;
    for (int level = 2; level > 0; level--) {
        uint64_t vpn = (va >> (12 + 9 * level)) & 0x1FF;
        uint64_t pte = table[vpn];
        if (!(pte & PTE_V)) return; /* 本就没映射 */
        table = (uint64_t *)(((pte >> 10) & 0xFFFFFFFFFFFUL) << 12);
    }
    uint64_t vpn0 = (va >> 12) & 0x1FF;
    table[vpn0] = 0; /* 清叶 PTE（中间表/帧不回收，教学从简） */
}

/* ===== ⑤ 写 satp 开 SV39 + 屏障（沿用 S5c，给定） ===== */
void enable_paging(uint64_t *root) {
    uint64_t satp = (8UL << 60) | ((uint64_t)root >> 12);
    asm volatile("sfence.vma");
    asm volatile("csrw satp, %0" : : "r"(satp));
    asm volatile("sfence.vma");
}
