/* S01b-c · 最小 libc 本体（baselibc/newlib 风）：malloc 经 _sbrk、printf/puts 经 _write。 */
#include "minlibc.h"
#include <stdarg.h>

void *malloc(size_t n) {
    n = (n + 7) & ~7UL; /* 8 字节对齐 */
    void *p = _sbrk((int)n);
    return p == (void *)-1 ? (void *)0 : p;
}

size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
char *strcpy(char *d, const char *s) { size_t i = 0; while ((d[i] = s[i])) i++; return d; }

__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *memset(void *d, int c, size_t n) {
    unsigned char *p = d;
    for (size_t i = 0; i < n; i++) p[i] = (unsigned char)c;
    return d;
}
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *memcpy(void *d, const void *s, size_t n) {
    unsigned char *a = d; const unsigned char *b = s;
    for (size_t i = 0; i < n; i++) a[i] = b[i];
    return d;
}

static void pc(char c) { _write(1, &c, 1); }
static void ps(const char *s) { while (*s) pc(*s++); }
static void pu(unsigned long v, int base) {
    char buf[24]; int n = 0; const char *dig = "0123456789abcdef";
    if (v == 0) { pc('0'); return; }
    while (v) { buf[n++] = dig[v % base]; v /= base; }
    while (n) pc(buf[--n]);
}
int puts(const char *s) { ps(s); pc('\n'); return 0; }
int printf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { pc(*p); continue; }
        p++;
        switch (*p) {
            case 'd': { int x = va_arg(ap, int);
                if (x < 0) { pc('-'); pu((unsigned long)(-(long)x), 10); }
                else { pu((unsigned long)x, 10); } break; }
            case 'x': pu((unsigned long)va_arg(ap, unsigned int), 16); break;
            case 's': ps(va_arg(ap, const char *)); break;
            case 'c': pc((char)va_arg(ap, int)); break;
            case '%': pc('%'); break;
            default: pc('%'); pc(*p); break;
        }
    }
    va_end(ap); return 0;
}
