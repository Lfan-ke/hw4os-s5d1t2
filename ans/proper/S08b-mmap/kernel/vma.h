/* S08b · 虚拟内存区域(VMA) + mmap/munmap + 按需调页接口。 */
#ifndef S8B_VMA_H
#define S8B_VMA_H
#include <stdint.h>

/* —— mmap 保护位（映射到叶 PTE 的 R/W/X）—— */
#define PROT_READ  (1UL << 0)
#define PROT_WRITE (1UL << 1)
#define PROT_EXEC  (1UL << 2)

/* 一个 VMA：登记「这段 VA 是合法的、将来缺页就给它补帧」，但 mmap 时不分配物理页。 */
#define MAX_VMA 8
struct vma {
    uint64_t start; /* 区间起始 VA（页对齐） */
    uint64_t len;   /* 字节长度（页对齐） */
    uint64_t prot;  /* PROT_* 权限 */
    int      used;  /* 槽位是否在用 */
};

/* 绑定根页表 + 清空 VMA 表与计数（harness 开分页后调用一次）。 */
void vma_init(uint64_t *root);

/* —— 学生实现 —— */
void *mmap(uint64_t addr, uint64_t len, uint64_t prot); /* 只登记 VMA（lazy），返回 addr */
int   vma_handle_fault(uint64_t fault_va);              /* 缺页：查 VMA → 命中补帧映射；1=已处理 0=真错误 */

/* —— 给定 —— */
void        munmap(uint64_t addr, uint64_t len);        /* 撤销 VMA + 拆掉已映射的页 */
struct vma *vma_find(uint64_t va);                      /* 查覆盖 va 的 VMA；无则 NULL */

/* harness 与 trap.c 共享的统计量（定义在 trap.c）。 */
extern volatile int      g_fault_count; /* 累计「被服务的按需缺页」次数 */
extern volatile int      g_real_fault;  /* 是否发生过「不在任何 VMA」的真错误缺页 */
extern volatile uint64_t g_real_cause;  /* 最近一次真错误缺页的 scause */

#endif
