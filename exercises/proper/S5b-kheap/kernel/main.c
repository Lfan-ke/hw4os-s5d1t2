/* S5b · 内核入口/测试驱动（给定，勿改）：检验 kalloc/kfree 的分配、复用、合并。
 *
 * 三道关：
 *   ① 连续 alloc NB 块、写各自 magic：互不重叠 + 16 对齐 + 写读一致 → ALLOC_PASS
 *   ② free 中间一块，再 alloc 同样大小：应复用刚释放的同一地址      → REUSE_PASS
 *   ③ free 相邻两块应合并成大块：再 alloc 一个「单块装不下」的更大块，
 *      它只能落在合并出的大块起点（即被合并区首块的地址）          → COALESCE_PASS
 *   三关全过                                                         → ALL_PASS
 */
#include "kernel.h"
#include "kheap.h"

#define BS 64   /* 每块 payload 字节数（NB 块等大、连续，便于检验复用/合并） */
#define NB 4    /* 连续分配的块数 */

/* 16 字节对齐检查 */
static int aligned16(void *p) { return ((uint64_t)p & 15u) == 0; }

/* [a,a+la) 与 [b,b+lb) 是否重叠 */
static int overlap(unsigned char *a, size_t la, unsigned char *b, size_t lb) {
    return a < b + lb && b < a + la;
}

void kmain(void) {
    kputs("\n[S5b] kernel heap allocator (free-list + coalesce)\n");
    kheap_init();

    unsigned char *p[NB];
    int ok = 1;

    /* ---- Test 1: 连续 alloc / 对齐 / 不重叠 / 写读校验 ---- */
    for (int i = 0; i < NB; i++) {
        p[i] = (unsigned char *)kalloc(BS);
        if (!p[i] || !aligned16(p[i])) { ok = 0; break; }
    }
    for (int i = 0; ok && i < NB; i++)
        for (int j = i + 1; ok && j < NB; j++)
            if (overlap(p[i], BS, p[j], BS)) ok = 0;
    if (ok) {
        for (int i = 0; i < NB; i++)
            for (int k = 0; k < BS; k++)
                p[i][k] = (unsigned char)(0xA0 + i);          /* 写各自 magic */
        for (int i = 0; ok && i < NB; i++)
            for (int k = 0; k < BS; k++)
                if (p[i][k] != (unsigned char)(0xA0 + i)) ok = 0; /* 互不踩踏 */
    }
    if (ok) kputs("ALLOC_PASS\n"); else kputs("ALLOC_MISS\n");

    /* ---- Test 2: free 中间块再 alloc 同样大小 → 复用同地址 ---- */
    int reuse_ok = 0;
    if (ok) {
        unsigned char *mid = p[1];
        kfree(p[1]);
        unsigned char *q = (unsigned char *)kalloc(BS);
        if (q == mid) reuse_ok = 1;
        p[1] = q;   /* 回填，供下一测使用（地址应与原 p[1] 相同） */
    }
    if (reuse_ok) kputs("REUSE_PASS\n"); else kputs("REUSE_MISS\n");

    /* ---- Test 3: free 相邻两块 p[1],p[2] 应合并 → alloc 单块装不下的更大块 ---- */
    int coal_ok = 0;
    if (reuse_ok && p[1]) {
        unsigned char *base = p[1];   /* p[1] 与 p[2] 相邻，合并后大块起点 = p[1] */
        kfree(p[1]);
        kfree(p[2]);
        /* 请求 100 字节：> 单块 BS(64)，单个释放块装不下，必须靠合并出的大块。
         * 若已正确合并，first-fit 会从地址最低的合并块切出 → 返回 base；
         * 若没合并，两个 64 字节碎块都装不下，只能落到高地址的尾部 → 不等于 base。 */
        unsigned char *r = (unsigned char *)kalloc(100);
        if (r == base) coal_ok = 1;
    }
    if (coal_ok) kputs("COALESCE_PASS\n"); else kputs("COALESCE_MISS\n");

    if (ok && reuse_ok && coal_ok) kputs("ALL_PASS\n");
}
