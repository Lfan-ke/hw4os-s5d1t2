/* S6c · trap 分发（给定，复用 S2 框架，扩展 scause=9 外部中断分支）。
 * 与 S2 唯一的区别：这里把「S 态外部中断」交给 PLIC 处理路径。
 * 防中断风暴：若外部中断进入次数超过上限（说明 complete/清源未正确实现），
 * 直接屏蔽 sie.SEIE 兜底，保证不会死循环卡住测试。 */
#include "kernel.h"
#include "riscv.h"
#include "plic.h"

#define MAX_EXT_TRAPS 64 /* 有界守卫：正常只该触发 1 次 */

volatile uint64_t g_ext_traps   = 0;
volatile uint64_t g_last_scause = 0;

void trap_init(void) {
    w_stvec((uint64_t)__alltraps); /* Direct 模式：所有 trap 跳 __alltraps */
}

void trap_handler(struct TrapContext *ctx) {
    uint64_t scause = r_scause();
    g_last_scause = scause;

    if (scause & SCAUSE_INT_BIT) {
        uint64_t code = scause & 0xff;
        if (code == SCAUSE_S_EXTERNAL) { /* scause=9：S 态外部中断 */
            g_ext_traps++;
            plic_external_handler();     /* 学生实现：claim → 读设备 → complete */
            if (g_ext_traps >= MAX_EXT_TRAPS) {
                ext_irq_off();           /* 兜底：疑似中断风暴，屏蔽外部中断 */
            }
            return;
        }
        if (code == SCAUSE_S_TIMER) {    /* 本实验不开时钟中断，保留分支以防万一 */
            set_next_trigger();
            return;
        }
    }
    /* 其它一律视为异常，跳过出错指令避免死循环（输出含 UNEXPECTED 触发判负）。 */
    kputs("UNEXPECTED_TRAP scause=");
    kputhex(scause);
    console_putchar('\n');
    ctx->sepc += 4;
}
