/* S06b · CTE（Context / Trap / Event）层实现（参考解）。
 * 这是 HAL 里最“硬核”的一层：把“陷入硬件”包装成与 arch 无关的事件 + 上下文，
 * 让上层调度器只跟 Event/Context 打交道，完全不碰 stvec/sepc/sret。
 *
 * 自陷用 ebreak（断点异常 scause=3，OpenSBI 默认委派到 S 态）——
 * 它就是 AM 里 yield 用 ecall 自陷的 S 态等价物（S 态 ecall 会被 OpenSBI 截走当 SBI 调用，
 * 故这里改用同样“同步、可委派”的 ebreak）。 */
#include "am.h"
#include "kernel.h"
#include "riscv.h"

#define SSTATUS_SPP  (1UL << 8)  /* 1 = 陷入前处于 S 态；sret 据此回到 S 态 */
#define SCAUSE_BREAKPOINT 3UL    /* 断点异常 = 我们的“自陷/yield” */

static Context *(*user_handler)(Event, Context *) = NULL;

/* C 侧分发器：被 trap.S 调用，收下完整现场，翻译成事件，交给上层 handler，
 * 返回“下一个要恢复的 Context*”（可能是另一个任务的现场）。 */
Context *__am_irq_handle(Context *c) {
    if (user_handler) {
        Event ev = { 0 };
        ev.cause = c->scause;
        switch (c->scause) {
        case SCAUSE_BREAKPOINT:
            ev.event = EVENT_YIELD;
            c->sepc += 4;   /* 跨过 ebreak，让出方下次被调度时从其后继续 */
            break;
        default:
            ev.event = EVENT_ERROR;
            c->sepc += 4;   /* 兜底跨过，避免对同一指令反复陷入 */
            break;
        }
        c = user_handler(ev, c);
    }
    return c;
}

bool cte_init(Context *(*handler)(Event, Context *)) {
    w_stvec((uint64_t)__am_trap);   /* 陷入入口 → trap.S */
    user_handler = handler;
    return true;
}

void yield(void) {
    /* 自陷：执行一条断点指令，硬件跳到 stvec(__am_trap)，
     * 保存现场 → __am_irq_handle 派 EVENT_YIELD → 上层调度器挑下一个 → 恢复返回。
     * 用未压缩的 ebreak 编码，确保 sepc += 4 正好跨过它。 */
    asm volatile(".word 0x00100073");
}

Context *kcontext(Area kstack, void (*entry)(void *), void *arg) {
    /* 在 kstack 顶部摆一份 Context：恢复后 sret 落到 entry，a0=arg。
     * Context 紧贴栈顶 → trap.S 恢复末尾 addi sp,+35*8 后 sp 正好回到 kstack.end。 */
    Context *c = (Context *)kstack.end - 1;
    for (int i = 0; i < 32; i++) c->gpr[i] = 0;
    c->scause = 0;
    c->sstatus = SSTATUS_SPP;     /* sret → S 态；SPIE=0 → 任务内中断关闭（协作式）*/
    c->sepc = (uintptr_t)entry;   /* 首次被调度时的入口 */
    c->gpr[10] = (uintptr_t)arg;  /* a0 = entry 的参数 */
    return c;
}
