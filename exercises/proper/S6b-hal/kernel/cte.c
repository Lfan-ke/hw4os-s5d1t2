/* S6b · CTE（Context / Trap / Event）层（学生填空版）。
 * 这是 HAL 里最“硬核”的一层：把“陷入硬件”包装成与 arch 无关的事件 + 上下文，
 * 让上层调度器只跟 Event/Context 打交道，完全不碰 stvec/sepc/sret。
 *
 * 自陷用 ebreak（断点异常 scause=3，OpenSBI 默认委派到 S 态）——
 * 它就是 AM 里 yield 用 ecall 自陷的 S 态等价物（S 态 ecall 会被 OpenSBI 截走当 SBI 调用，
 * 故这里改用同样“同步、可委派”的 ebreak）。
 *
 * 你要填两处 TODO：yield() 的自陷、__am_irq_handle() 的事件分发。
 * 给定：cte_init（设 stvec）、kcontext（造初始上下文）。 */
#include "am.h"
#include "kernel.h"
#include "riscv.h"

#define SSTATUS_SPP  (1UL << 8)  /* 1 = 陷入前处于 S 态；sret 据此回到 S 态 */
#define SCAUSE_BREAKPOINT 3UL    /* 断点异常 = 我们的“自陷/yield” */

static Context *(*user_handler)(Event, Context *) = NULL;

/* C 侧分发器：被 trap.S 调用，收下完整现场，翻译成事件，交给上层 handler，
 * 返回“下一个要恢复的 Context*”（可能是另一个任务的现场）。 */
Context *__am_irq_handle(Context *c) {
    /* TODO: 事件分发。
     *   if (user_handler) {
     *       Event ev = { 0 };
     *       ev.cause = c->scause;
     *       若 c->scause == SCAUSE_BREAKPOINT：ev.event = EVENT_YIELD; 且 c->sepc += 4;
     *           （sepc+=4 跨过 ebreak，让出方下次被调度时从其后继续）
     *       否则：ev.event = EVENT_ERROR; c->sepc += 4;（兜底跨过，避免反复陷入）
     *       c = user_handler(ev, c);   // handler 返回“下一个 Context*”
     *   }
     *   return c;
     * 占位：直接返回 c（不分发、不调度）—— 配合下面 yield() 占位，
     *       上层 schedule 永不被调用 → 任务从不被拉起 → 运行序为空 → CTE_MISS（不崩、不死循环）。 */
    return c;
}

bool cte_init(Context *(*handler)(Event, Context *)) {
    w_stvec((uint64_t)__am_trap);   /* 陷入入口 → trap.S（给定）*/
    user_handler = handler;
    return true;
}

void yield(void) {
    /* TODO: 自陷一次。执行一条未压缩的断点指令，硬件即跳到 stvec(__am_trap)：
     *   asm volatile(".word 0x00100073");   // ebreak，确保 sepc += 4 正好跨过它
     * 之后 trap.S 存现场 → __am_irq_handle 派 EVENT_YIELD → 上层挑下一个 → 恢复返回。
     * 占位：什么都不做（yield 变成空操作）—— 任务无法切换 → CTE_MISS（不崩、不死循环）。 */
}

/* kcontext（给定）：在 kstack 顶部摆一份初始 Context，恢复后 sret 落到 entry，a0=arg。 */
Context *kcontext(Area kstack, void (*entry)(void *), void *arg) {
    Context *c = (Context *)kstack.end - 1;
    for (int i = 0; i < 32; i++) c->gpr[i] = 0;
    c->scause = 0;
    c->sstatus = SSTATUS_SPP;     /* sret → S 态；SPIE=0 → 任务内中断关闭（协作式）*/
    c->sepc = (uintptr_t)entry;   /* 首次被调度时的入口 */
    c->gpr[10] = (uintptr_t)arg;  /* a0 = entry 的参数 */
    return c;
}
