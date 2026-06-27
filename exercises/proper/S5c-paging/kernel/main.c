/* S5c · 内核入口/测试驱动（给定）：建页表 → 开 SV39 → 映射新页 → 触发缺页。 */
#include "kernel.h"
#include "paging.h"

extern volatile int g_fault; /* trap.c：缺页发生标志 */

/* 被映射/被测试的虚拟地址（均为 SV39 39 位范围内、与内核恒等区不重叠）。 */
#define MAP_VA   0x0000000050000000UL /* 新映射到一个全新物理帧 */
#define FAULT_VA 0x0000000040000000UL /* 故意不映射 → 应触发缺页 */
#define MAGIC    0x123456789abcdef0UL

/* 内核运行所需恒等映射区间：覆盖内核 text/data/bss + 帧池 + 栈。 */
#define KMEM_LO  0x0000000080000000UL
#define KMEM_HI  0x0000000080800000UL /* 8MB，足以覆盖整个内核镜像 */
#define UART0    0x0000000010000000UL /* SBI 走 ecall，但保留 UART 页便于扩展 */

void kmain(void) {
    kputs("\n[S5c] SV39 virtual memory\n");
    trap_init();

    /* ②③ 建 SV39 根页表 + 恒等映射内核运行区（含 UART MMIO 页）。 */
    uint64_t *root = (uint64_t *)frame_alloc();
    for (uint64_t pa = KMEM_LO; pa < KMEM_HI; pa += PAGE_SIZE)
        map_one(root, pa, pa, PTE_R | PTE_W | PTE_X); /* 内核需可执行 */
    map_one(root, UART0, UART0, PTE_R | PTE_W);

    /* ④ 写 satp 开 SV39；若仍存活说明恒等映射保住了 PC/栈。 */
    enable_paging(root);
    if ((r_satp() >> 60) == 8) {
        kputs("PAGING_ON_PASS\n");
    } else {
        kputs("PAGING_ON_MISS (satp 未进入 SV39，跳过后续)\n");
        return; /* 占位安全：未开分页则不做 VA 测试，避免裸地址乱访问 */
    }

    /* ⑤ 分配新帧、映射新虚拟页、经 VA 写入再读回。 */
    void *pa = frame_alloc();
    map_one(root, MAP_VA, (uint64_t)pa, PTE_R | PTE_W);
    asm volatile("sfence.vma"); /* 新增映射后刷 TLB */

    g_fault = 0;
    *(volatile uint64_t *)MAP_VA = MAGIC; /* 经新 VA 写 */
    if (!g_fault) kputs("MAP_PASS\n"); else kputs("MAP_BAD\n");

    uint64_t back = *(volatile uint64_t *)MAP_VA; /* 经新 VA 读回 */
    /* 同时经物理帧（恒等映射）读回，确认 VA→PA 翻译确实落到该帧。 */
    int translate_ok = (back == MAGIC) && (*(volatile uint64_t *)pa == MAGIC);
    if (translate_ok) kputs("TRANSLATE_PASS\n"); else kputs("TRANSLATE_BAD\n");

    /* ⑥ 访问未映射 VA → 应缺页，trap_handler 捕获并安全恢复。 */
    g_fault = 0;
    volatile uint64_t t = *(volatile uint64_t *)FAULT_VA;
    (void)t;
    if (g_fault) kputs("FAULT_PASS\n"); else kputs("FAULT_MISS\n");

    if (translate_ok && g_fault) kputs("ALL_PASS\n");
}
