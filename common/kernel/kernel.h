/* 正经赛道共享内核头：声明各实验/common 提供的最小接口。 */
#ifndef OSLAB_KERNEL_H
#define OSLAB_KERNEL_H
#include <stdint.h>

/* —— SBI 层（S1 由学生实现；后续阶段由 common/sbi 提供）—— */
long sbi_call(long eid, long fid, long a0, long a1, long a2);
void console_putchar(int c);
void k_shutdown(void);
void sbi_set_timer(uint64_t t);

/* —— 时钟（common/timer.c）—— */
uint64_t get_time(void);
void set_next_trigger(void);

/* —— trap（S2+）—— */
struct TrapContext {
    uint64_t x[32];   /* 通用寄存器 x0..x31（x0/x2 槽位保留） */
    uint64_t sstatus;
    uint64_t sepc;
};
void __alltraps(void);        /* common/trap.S */
void trap_init(void);         /* 实验提供：设 stvec */
void trap_handler(struct TrapContext *ctx); /* 实验提供：分发 */

/* —— 控制台辅助（common/kernel/console.c 提供，调用 console_putchar）—— */
void kputs(const char *s);
void kputhex(uint64_t x);
void kputdec(uint64_t x);

/* —— 内核入口（各实验提供 kmain，作为测试驱动）—— */
void kmain(void);

/* 链接脚本符号 */
extern char sbss[], ebss[], skernel[], ekernel[];

#endif
