/* 正经赛道共享内核头：声明各实验/common 提供的最小接口。 */
#ifndef OSLAB_KERNEL_H
#define OSLAB_KERNEL_H
#include <stdint.h>

/* —— SBI 层（S1 由学生实现；后续阶段由 common/sbi 提供）—— */
long sbi_call(long eid, long fid, long a0, long a1, long a2);
void console_putchar(int c);
void k_shutdown(void);

/* —— 控制台辅助（common/kernel/console.c 提供，调用 console_putchar）—— */
void kputs(const char *s);
void kputhex(uint64_t x);
void kputdec(uint64_t x);

/* —— 内核入口（各实验提供 kmain，作为测试驱动）—— */
void kmain(void);

/* 链接脚本符号 */
extern char sbss[], ebss[], skernel[], ekernel[];

#endif
