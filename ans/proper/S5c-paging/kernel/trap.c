/* S5c · trap 钩子（给定）：捕获缺页异常，置标志并跳过出错指令安全恢复。 */
#include "kernel.h"
#include "riscv.h"

/* 缺页异常 code（scause 最高位=0 表示异常）。 */
#define EXC_INST_PAGE_FAULT  12UL /* 取指缺页 */
#define EXC_LOAD_PAGE_FAULT  13UL /* load 缺页 */
#define EXC_STORE_PAGE_FAULT 15UL /* store/AMO 缺页 */

volatile int      g_fault       = 0; /* 是否发生过缺页（harness 检查） */
volatile uint64_t g_fault_cause = 0; /* 最近一次缺页的 scause */

void trap_init(void) {
    w_stvec((uint64_t)__alltraps); /* Direct 模式：所有 trap → __alltraps */
}

void trap_handler(struct TrapContext *ctx) {
    uint64_t scause = r_scause();
    int is_exc = !(scause & SCAUSE_INT_BIT);
    if (is_exc && (scause == EXC_LOAD_PAGE_FAULT ||
                   scause == EXC_STORE_PAGE_FAULT ||
                   scause == EXC_INST_PAGE_FAULT)) {
        g_fault = 1;
        g_fault_cause = scause;
        kputs("page fault scause=");
        kputdec(scause);
        kputs(" stval=");
        kputhex(r_stval());
        console_putchar('\n');
        /* 跳过出错指令安全恢复：按低 2 位判压缩指令长度（2 或 4 字节）。 */
        uint16_t lo = *(volatile uint16_t *)ctx->sepc;
        ctx->sepc += ((lo & 0x3) == 0x3) ? 4 : 2;
    } else {
        kputs("TRAP_BAD scause=");
        kputhex(scause);
        console_putchar('\n');
        ctx->sepc += 4;
    }
}
