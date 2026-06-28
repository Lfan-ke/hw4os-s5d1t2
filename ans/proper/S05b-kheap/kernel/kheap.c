/* S05b · 内核堆分配器（参考实现）：静态 64KB 字节池 + 带 size 头的空闲块链表。
 *
 * 设计（first-fit + 地址有序空闲链 + 释放即合并）：
 *   - 每个块（无论占用/空闲）开头是一个 16 字节头 block_t{ size, next }。
 *     size = 本块「连头带尾」的总字节数（恒为 16 对齐）；payload 在 +HDR 处、16 对齐。
 *   - 空闲块串成一条**按地址升序**的单链表 free_list；next 仅当块空闲时有效，
 *     占用块的这块内存就还给用户当 payload 用了。
 *   - kalloc：first-fit 找第一个够大的空闲块；够大就**分裂**出尾部剩余块，否则整块取走。
 *   - kfree：按地址插回链表，再与**前驱/后继**相邻空闲块**合并(coalesce)**，对抗外部碎片。
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

    /* first-fit：用二级指针 link 指向「指向当前块的那个指针」，便于原地摘除。 */
    block_t **link = &free_list;
    for (block_t *b = free_list; b; link = &b->next, b = b->next) {
        if (b->size < total) continue;                 /* 装不下，下一块 */
        if (b->size >= total + HDR + ALIGN) {
            /* 够大：分裂——在 b 的尾部留一个剩余空闲块，b 收缩成 total。 */
            block_t *rem = (block_t *)((unsigned char *)b + total);
            rem->size = b->size - total;
            rem->next = b->next;
            b->size   = total;
            *link = rem;                               /* 用 rem 顶替 b 在链上的位置 */
        } else {
            *link = b->next;                           /* 不值得分裂：整块摘走 */
        }
        return (unsigned char *)b + HDR;               /* 返回 payload 地址 */
    }
    return 0;   /* 池满 */
}

void kfree(void *p) {
    if (!p) return;
    block_t *b = (block_t *)((unsigned char *)p - HDR);

    /* 1) 按地址升序找到插入点：prev < b < cur */
    block_t *prev = 0, *cur = free_list;
    while (cur && cur < b) { prev = cur; cur = cur->next; }
    b->next = cur;
    if (prev) prev->next = b; else free_list = b;

    /* 2) 与后继合并：b 的尾巴正好顶到 cur 的头 → b 吞掉 cur */
    if (cur && (unsigned char *)b + b->size == (unsigned char *)cur) {
        b->size += cur->size;
        b->next  = cur->next;
    }
    /* 3) 与前驱合并：prev 的尾巴正好顶到 b 的头 → prev 吞掉 b */
    if (prev && (unsigned char *)prev + prev->size == (unsigned char *)b) {
        prev->size += b->size;
        prev->next  = b->next;
    }
}
