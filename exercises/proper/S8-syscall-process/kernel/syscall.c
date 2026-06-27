/* S8 · trap 入口设置 + U 态 ecall 的 syscall 分发（填空版）。
 * 进/出 U 态的 run_user/return_to_kernel 见 uentry.S（已给）。
 * 你只需补全 trap_handler 里对 U 态 ecall 的分发与返回处理。 */
#include "app.h"
#include "riscv.h"

void trap_init(void) {
    w_stvec((uint64_t)__alltraps); /* Direct 模式：所有 trap 跳共享 __alltraps */
}

/* —— 各系统调用处理体（已给）—— */
static long sys_write(long fd, const char *buf, long len) {
    (void)fd; /* 本实验只支持 fd=1（stdout） */
    for (long i = 0; i < len; i++) console_putchar((unsigned char)buf[i]);
    return len;
}

static void sys_exit(long code) {
    g_exit_code = code;
    g_proc_done = 1;
    return_to_kernel(); /* longjmp 回内核主线；不返回 */
}

/* a7 分发表（已给）：未知号返回 -1 并报错。 */
long do_syscall(long n, long a0, long a1, long a2) {
    switch (n) {
    case SYS_WRITE:
        return sys_write(a0, (const char *)a1, a2);
    case SYS_EXIT:
        sys_exit(a0); /* 不返回 */
        return 0;
    default:
        kputs("UNEXPECTED_SYSCALL n=");
        kputdec((uint64_t)n);
        console_putchar('\n');
        return -1;
    }
}

void trap_handler(struct TrapContext *ctx) {
    uint64_t scause = r_scause();
    (void)ctx;
    (void)scause;
    /* TODO: 处理 U 态系统调用。
     * 1) 判定是否 U 态 ecall：!(scause & SCAUSE_INT_BIT) 且 scause == SCAUSE_U_ECALL(=8)。
     * 2) 取号与参数：a7=ctx->x[17] 为调用号，a0..a2=ctx->x[10..12] 为参数。
     * 3) 调 do_syscall(n,a0,a1,a2)，把返回值写回 a0：ctx->x[10] = ret。
     * 4) ctx->sepc += 4 跳过 ecall（否则 sret 回到同一条 ecall，立刻又陷入 → 死循环）。
     * 5) 否则（其它异常）：打印 scause（kputs/kputhex），并 ctx->sepc += 4 跳过出错指令。
     * 注意：sys_exit 会经 return_to_kernel() longjmp 回内核主线，不会从这里正常返回。
     *
     * 占位：先只把 sepc 前移，避免 ecall 风暴；但不分发 → 进程无法退出、内核回收不到，
     * 跑不出 ALL_PASS（填好上面逻辑后才会通过）。 */
    ctx->sepc += 4;
}
