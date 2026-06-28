/* S10 · 本实验内部共享声明（U 态入口、syscall、ulib 丐版 libc、三个用户程序）。 */
#ifndef OSLAB_S10_APP_H
#define OSLAB_S10_APP_H
#include <stdint.h>
#include "kernel.h"

/* uentry.S 提供：进入 U 态 / 从 syscall 长跳回内核（同 S08）。 */
void run_user(uint64_t entry, uint64_t ustack); /* 设 sstatus.SPP=0、sepc、sp 后 sret */
void return_to_kernel(void);                     /* 恢复 kctx 后 ret 回 kmain（longjmp 风） */
extern uint64_t kctx[14];                         /* 内核 callee-saved 保存区 ra,sp,s0..s11 */

/* user.c 提供：嵌入内核的 U 态主程序 */
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

/* —— ulib：极简用户态运行时（S09 libc 丐版）。只靠 ecall 求服务，不直接碰内核控制台。—— */
long usyscall(long n, long a0, long a1, long a2);
unsigned long u_strlen(const char *s);
int  u_strcmp(const char *a, const char *b);
void u_write(const char *buf, long len);
void u_puts(const char *s);
void u_putint(int v);
void u_exit(long code);

/* —— 三个用户程序：各自返回 1=过 / 0=不过 —— */
int app_sort(void);
int app_template(void);
int app_tui(void);

#endif
