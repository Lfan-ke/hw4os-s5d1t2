/* S2 · trap 分发（参考解）。 */
#include "kernel.h"
#include "riscv.h"

void trap_init(void) {
    w_stvec((uint64_t)__alltraps); /* Direct 模式：所有 trap 跳 __alltraps */
}

void trap_handler(struct TrapContext *ctx) {
    uint64_t scause = r_scause();
    if ((scause & SCAUSE_INT_BIT) && (scause & 0xff) == SCAUSE_S_TIMER) {
        extern volatile uint64_t g_ticks;
        g_ticks++;
        set_next_trigger(); /* 重置比较器，清本次时钟中断 */
    } else {
        kputs("UNEXPECTED_TRAP scause=");
        kputhex(scause);
        console_putchar('\n');
        ctx->sepc += 4; /* 跳过出错指令，避免死循环 */
    }
}
