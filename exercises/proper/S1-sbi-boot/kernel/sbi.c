/* S1 · SBI 层：用 ecall 触发 S→M 陷入，请 OpenSBI 提供服务。你只需填 sbi_call。 */
#include "kernel.h"

long sbi_call(long eid, long fid, long a0, long a1, long a2) {
    (void)eid; (void)fid; (void)a0; (void)a1; (void)a2;
    /* TODO: 用内联汇编做 SBI 调用：
     *   eid→a7, fid→a6, a0/a1/a2→a0/a1/a2，执行 ecall，返回值在 a0。
     * HINT: 用 GCC 具名寄存器变量：
     *   register long r_a7 asm("a7") = eid;  ... register long r_a0 asm("a0") = a0;
     *   asm volatile("ecall" : "+r"(r_a0) : "r"(r_a1),"r"(r_a2),"r"(r_a6),"r"(r_a7) : "memory");
     *   return r_a0;
     */
    return 0; /* ← 占位：未实现 → 控制台无输出、无法关机 → 判 FAIL/超时 */
}

/* 以下两个由 sbi_call 派生（给定） */
void console_putchar(int c) {
    sbi_call(1, 0, c, 0, 0); /* legacy console putchar, EID=1 */
}

void k_shutdown(void) {
    sbi_call(0x53525354, 0, 0, 0, 0); /* SRST shutdown */
    sbi_call(8, 0, 0, 0, 0);          /* 回退 legacy EID=8 */
    for (;;) {
    }
}
