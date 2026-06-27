/* S5b · 内核堆分配器接口（rcore ch4 风：静态字节池上的 alloc/free）。 */
#ifndef OSLAB_KHEAP_H
#define OSLAB_KHEAP_H
#include <stddef.h>

/* 在静态池上建立初始的「一个大空闲块」。kmain 启动时调一次。 */
void  kheap_init(void);

/* 分配至少 n 字节，返回 16 字节对齐的指针；失败（池满）返回 0。 */
void *kalloc(size_t n);

/* 释放 kalloc 返回的指针；与相邻空闲块合并(coalesce)。p==0 时为空操作。 */
void  kfree(void *p);

#endif
