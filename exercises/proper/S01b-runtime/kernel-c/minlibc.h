/* S01b-c · 最小 libc 接口（建在 newlib 式 syscall 桩之上）。 */
#ifndef MINLIBC_H
#define MINLIBC_H
typedef unsigned long size_t;
/* —— OS 适配层：newlib/picolibc 要求的 syscall 桩（本实验由学生实现 _sbrk/_write）—— */
void *_sbrk(int incr);
int   _write(int fd, const char *buf, int n);
void  _exit(int code);
/* —— minlibc：标准库本体，全部经上面的桩与内核/SBI 打交道 —— */
void *malloc(size_t n);
size_t strlen(const char *s);
char *strcpy(char *d, const char *s);
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
int   printf(const char *fmt, ...);
int   puts(const char *s);
#endif
