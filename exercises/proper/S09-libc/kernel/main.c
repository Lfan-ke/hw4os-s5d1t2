/* S09 · 内核入口/测试驱动（给定）：跌入 U 态跑「带 libc 的用户程序」，回收后报告。
 * 用户程序自己经 printf 打印 CRT0_PASS / MALLOC_PASS / PRINTF_PASS；
 * 内核仅在用户 exit(0) 时盖章 ALL_PASS（任一子项不过 → 用户 exit 非 0 → 不出 ALL_PASS）。 */
#include "app.h"

/* 内核 callee-saved 保存区（被 uentry.S 使用）。 */
uint64_t kctx[14];

/* U 态栈（与内核同地址空间，无分页；仅特权级隔离）。16 字节对齐。 */
static uint64_t user_stack[1024] __attribute__((aligned(16)));

volatile long g_exit_code = -1;
volatile long g_proc_done = 0;

void kmain(void) {
    kputs("\n[S09] minimal libc for user programs (crt0 + syscall + malloc + printf)\n");
    trap_init(); /* stvec -> __alltraps */

    kputs("entering U mode, running user program via crt0...\n");
    /* sret 进 U 态执行 crt0(user_start) -> main()；
     * 用户 exit() 时（SYS_EXIT）longjmp 回到此处下一行。 */
    run_user((uint64_t)user_start,
             (uint64_t)((char *)user_stack + sizeof(user_stack)));

    /* —— 回到内核：回收并报告 —— */
    kputs("kernel reclaimed user program, exit code=");
    kputdec((uint64_t)g_exit_code);
    console_putchar('\n');

    if (g_proc_done && g_exit_code == 0) {
        kputs("ALL_PASS\n");
    } else {
        /* 失败诊断（不含 FAIL/UNEXPECTED 字样）。 */
        kputs("[kernel] user exited non-zero; not all libc subtests passed\n");
    }
}
