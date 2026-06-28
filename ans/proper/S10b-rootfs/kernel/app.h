/* S10b · 本实验共享声明：U 态运行（承 S08）+ cpio/initramfs 解包。
 * RAM-fs（块设备 + inode/目录）见 fs.h（承 S07）。 */
#ifndef OSLAB_S10B_APP_H
#define OSLAB_S10B_APP_H
#include <stdint.h>
#include "kernel.h"

/* —— U 态进入 / syscall 返回内核（uentry.S，承 S08，已给）—— */
void run_user(uint64_t entry, uint64_t ustack); /* 清 sstatus.SPP、设 sepc/sp 后 sret */
void return_to_kernel(void);                     /* 恢复 kctx 后 ret 回 kmain（longjmp 风） */
extern uint64_t kctx[14];                        /* 内核 callee-saved 保存区 */

/* —— syscall（syscall.c，承 S08，已给）—— */
long do_syscall(long n, long a0, long a1, long a2);
extern volatile long g_exit_code;
extern volatile long g_proc_done;

/* 系统调用号（RV64 Linux ABI 风）。 */
#define SYS_WRITE 64
#define SYS_EXIT  93
/* U 态 ecall 的 scause code。 */
#define SCAUSE_U_ECALL 8UL

/* —— cpio / initramfs（cpio.c）—— */
/* 解析一条 newc 头后的视图：名字、文件大小、数据指针、下一条头的偏移。 */
struct cpio_hdr_view {
    const char    *name;     /* 以 0 结尾的成员名（指向归档内） */
    uint32_t       namesize; /* 名字字节数（含结尾 0） */
    uint32_t       filesize; /* 文件数据字节数 */
    const uint8_t *data;     /* 文件数据起始（指向归档内） */
    uint32_t       next;     /* 下一条头在归档中的字节偏移 */
};

/* 解析 8 个 ASCII 十六进制字符为 uint32（newc 头字段格式，已给）。 */
uint32_t cpio_hex8(const uint8_t *p);

/* 把内嵌的 cpio 归档逐条解开、灌进 RAM-fs；返回常规文件数（不含 TRAILER）。 */
int cpio_unpack(const uint8_t *arc, uint32_t len);

#endif
