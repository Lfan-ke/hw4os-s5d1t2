/* S08b · trap 缺页钩子（给定）：缺页 → 交 vma_handle_fault 按需调页；
 * 命中按需补页 → 不前移 sepc，sret 后重执行出错指令即成功；
 * 未命中(真错误) → 记录 + 跳过出错指令安全恢复（不崩、不死循环）。 */
#include "kernel.h"
#include "riscv.h"
#include "vma.h"

#define EXC_INST_PAGE_FAULT  12UL /* 取指缺页 */
#define EXC_LOAD_PAGE_FAULT  13UL /* load 缺页 */
#define EXC_STORE_PAGE_FAULT 15UL /* store/AMO 缺页 */

/* harness 检查用的统计量（vma.h 里 extern）。 */
volatile int      g_fault_count = 0; /* 被服务的按需缺页次数（== 被触碰的页数） */
volatile int      g_real_fault  = 0; /* 是否发生过「不在任何 VMA」的真错误缺页 */
volatile uint64_t g_real_cause  = 0;

void trap_init(void) {
    w_stvec((uint64_t)__alltraps); /* Direct 模式：所有 trap → __alltraps */
}

void trap_handler(struct TrapContext *ctx) {
    uint64_t scause = r_scause();
    int is_exc = !(scause & SCAUSE_INT_BIT);
    int is_pf  = is_exc && (scause == EXC_LOAD_PAGE_FAULT ||
                            scause == EXC_STORE_PAGE_FAULT ||
                            scause == EXC_INST_PAGE_FAULT);
    if (is_pf) {
        uint64_t fva = r_stval();
        if (vma_handle_fault(fva)) {
            /* 已按需补页：计数 +1，不前移 sepc → 重执行出错指令。 */
            g_fault_count++;
            return;
        }
        /* 真错误：不在任何 VMA（野地址或已 munmap）。记录并安全跳过出错指令。 */
        g_real_fault = 1;
        g_real_cause = scause;
        kputs("real page fault scause=");
        kputdec(scause);
        kputs(" stval=");
        kputhex(fva);
        console_putchar('\n');
        uint16_t lo = *(volatile uint16_t *)ctx->sepc; /* 压缩指令 2 字节、否则 4 字节 */
        ctx->sepc += ((lo & 0x3) == 0x3) ? 4 : 2;
    } else {
        kputs("TRAP_BAD scause=");
        kputhex(scause);
        console_putchar('\n');
        ctx->sepc += 4;
    }
}
