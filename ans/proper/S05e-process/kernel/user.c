/* S05e · 两个内嵌 U 态用户程序（给定）。
 * 只经 ecall + 寄存器约定向内核求服务，不直接调用任何内核函数。
 * 约定：a7=系统调用号, a0..a2=参数, ecall 后返回值在 a0。
 *
 * 关键：「共享变量」放在固定用户 VA = DATA_VA 的页里（fork 时被 CoW 共享）。
 * 用户程序经裸指针读写它，从而能演示父子地址空间隔离 / 写时复制。 */
#include "proc.h"

static long usys(long n, long a0, long a1, long a2) {
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

static void uwrite(const char *s) {
    long n = 0;
    while (s[n]) n++;
    usys(SYS_WRITE, 1, (long)s, n);
}
static long ufork(void)        { return usys(SYS_FORK, 0, 0, 0); }
static long ugetpid(void)      { return usys(SYS_GETPID, 0, 0, 0); }
static long uwait(long *code)  { return usys(SYS_WAIT, (long)code, 0, 0); }
static void uexec(void)        { usys(SYS_EXEC, 0, 0, 0); for (;;) {} }
static void uexit(long c)      { usys(SYS_EXIT, c, 0, 0); for (;;) {} }

/* 第一个程序：设共享变量 → fork → 子进程写变量（触发 CoW）→ exec；父进程 wait 回收。 */
void user_a(void) {
    volatile long *shared = (volatile long *)DATA_VA;
    *shared = 100; /* fork 前写：此时页可写，是父进程的私有值 */

    long pid = ufork();
    if (pid == 0) {
        /* —— 子进程 —— */
        uwrite("[child] running (fork returned 0), pid=");
        long me = ugetpid();
        (void)me;
        uwrite("\n[child] writing shared=999 (should split CoW page)\n");
        *shared = 999; /* 写共享页：触发 store 缺页 → CoW 分裂成私有帧 */
        uwrite("[child] wrote shared, now exec the second program\n");
        uexec();       /* 用第二个程序替换自身映像，不返回 */
        uexit(123);    /* 到不了 */
    } else {
        /* —— 父进程 —— */
        uwrite("[parent] forked a child, now wait() for it\n");
        long code = -1;
        long w = uwait(&code);
        uwrite("[parent] child reaped\n");
        if (*shared == 100)
            uwrite("[parent] my shared is still 100 -> address spaces isolated\n");
        else
            uwrite("[parent] my shared CHANGED -> isolation broken\n");
        (void)w;
        uexit(0);
    }
}

/* 第二个程序：被 exec 载入运行，打印身份后 exit(7)（父 wait 拿到的退出码即 7）。 */
void user_b(void) {
    uwrite("[B] second program running via exec, will exit(7)\n");
    uexit(7);
}
