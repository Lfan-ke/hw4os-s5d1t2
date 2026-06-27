/* S10 · ulib —— 极简用户态运行时（S9 libc 的丐版，给定）。
 * 三个用户程序复用它求服务：所有输出都经 ecall(SYS_WRITE) 走系统调用，
 * 而非直接调用内核 console_putchar —— U 态程序只认「调用号 + 寄存器约定」。 */
#include "app.h"

long usyscall(long n, long a0, long a1, long a2) {
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

unsigned long u_strlen(const char *s) {
    unsigned long n = 0;
    while (s[n]) n++;
    return n;
}

int u_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

void u_write(const char *buf, long len) {
    usyscall(SYS_WRITE, 1 /*fd=stdout*/, (long)buf, len);
}

void u_puts(const char *s) {
    u_write(s, (long)u_strlen(s));
}

/* 有符号十进制打印（丐版 printf 的零件）。 */
void u_putint(int v) {
    char buf[16];
    int n = 0;
    unsigned int x;
    if (v < 0) {
        char m = '-';
        u_write(&m, 1);
        x = (unsigned int)(-(long)v);
    } else {
        x = (unsigned int)v;
    }
    if (x == 0) { char z = '0'; u_write(&z, 1); return; }
    while (x) { buf[n++] = (char)('0' + x % 10); x /= 10; }
    char out[16];
    int oi = 0;
    while (n) out[oi++] = buf[--n];
    u_write(out, oi);
}

void u_exit(long code) {
    usyscall(SYS_EXIT, code, 0, 0);
}
