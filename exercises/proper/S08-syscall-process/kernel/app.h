/* S08 · 本实验内部共享声明（U 态入口、syscall、内核保存区）。 */
#ifndef OSLAB_S8_APP_H
#define OSLAB_S8_APP_H
#include <stdint.h>
#include "kernel.h"

/* uentry.S 提供：进入 U 态 / 从 syscall 长跳回内核 */
void run_user(uint64_t entry, uint64_t ustack); /* 设 sstatus.SPP=0、sepc、sp 后 sret */
void return_to_kernel(void);                     /* 恢复 kctx 后 ret 回 kmain（longjmp 风） */

/* 内核 callee-saved 保存区：ra,sp,s0..s11（共 14 槽），run_user 存、return_to_kernel 取 */
extern uint64_t kctx[14];

/* user.c 提供：嵌入内核的 U 态程序 */
void user_main(void);

/* syscall.c 提供：分发与处理 */
long do_syscall(long n, long a0, long a1, long a2);

/* main.c 提供：进程退出状态 */
extern volatile long g_exit_code;
extern volatile long g_proc_done;

/* 系统调用号（取真实 RV64 Linux ABI 号以保持 GNU 规范味） */
#define SYS_WRITE 64
#define SYS_EXIT  93

/* U 态 ecall 的 scause code */
#define SCAUSE_U_ECALL 8UL

#endif
