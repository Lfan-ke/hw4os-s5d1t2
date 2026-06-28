/* S02 · trap 分发：填 trap_handler。trap 入口汇编(__alltraps)与 stvec 设置已给。 */
#include "kernel.h"
#include "riscv.h"

void trap_init(void) {
    w_stvec((uint64_t)__alltraps); /* Direct 模式：所有 trap 跳 __alltraps */
}

void trap_handler(struct TrapContext *ctx) {
    uint64_t scause = r_scause();
    (void)ctx;
    (void)scause;
    /* TODO: 分发 trap：
     *   若 (scause & SCAUSE_INT_BIT) 且 (scause & 0xff)==SCAUSE_S_TIMER：S 态时钟中断
     *       extern volatile uint64_t g_ticks; g_ticks++; set_next_trigger();
     *   否则（异常）：打印 scause（kputs/kputhex），并 ctx->sepc += 4 跳过出错指令。
     * 注意：时钟中断必须调 set_next_trigger() 重置比较器，否则中断风暴、内核卡死。
     */
}
