/* S5c · SV39 分页：PTE 位定义 / 帧分配 / 建表 / 开 satp 接口（给定）。 */
#ifndef S5C_PAGING_H
#define S5C_PAGING_H
#include <stdint.h>

/* —— SV39 页 —— */
#define PAGE_SIZE 4096UL

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

/* —— 物理帧分配器（paging.c 给定）：从静态帧池按页发放并清零 —— */
void *frame_alloc(void);

/* —— 建立一条映射：把虚拟页 va 映到物理页 pa，叶 PTE 取 flags（学生实现）—— */
void map_one(uint64_t *root, uint64_t va, uint64_t pa, uint64_t flags);

/* —— 开启 SV39：写 satp=(8<<60)|(root_ppn) + sfence.vma（学生实现）—— */
void enable_paging(uint64_t *root);

/* 读 satp（用于自检是否已进入 SV39 模式）。 */
static inline uint64_t r_satp(void) {
    uint64_t x;
    asm volatile("csrr %0, satp" : "=r"(x));
    return x;
}

#endif
