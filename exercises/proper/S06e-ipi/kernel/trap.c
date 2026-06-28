/* S06e · trap 分发（给定，复用 S02/S06c 框架）。
 * 这里关心的是「S 态软件中断」(scause=1) - 即 IPI 抵达：OpenSBI 写了目标 hart 的
 * CLINT MSIP、把 mip.SSIP 反射给 S 态，CPU 取到 scause=1。处理就一件事：ack（清 sip.SSIP）
 * 并计数；不清则该中断会反复重入。外部中断在本实验用「轮询 claim」演示仲裁、SEIE 全程关，
 * 故这里对 scause=9 仅做防御性屏蔽。 */
#include "kernel.h"
#include "riscv.h"
#include "ipi.h"

#define MAX_SW_TRAPS 64 /* 有界守卫：正常每核只该收 1 次 IPI */

void trap_init(void) {
    w_stvec((uint64_t)__alltraps); /* Direct 模式：所有 trap 跳 __alltraps（复用 common/trap.S）*/
}

void trap_handler(struct TrapContext *ctx) {
    uint64_t scause = r_scause();

    if (scause & SCAUSE_INT_BIT) {
        uint64_t code = scause & 0xff;
        if (code == SCAUSE_S_SOFT) {      /* scause=1：S 态软件中断（IPI 抵达）*/
            soft_irq_ack();               /* 清 sip.SSIP，否则反复重入 */
            g_mbox.last_sw_scause = scause;
            __sync_fetch_and_add(&g_mbox.ipi_count, 1);
            if (g_mbox.ipi_count >= MAX_SW_TRAPS) soft_irq_off(); /* 兜底：疑似风暴 */
            return;
        }
        if (code == SCAUSE_S_EXTERNAL) {  /* 本实验外部中断走轮询 claim，SEIE 关，防御性屏蔽 */
            ext_irq_off();
            return;
        }
        if (code == SCAUSE_S_TIMER) {     /* 本实验不开时钟中断，保留分支 */
            return;
        }
    }
    /* 其它一律视为异常，跳过出错指令避免死循环（输出含 UNEXPECTED 触发判负）。 */
    kputs("UNEXPECTED_TRAP scause=");
    kputhex(scause);
    console_putchar('\n');
    ctx->sepc += 4;
}
