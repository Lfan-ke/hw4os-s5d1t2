/* S5e · 内存管理（给定）：物理帧分配 + 引用计数 + SV39 建表/翻译 + 内核恒等映射。
 * 复用 S5c 的 map_one 思路，新增：va2pa（走表）、引用计数（CoW 共享）、map_kernel。 */
#include "kernel.h"
#include "proc.h"

/* ===== 物理帧池（.bss，落在内核镜像内 → 被 map_kernel 恒等映射覆盖） ===== */
#define NFRAMES 256
static uint8_t frame_pool[NFRAMES * PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
static uint64_t frame_next = 0;
static uint8_t refcnt[NFRAMES]; /* 每帧引用计数：CoW 共享时 >1 */

static int pa_index(uint64_t pa) {
    return (int)((pa - (uint64_t)frame_pool) / PAGE_SIZE);
}

/* 取下一空闲帧、清零、引用计数置 1、返回物理基址（恒等映射下 == 指针值）。 */
void *frame_alloc(void) {
    if (frame_next >= NFRAMES) {
        kputs("OOM_FRAME\n");
        return 0;
    }
    int idx = (int)frame_next;
    uint8_t *f = frame_pool + frame_next * PAGE_SIZE;
    frame_next++;
    for (uint64_t i = 0; i < PAGE_SIZE; i++) f[i] = 0;
    refcnt[idx] = 1;
    return f;
}

void ref_inc(uint64_t pa) { refcnt[pa_index(pa)]++; }
void ref_dec(uint64_t pa) { if (refcnt[pa_index(pa)]) refcnt[pa_index(pa)]--; }
int  ref_get(uint64_t pa) { return refcnt[pa_index(pa)]; }

void kmemcpy(void *d, const void *s, uint64_t n) {
    uint8_t *dd = (uint8_t *)d;
    const uint8_t *ss = (const uint8_t *)s;
    for (uint64_t i = 0; i < n; i++) dd[i] = ss[i];
}

/* ===== SV39 三级 walk：建中间表 + 写叶 PTE（与 S5c 同构）===== */
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
    table[vpn0] = ((pa >> 12) << 10) | flags | PTE_V; /* 叶 PTE：权限 + V */
}

/* 走表得 VA 对应物理页基址；任一级无效则返回 0。 */
uint64_t va2pa(uint64_t *root, uint64_t va) {
    uint64_t *table = root;
    for (int level = 2; level > 0; level--) {
        uint64_t vpn = (va >> (12 + 9 * level)) & 0x1FF;
        uint64_t pte = table[vpn];
        if (!(pte & PTE_V)) return 0;
        table = (uint64_t *)(((pte >> 10) & 0xFFFFFFFFFFFUL) << 12);
    }
    uint64_t vpn0 = (va >> 12) & 0x1FF;
    uint64_t pte = table[vpn0];
    if (!(pte & PTE_V)) return 0;
    return ((pte >> 10) & 0xFFFFFFFFFFFUL) << 12;
}

/* 给 root 装上「内核运行所需」的两套映射，区间 KMEM_LO .. ceil(ekernel)：
 *   ① 恒等、无 U（R|W|X）：S 态在此取指跑内核/陷入、按数据访问页表与帧池；
 *   ② 用户别名、带 U（R|X|U）：U 态在此取指跑嵌入镜像的用户代码（同一物理页）。
 * 二者指向同一物理页，只是 VA 与权限不同——绕开「S 态不能从 U 页取指」的硬约束。 */
void map_kernel(uint64_t *root) {
    uint64_t hi = ((uint64_t)ekernel + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    for (uint64_t pa = KMEM_LO; pa < hi; pa += PAGE_SIZE) {
        map_one(root, pa, pa, PTE_R | PTE_W | PTE_X);                 /* 内核：无 U */
        map_one(root, USER_BASE + (pa - KMEM_LO), pa, PTE_R | PTE_X | PTE_U); /* 用户别名：带 U */
    }
}
