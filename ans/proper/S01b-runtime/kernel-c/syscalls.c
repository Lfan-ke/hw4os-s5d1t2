/* S01b-c · newlib 式 syscall 桩（OS 适配层，参考答案）。
 * 真正的 newlib/picolibc 就是插在这一组桩上：malloc→_sbrk，printf/puts→_write，
 * exit→_exit，另需 _read/_close/_lseek/_isatty/_fstat/_kill/_getpid（此处给空桩）。 */
#include "minlibc.h"

static long sbi(long eid, long fid, long a0) {
    register long r0 asm("a0") = a0;
    register long r6 asm("a6") = fid;
    register long r7 asm("a7") = eid;
    asm volatile("ecall" : "+r"(r0) : "r"(r6), "r"(r7) : "memory");
    return r0;
}

/* —— 学生实现①：堆来源。malloc 经此向 OS 要内存 —— */
static char heap[64 * 1024];
static char *hp = heap;
void *_sbrk(int incr) {
    char *prev = hp;
    if (hp + incr > heap + sizeof(heap)) return (void *)-1; /* OOM */
    hp += incr;
    return prev;
}

/* —— 学生实现②：标准输出出口。printf/puts 经此把字节送到控制台 —— */
int _write(int fd, const char *buf, int n) {
    (void)fd;
    for (int i = 0; i < n; i++) sbi(1, 0, (unsigned char)buf[i]); /* SBI console putchar */
    return n;
}

void _exit(int code) {
    (void)code;
    /* SRST system_reset: a0=type(0=shutdown) a1=reason(0) a6=fid(0) a7=eid */
    register long a0 asm("a0") = 0;
    register long a1 asm("a1") = 0;
    register long a6 asm("a6") = 0;
    register long a7 asm("a7") = 0x53525354;
    asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a6), "r"(a7) : "memory");
    sbi(8, 0, 0); /* fallback: legacy shutdown */
    for (;;) {}
}
/* 真 newlib 还需以下桩（minlibc 未用到，给最小空实现以示完整移植层）*/
int _read(int fd, char *b, int n) { (void)fd; (void)b; (void)n; return 0; }
int _close(int fd) { (void)fd; return -1; }
int _lseek(int fd, int o, int w) { (void)fd; (void)o; (void)w; return 0; }
int _isatty(int fd) { (void)fd; return 1; }
int _kill(int p, int s) { (void)p; (void)s; return -1; }
int _getpid(void) { return 1; }
