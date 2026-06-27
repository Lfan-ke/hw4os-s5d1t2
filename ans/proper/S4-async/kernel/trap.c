/* S4 · trap 分发（给定，复用 S2 口径）：时钟中断累加 g_ticks 并重置比较器。
 * 异步运行时的"时间推进"完全由这里驱动——延时 future 等的就是 g_ticks。 */
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
