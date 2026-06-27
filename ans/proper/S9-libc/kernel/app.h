/* S9 · 内核侧共享声明（承接 S8 的 U 态 + syscall 骨架）。
 * 本实验把用户侧从「裸 ecall」升级为「有 libc 的用户程序」：
 *   crt0(user_start) -> main() -> printf/malloc -> write/exit 系统调用。 */
#ifndef OSLAB_S9_APP_H
#define OSLAB_S9_APP_H
#include <stdint.h>
#include "kernel.h"

/* uentry.S：进入 U 态 / 从 syscall 长跳回内核（与 S8 同）。 */
void run_user(uint64_t entry, uint64_t ustack); /* 清 sstatus.SPP=0、置 sepc/sp 后 sret */
void return_to_kernel(void);                     /* 恢复 kctx 后 ret 回 kmain（longjmp 风） */

/* 内核 callee-saved 保存区：ra,sp,s0..s11（共 14 槽）。 */
extern uint64_t kctx[14];

/* crt0.S：U 态程序的 C 运行时入口（_start 之于普通进程）。 */
void user_start(void);

/* syscall.c：分发。 */
long do_syscall(long n, long a0, long a1, long a2);

/* main.c：进程退出状态。 */
extern volatile long g_exit_code;
extern volatile long g_proc_done;

/* 系统调用号（取真实 RV64 Linux ABI 号）。libc 与内核必须一致。 */
#define SYS_WRITE 64
#define SYS_EXIT  93

/* U 态 ecall 的 scause code。 */
#define SCAUSE_U_ECALL 8UL

#endif
