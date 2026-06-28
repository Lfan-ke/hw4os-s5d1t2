/* S01 · SBI 层（参考解）：用 ecall 触发 S→M 陷入，请 OpenSBI 提供服务。 */
#include "kernel.h"

long sbi_call(long eid, long fid, long a0, long a1, long a2) {
    register long r_a0 asm("a0") = a0;
    register long r_a1 asm("a1") = a1;
    register long r_a2 asm("a2") = a2;
    register long r_a6 asm("a6") = fid;
    register long r_a7 asm("a7") = eid;
    asm volatile("ecall"
                 : "+r"(r_a0)
                 : "r"(r_a1), "r"(r_a2), "r"(r_a6), "r"(r_a7)
                 : "memory");
    return r_a0;
}

void console_putchar(int c) {
    sbi_call(1, 0, c, 0, 0); /* legacy console putchar, EID=1 */
}

void k_shutdown(void) {
    sbi_call(0x53525354, 0, 0, 0, 0); /* SRST: system_reset(shutdown) */
    sbi_call(8, 0, 0, 0, 0);          /* 回退：legacy shutdown EID=8 */
    for (;;) {
    }
}
