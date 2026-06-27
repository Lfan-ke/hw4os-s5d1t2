/* S9 · 极简 libc 的用户侧头文件（给 U 态程序 include）。
 * U 态程序只认这一个头：所有「服务」最终经 write/exit 的 ecall 落到内核。
 * 注意：用户程序绝不直接调用 console_putchar/kputs 等内核函数——那会击穿特权边界。 */
#ifndef OSLAB_S9_ULIB_H
#define OSLAB_S9_ULIB_H
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* 系统调用号（必须与内核 app.h 一致）。 */
#define SYS_WRITE 64
#define SYS_EXIT  93

/* —— 系统调用封装 —— */
long  write(int fd, const void *buf, long len);
void  exit(int code);

/* —— 堆分配（bump）—— */
void *malloc(size_t n);

/* —— 字符串小工具 —— */
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);

/* —— 格式化 —— */
int vfmt(char *out, const char *fmt, va_list ap); /* 核心：写入 out（含 '\0'），返回字符数 */
int sprintf(char *out, const char *fmt, ...);     /* 格式化到缓冲区（自校验用） */
int printf(const char *fmt, ...);                 /* 格式化到内部缓冲后 write(1,...) */

/* 用户程序入口 */
int main(void);

#endif
