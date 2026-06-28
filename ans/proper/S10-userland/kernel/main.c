/* S10 · 内核入口/测试驱动（给定）：跌入 U 态跑用户态应用集，回收后报告。
 * 承接 S08（U 态 + syscall）/ S09（libc）：用户程序基于 ulib 求服务、自检后退出。 */
#include "app.h"

/* 内核 callee-saved 保存区（被 uentry.S 使用）。 */
uint64_t kctx[14];

/* U 态栈（与内核同地址空间，无分页；仅特权级隔离）。16 字节对齐。 */
static uint64_t user_stack[2048] __attribute__((aligned(16)));

volatile long g_exit_code = -1;
volatile long g_proc_done = 0;

void kmain(void) {
    kputs("\n[S10] userland: sort + template + MD->ANSI TUI (U mode via S09 libc)\n");
    trap_init(); /* stvec -> __alltraps */

    kputs("entering U mode, running user apps...\n\n");
    run_user((uint64_t)user_main,
             (uint64_t)((char *)user_stack + sizeof(user_stack)));

    /* —— 回到内核：回收并报告 —— */
    kputs("\nkernel reclaimed userland, exit code=");
    kputdec((uint64_t)g_exit_code);
    console_putchar('\n');

    if (g_proc_done && g_exit_code == 0) {
        kputs("ALL_PASS\n");
    } else {
        kputs("userland did not finish cleanly\n");
    }
}
