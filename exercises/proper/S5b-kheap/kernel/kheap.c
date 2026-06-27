/* S5b · 内核堆分配器（学生填空版）：静态 64KB 字节池 + 带 size 头的空闲块链表。
 *
 * 设计（first-fit + 地址有序空闲链 + 释放即合并）：
 *   - 每个块（无论占用/空闲）开头是一个 16 字节头 block_t{ size, next }。
 *     size = 本块「连头带尾」的总字节数（恒为 16 对齐）；payload 在 +HDR 处、16 对齐。
 *   - 空闲块串成一条**按地址升序**的单链表 free_list；next 仅当块空闲时有效。
 *
 * 你要补两处 // TODO：kalloc 的「找/切空闲块」与 kfree 的「插回 + 相邻合并」。
 * 给定部分：block_t、heap_pool、free_list、kheap_init、align_up、对齐常量。
 */
#include "kernel.h"
#include "kheap.h"

#define HEAP_SIZE (64 * 1024)   /* 静态池大小（字节），16 的整数倍 */
#define ALIGN     16            /* payload 对齐粒度 */

/* 块头：size=本块总字节（含头，16 对齐）；next=下一个空闲块（地址升序，空闲时有效）。 */
typedef struct block {
    size_t        size;
    struct block *next;
} block_t;

#define HDR ((size_t)sizeof(block_t))   /* =16，正好等于 ALIGN：payload 自然 16 对齐 */

static unsigned char heap_pool[HEAP_SIZE] __attribute__((aligned(ALIGN)));
static block_t      *free_list;         /* 地址升序的空闲块链表头 */

static size_t align_up(size_t n, size_t a) { return (n + a - 1) & ~(a - 1); }

void kheap_init(void) {
    block_t *b = (block_t *)heap_pool;
    b->size = HEAP_SIZE;     /* 起步：整池是一个大空闲块 */
    b->next = 0;
    free_list = b;
}

void *kalloc(size_t n) {
    if (n == 0) return 0;
    size_t total = HDR + align_up(n, ALIGN);   /* 头 + 对齐后的 payload */

    /* TODO: first-fit 找/切一个空闲块。
     *   遍历 free_list（地址有序），找第一个 b->size >= total 的块 b：
     *     - 若 b->size >= total + HDR + ALIGN（剩余够装下一个最小块）：分裂——
     *       在 (char*)b + total 处新建剩余空闲块 rem（rem->size = b->size - total,
     *       rem->next = b->next），把 b->size 收缩成 total，并用 rem 顶替 b 在链上的位置。
     *     - 否则：整块取走（把 b 从链表摘除）。
     *   从链表摘除/顶替时，用一个 prev 指针或二级指针 block_t **link 指向「指向当前块的那个指针」。
     *   返回 (unsigned char*)b + HDR（payload 地址）。找不到则返回 0（池满）。
     */
    (void)total;
    return 0;   /* 占位：未实现 → 分配失败，harness 报 ALLOC_MISS（不崩） */
}

void kfree(void *p) {
    if (!p) return;
    block_t *b = (block_t *)((unsigned char *)p - HDR);

    /* TODO: 把 b 插回 free_list（按地址升序），并与相邻空闲块合并(coalesce)。
     *   1) 按地址升序找到插入点：prev < b < cur；把 b 接进去
     *      （b->next = cur；prev 存在则 prev->next = b，否则 free_list = b）。
     *   2) 与后继合并：若 (unsigned char*)b + b->size == (unsigned char*)cur，
     *      则 b 吞掉 cur（b->size += cur->size; b->next = cur->next）。
     *   3) 与前驱合并：若 prev 且 (unsigned char*)prev + prev->size == (unsigned char*)b，
     *      则 prev 吞掉 b（prev->size += b->size; prev->next = b->next）。
     *   未实现时本函数什么都不做（内存泄漏但不崩）——REUSE/COALESCE 会 *_MISS。
     */
    (void)b;
}
