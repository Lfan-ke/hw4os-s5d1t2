/* S08b · 内核入口/测试驱动（给定）：建页表 → 开 SV39 → mmap 登记 → 按需调页验证。
 *
 * 判据：
 *   MMAP_PASS          —— mmap 后 VMA 已登记，但页表里这段 VA 仍无任何叶 PTE（lazy）。
 *   FAULT_HANDLED_PASS —— 访问触发缺页 → 按需映射 → 读回 == 写入值。
 *   LAZY_PASS          —— 只有被触碰的页才分配：缺页次数 == 被触碰页数，未碰的页仍未映射。
 *   UNMAP_PASS         —— munmap 后再访问 → 被识别为真错误缺页（不在任何 VMA）。
 */
#include "kernel.h"
#include "paging.h"
#include "vma.h"

/* mmap 区域：4 页，落在内核恒等区/MMIO 之外的 SV39 合法范围。 */
#define BASE_VA 0x0000000060000000UL
#define NPAGES  4
#define MAGIC   0xcafef00d00000000UL

/* 内核运行所需恒等映射区间：覆盖内核 text/data/bss + 帧池 + 栈。 */
#define KMEM_LO 0x0000000080000000UL
#define KMEM_HI 0x0000000080800000UL /* 8MB */
#define UART0   0x0000000010000000UL

static inline int page_mapped(uint64_t *root, uint64_t va) {
    return (pte_lookup(root, va) & PTE_V) != 0;
}

void kmain(void) {
    kputs("\n[S08b] mmap + demand paging\n");
    trap_init();

    /* 建 SV39 根表 + 恒等映射内核运行区（含 UART MMIO 页），然后开分页。 */
    uint64_t *root = (uint64_t *)frame_alloc();
    for (uint64_t pa = KMEM_LO; pa < KMEM_HI; pa += PAGE_SIZE)
        map_one(root, pa, pa, PTE_R | PTE_W | PTE_X);
    map_one(root, UART0, UART0, PTE_R | PTE_W);

    vma_init(root);
    enable_paging(root);
    if ((r_satp() >> 60) != 8) {
        kputs("PAGING_ON_MISS (satp 未进入 SV39，跳过后续)\n");
        return; /* 占位安全：未开分页就不碰裸 VA */
    }

    /* ===== ① mmap：登记 VMA，但不应映射任何页（lazy） ===== */
    uint64_t len = NPAGES * PAGE_SIZE;
    void *r = mmap(BASE_VA, len, PROT_READ | PROT_WRITE);
    int vma_ok = (r == (void *)BASE_VA) && (vma_find(BASE_VA) != 0);
    int none_mapped = 1;
    for (int i = 0; i < NPAGES; i++)
        if (page_mapped(root, BASE_VA + (uint64_t)i * PAGE_SIZE)) none_mapped = 0;
    int mmap_ok = vma_ok && none_mapped;
    if (mmap_ok) kputs("MMAP_PASS\n"); else kputs("MMAP_MISS\n");

    /* ===== ②③ 只触碰第 0、2 页：缺页 → 按需补帧 → 写入再读回 ===== */
    g_fault_count = 0;
    g_real_fault  = 0;

    volatile uint64_t *p0 = (volatile uint64_t *)(BASE_VA + 0 * PAGE_SIZE);
    volatile uint64_t *p2 = (volatile uint64_t *)(BASE_VA + 2 * PAGE_SIZE);
    *p0 = MAGIC + 0; /* 首写 → store 缺页 → 补页 → 重执行写成功 */
    *p2 = MAGIC + 2;
    uint64_t b0 = *p0, b2 = *p2; /* 读回（页已在，不再缺页） */

    int handled_ok = (b0 == MAGIC + 0) && (b2 == MAGIC + 2) && !g_real_fault;
    if (handled_ok) kputs("FAULT_HANDLED_PASS\n"); else kputs("FAULT_HANDLED_MISS\n");

    /* ===== ④ lazy：缺页次数 == 触碰页数(2)，未碰的 1、3 页仍未映射(故不占数据帧) ===== */
    int p1_unmapped = !page_mapped(root, BASE_VA + 1 * PAGE_SIZE);
    int p3_unmapped = !page_mapped(root, BASE_VA + 3 * PAGE_SIZE);
    int lazy_ok = (g_fault_count == 2) &&    /* 恰好 2 次按需缺页 == 被触碰页数 */
                  p1_unmapped && p3_unmapped; /* 未触碰的页无叶 PTE → 一帧未分 */
    if (lazy_ok) kputs("LAZY_PASS\n"); else kputs("LAZY_MISS\n");

    /* ===== ⑤ munmap：撤销后再访问 → 真错误缺页被识别 ===== */
    g_real_fault = 0;
    munmap(BASE_VA, len);
    volatile uint64_t t = *p0; /* 该页已被拆映射、VMA 已撤 → 真缺页 */
    (void)t;
    int unmap_ok = g_real_fault && (vma_find(BASE_VA) == 0);
    if (unmap_ok) kputs("UNMAP_PASS\n"); else kputs("UNMAP_MISS\n");

    if (mmap_ok && handled_ok && lazy_ok && unmap_ok) kputs("ALL_PASS\n");
}
