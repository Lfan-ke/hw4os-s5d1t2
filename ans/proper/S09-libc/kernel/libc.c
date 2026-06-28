/* S09 · 极简 libc 实现（参考解）。运行在 U 态，只通过 ecall 与内核交互。
 * 学生版需自行实现两处：bump malloc、以及 vfmt 里 %d/%x 的数字格式化。 */
#include "ulib.h"

/* —— 系统调用封装：a7=号, a0..a2=参数, ecall 后返回值在 a0（与内核 ABI 约定一致）—— */
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

long write(int fd, const void *buf, long len) {
    return usys(SYS_WRITE, (long)fd, (long)buf, len);
}

void exit(int code) {
    usys(SYS_EXIT, (long)code, 0, 0);
    for (;;) { } /* SYS_EXIT 不返回；保险死循环 */
}

/* —— bump（指针碰撞）分配器：从一块静态堆里顺序切，永不回收 —— */
static char   heap[8192] __attribute__((aligned(16)));
static size_t heap_off = 0;

void *malloc(size_t n) {
    /* 8 字节对齐当前游标 */
    size_t off = (heap_off + 7u) & ~(size_t)7u;
    if (off + n > sizeof(heap)) return 0; /* 堆耗尽 */
    void *p = &heap[off];
    heap_off = off + n;
    return p;
}

/* —— 字符串小工具 —— */
size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/* —— 格式化核心：支持 %d / %x / %s / %% —— */
int vfmt(char *out, const char *fmt, va_list ap) {
    int pos = 0;
    for (; *fmt; fmt++) {
        if (*fmt != '%') { out[pos++] = *fmt; continue; }
        fmt++;
        switch (*fmt) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (*s) out[pos++] = *s++;
            break;
        }
        case 'd': {
            int v = va_arg(ap, int);
            unsigned int uv;
            if (v < 0) { out[pos++] = '-'; uv = (unsigned int)(-(long)v); }
            else        uv = (unsigned int)v;
            char tmp[16];
            int  t = 0;
            if (uv == 0) tmp[t++] = '0';
            while (uv) { tmp[t++] = (char)('0' + uv % 10); uv /= 10; }
            while (t) out[pos++] = tmp[--t]; /* 逆序倒出 */
            break;
        }
        case 'x': {
            unsigned int v = va_arg(ap, unsigned int);
            char tmp[16];
            int  t = 0;
            if (v == 0) tmp[t++] = '0';
            while (v) { int d = (int)(v & 0xF); tmp[t++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); v >>= 4; }
            while (t) out[pos++] = tmp[--t];
            break;
        }
        case '%': out[pos++] = '%'; break;
        default:  out[pos++] = '%'; out[pos++] = *fmt; break;
        }
    }
    out[pos] = '\0';
    return pos;
}

int sprintf(char *out, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vfmt(out, fmt, ap);
    va_end(ap);
    return n;
}

static char pbuf[256];

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vfmt(pbuf, fmt, ap);
    va_end(ap);
    write(1, pbuf, n);
    return n;
}
