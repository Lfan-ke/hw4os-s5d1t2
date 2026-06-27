/* S5e · 内核入口/测试驱动（给定）：建首个进程的地址空间 → 进调度器跑 fork/exec/wait。 */
#include "kernel.h"
#include "proc.h"
#include "riscv.h"

void kmain(void) {
    kputs("\n[S5e] process: fork / exec / wait + copy-on-write (rcore ch5)\n");

    /* S 态访问带 U 标志的页：处理 trap 时内核要读写用户栈/进程页，需开 sstatus.SUM。 */
    asm volatile("csrs sstatus, %0" ::"r"(1UL << 18));

    trap_init(); /* stvec -> __alltraps */

    /* 建首个进程 proc0（跑 user_a）：独立 SV39 地址空间（内核恒等映射 + 用户栈 + 数据页）。 */
    struct proc *p0 = proc_alloc();
    p0->parent = -1;
    p0->root = uvm_create();
    p0->satp = make_satp(p0->root);
    p0->tf.sepc = TO_USER(user_a);   /* U 态入口（用户别名区地址） */
    p0->tf.x[2] = USTACK_TOP;        /* 用户栈顶 */
    p0->tf.sstatus = (1UL << 18);    /* SUM=1, SPP=0 → sret 落 U 态 */
    p0->state = P_RUNNABLE;

    /* 进调度器：依次切 satp 跑各进程（fork 出的子进程、exec 的第二程序），
     * 直到再无可运行进程，longjmp 回这里。 */
    sched_enter();

    kputs("[kernel] all processes finished\n");
    if (g_fork && g_cow && g_exec && g_wait) {
        kputs("ALL_PASS\n");
    } else {
        kputs("INCOMPLETE fork=");
        kputdec((uint64_t)g_fork);
        kputs(" cow=");
        kputdec((uint64_t)g_cow);
        kputs(" exec=");
        kputdec((uint64_t)g_exec);
        kputs(" wait=");
        kputdec((uint64_t)g_wait);
        console_putchar('\n');
    }
    /* kmain 返回 → entry.S 调 k_shutdown 关机。 */
}
