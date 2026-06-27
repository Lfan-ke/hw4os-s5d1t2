/* 正经赛道共享 SBI 层（S2+ 给定；S1 由学生在 lab 内自行实现以学习 ecall）。 */
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

void console_putchar(int c) { sbi_call(1, 0, c, 0, 0); }

void sbi_set_timer(uint64_t t) { sbi_call(0, 0, (long)t, 0, 0); } /* legacy set_timer, EID=0 */

void k_shutdown(void) {
    sbi_call(0x53525354, 0, 0, 0, 0);
    sbi_call(8, 0, 0, 0, 0);
    for (;;) {
    }
}
