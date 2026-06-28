/* S08 · 嵌在内核里的 U 态用户程序（给定）。
 * 只通过 ecall + 寄存器约定向内核求服务，不直接调用任何内核函数。
 * 约定：a7=系统调用号, a0..a2=参数, ecall 后返回值在 a0。 */
#include "app.h"

static long usyscall(long n, long a0, long a1, long a2) {
    register long r_a0 asm("a0") = a0;
    register long r_a1 asm("a1") = a1;
    register long r_a2 asm("a2") = a2;
    register long r_a7 asm("a7") = n;
    asm volatile("ecall"
                 : "+r"(r_a0)
                 : "r"(r_a1), "r"(r_a2), "r"(r_a7)
                 : "memory");
    return r_a0;
}

void user_main(void) {
    /* 用户态打印（经 sys_write 走 syscall，而非直接碰控制台）。 */
    const char msg[] = "SYSCALL_PASS\n";
    usyscall(SYS_WRITE, 1 /*fd=stdout*/, (long)msg, (long)(sizeof(msg) - 1));
    usyscall(SYS_EXIT, 0, 0, 0);
    /* sys_exit 不返回；保险起见死循环。 */
    for (;;) {
    }
}
