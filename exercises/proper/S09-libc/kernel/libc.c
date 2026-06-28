/* S09 · 极简 libc 实现（学生版）。运行在 U 态，只通过 ecall 与内核交互。
 * 需你实现两处 TODO：bump malloc、以及 vfmt 里 %d/%x 的数字格式化。
 * 占位状态下应能编译、运行、不崩溃，但不会出 MALLOC_PASS / PRINTF_PASS / ALL_PASS。 */
#include "ulib.h"

/* —— 系统调用封装：a7=号, a0..a2=参数, ecall 后返回值在 a0（已给定）—— */
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
    /* TODO(malloc): 实现 bump 分配。
     *   1) 把 heap_off 向上对齐到 8 字节：off = (heap_off + 7) & ~(size_t)7
     *   2) 若 off + n > sizeof(heap) → 堆耗尽，return 0
     *   3) 记下 void *p = &heap[off]；heap_off = off + n；return p
     * 占位：直接返回 NULL（能编译/运行，但 MALLOC_PASS 不会出现）。 */
    (void)n;
    (void)heap;
    (void)heap_off;
    return 0;
}

/* —— 字符串小工具（已给定）—— */
size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/* —— 格式化核心：%s 已给定；%d / %x 的数字格式化由你实现 —— */
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
            int v = va_arg(ap, int);  /* 取参（务必保留，否则后续 va_arg 错位）。 */
            /* TODO(printf-%d): 把十进制整数 v（含负号）写入 out[pos..]，更新 pos。
             *   提示：先处理负号（INT_MIN 用 -(long)v 再转 unsigned 防溢出），
             *   反复 % 10 把各位倒着塞临时数组（最低位先得），再逆序倒出。 */
            (void)v;
            break;
        }
        case 'x': {
            unsigned int v = va_arg(ap, unsigned int); /* 取参（务必保留）。 */
            /* TODO(printf-%x): 把 v 按小写十六进制写入 out[pos..]，更新 pos。
             *   提示：反复取低 4 位 v & 0xF（0..9->'0'..'9'，10..15->'a'..'f'），
             *   倒着塞临时数组再逆序倒出。 */
            (void)v;
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
