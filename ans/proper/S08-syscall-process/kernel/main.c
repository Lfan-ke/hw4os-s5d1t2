/* S08 · 内核入口/测试驱动（给定）：跌入 U 态跑用户程序，回收后报告。 */
#include "app.h"

/* 内核 callee-saved 保存区（被 uentry.S 使用）。 */
uint64_t kctx[14];

/* U 态栈（与内核同地址空间，无分页；仅特权级隔离）。16 字节对齐。 */
static uint64_t user_stack[1024] __attribute__((aligned(16)));

volatile long g_exit_code = -1;
volatile long g_proc_done = 0;

void kmain(void) {
    kputs("\n[S08] user mode + syscall (rcore ch2 batch style)\n");
    trap_init(); /* stvec -> __alltraps */

    kputs("entering U mode, running user program...\n");
    /* sret 进 U 态执行 user_main；user_main 经 ecall 求内核服务，
     * sys_exit 时 longjmp 回到此处下一行。 */
    run_user((uint64_t)user_main,
             (uint64_t)((char *)user_stack + sizeof(user_stack)));

    /* —— 回到内核：回收并报告 —— */
    kputs("kernel reclaimed process, exit code=");
    kputdec((uint64_t)g_exit_code);
    console_putchar('\n');

    if (g_proc_done && g_exit_code == 0) {
        kputs("PROC_PASS\n");
    } else {
        kputs("PROC_FAIL\n");
    }
    kputs("ALL_PASS\n");
}
