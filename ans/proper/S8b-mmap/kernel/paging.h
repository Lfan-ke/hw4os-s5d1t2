/* S8b · SV39 分页基础设施（沿用 S5c，给定）：PTE 位 / 帧分配 / 建表 / 查表 / 拆映射 / 开 satp。 */
#ifndef S8B_PAGING_H
#define S8B_PAGING_H
#include <stdint.h>

/* —— SV39 页 —— */
#define PAGE_SIZE 4096UL
#define PAGE_MASK (PAGE_SIZE - 1)
#define PAGE_DOWN(x) ((x) & ~PAGE_MASK) /* 向下对齐到页边界 */

/* —— PTE 标志位（低 8 位）—— */
#define PTE_V (1UL << 0) /* Valid：该 PTE 有效 */
#define PTE_R (1UL << 1) /* Readable */
#define PTE_W (1UL << 2) /* Writable */
#define PTE_X (1UL << 3) /* eXecutable */
#define PTE_U (1UL << 4) /* User 可访问 */
#define PTE_G (1UL << 5) /* Global */
#define PTE_A (1UL << 6) /* Accessed */
#define PTE_D (1UL << 7) /* Dirty */
/* R=W=X 全 0 的有效 PTE 是「指向下一级页表」的非叶节点；否则是叶 PTE。 */

/* —— 物理帧分配器（给定）：从静态帧池按页发放并清零 —— */
void *frame_alloc(void);
uint64_t frame_used(void); /* 已发放帧数（用于自检按需调页省内存） */

/* —— 建立/拆除一条映射、查叶 PTE（给定，沿用 S5c map_one）—— */
void     map_one(uint64_t *root, uint64_t va, uint64_t pa, uint64_t flags);
void     unmap_one(uint64_t *root, uint64_t va);     /* 清叶 PTE（munmap 用） */
uint64_t pte_lookup(uint64_t *root, uint64_t va);    /* 返回叶 PTE 值；0=未映射 */

/* —— 开启 SV39：写 satp=(8<<60)|root_ppn + sfence.vma（给定）—— */
void enable_paging(uint64_t *root);

/* 读 satp（用于自检是否已进入 SV39 模式）。 */
static inline uint64_t r_satp(void) {
    uint64_t x;
    asm volatile("csrr %0, satp" : "=r"(x));
    return x;
}

#endif
