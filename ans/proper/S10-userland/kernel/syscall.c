/* S10 · trap 入口设置 + U 态 ecall 的 syscall 分发（给定，沿用 S08 的最小 ABI）。 */
#include "app.h"
#include "riscv.h"

void trap_init(void) {
    w_stvec((uint64_t)__alltraps); /* Direct 模式：所有 trap 跳共享 __alltraps */
}

/* —— 各系统调用处理体（给定）—— */
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

/* a7 分发表（给定）：未知号返回 -1 并报错（用中性词，避免命中 forbid）。 */
long do_syscall(long n, long a0, long a1, long a2) {
    switch (n) {
    case SYS_WRITE:
        return sys_write(a0, (const char *)a1, a2);
    case SYS_EXIT:
        sys_exit(a0); /* 不返回 */
        return 0;
    default:
        kputs("unhandled syscall n=");
        kputdec((uint64_t)n);
        console_putchar('\n');
        return -1;
    }
}

void trap_handler(struct TrapContext *ctx) {
    uint64_t scause = r_scause();
    /* U 态 ecall：scause 为异常（最高位=0）且 code==8。 */
    if (!(scause & SCAUSE_INT_BIT) && scause == SCAUSE_U_ECALL) {
        long n  = (long)ctx->x[17]; /* a7：系统调用号 */
        long a0 = (long)ctx->x[10];
        long a1 = (long)ctx->x[11];
        long a2 = (long)ctx->x[12];
        long ret = do_syscall(n, a0, a1, a2);
        ctx->x[10] = (uint64_t)ret; /* 返回值写回 a0 */
        ctx->sepc += 4;             /* 跳过 ecall，否则 sret 后又陷入 → 死循环 */
    } else {
        kputs("unhandled trap scause=");
        kputhex(scause);
        console_putchar('\n');
        ctx->sepc += 4;
    }
}
