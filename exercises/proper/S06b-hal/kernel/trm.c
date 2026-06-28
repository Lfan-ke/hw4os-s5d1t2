/* S06b · TRM（Turing Machine / 最小计算）层实现（给定）。
 * 一个能“算”的最小机器只需三样东西：能输出(putch)、能停机(halt)、有一块内存(heap)。
 * 这里把它们架在 SBI 控制台与一块静态 RAM 上 —— 换个平台只需换这层。 */
#include "am.h"
#include "kernel.h"

/* 一块给程序自由使用的 RAM（本课没有分配器，heap 就是“一段可用内存”）。 */
#define HEAP_SIZE (64 * 1024)
static uint8_t heap_mem[HEAP_SIZE] __attribute__((aligned(16)));
Area heap = { heap_mem, heap_mem + HEAP_SIZE };

void putch(char ch) {
    console_putchar((unsigned char)ch); /* 经 SBI 控制台 */
}

void halt(int code) {
    (void)code;
    k_shutdown();      /* 经 SBI 关机 */
    for (;;) { }
}
