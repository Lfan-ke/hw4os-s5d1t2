/* S06e · 内核入口 / 测试驱动（参考解）。引导核当协调者，target hart 由 SBI HSM 唤醒。
 *
 * 三段：
 *   1) IPI：引导核用 SBI IPI 给 target 发软件中断（OpenSBI 落到 target 的 CLINT MSIP），
 *      target 的 trap 处理累加 ipi_count - IPI_PASS。
 *   2) CLAIM：UART 源同时使能到两核 S-context，回环自激置 pending，两核在 barrier 处一起
 *      读 claim，PLIC 仲裁出恰一个赢家、一个输家 - CLAIM_PASS。
 *   3) SMP：上线 + IPI + 仲裁三者皆成 - SMP_PASS。
 *
 * 打印只由引导核做（避免两核 console 交错毁掉 Pass 子串）；CLAIM 段开了 UART 回环、控制台
 * 静默，故该段只记录、关回环恢复控制台后再统一打印（同 S06c）。全程有界自旋，不 wfi 死等。 */
#include "kernel.h"
#include "riscv.h"
#include "ipi.h"

#define TEST_BYTE  'A'
#define SPIN_LIMIT 30000000UL

volatile struct ipi_mbox g_mbox;

static int wait_ge(volatile uint64_t *p, uint64_t v) {
    for (uint64_t i = 0; i < SPIN_LIMIT; i++) {
        if (*p >= v) return 1;
        smp_fence();
    }
    return 0;
}

/* ===== target hart 主体（给定脚手架）：配自己的 trap/PLIC ctx、开软件中断、报到、
 * 异步接 IPI、并在 barrier 处参与 claim 竞争。全程不打印。 ===== */
void secondary_main(unsigned long hartid) {
    g_mbox.target_online = 1;
    trap_init();                 /* 设本 hart 的 stvec */
    plic_ctx_init(hartid);       /* 配本 hart 的 S-context（使能 UART 源）*/
    soft_irq_on();               /* sie.SSIE：收 IPI */
    intr_on();                   /* sstatus.SIE */
    smp_fence();
    g_mbox.target_armed = 1;     /* 通知引导核：已就绪 */

    /* 等到 CLAIM 段（其间 IPI 会异步进来，由 trap 处理）。*/
    for (uint64_t i = 0; i < SPIN_LIMIT * 8 && g_mbox.phase < PHASE_CLAIM; i++) smp_fence();
    if (g_mbox.phase >= PHASE_CLAIM) {
        __sync_fetch_and_add(&g_mbox.barrier, 1);                       /* 到达屏障 */
        for (uint64_t i = 0; i < SPIN_LIMIT && g_mbox.barrier < 2; i++) smp_fence();
        plic_claim_one(hartid);                                        /* 与引导核同时读 claim */
    }

    for (uint64_t i = 0; i < SPIN_LIMIT * 8 && g_mbox.phase < PHASE_DONE; i++) smp_fence();
    soft_irq_off();
    ext_irq_off();
    intr_off();
    g_mbox.target_done = 1;
    /* 返回 secondary_entry 的 wfi 停泊。*/
}

void kmain(void) {
    /* 引导 hartid：OpenSBI 经 a0 传入；common/entry.S 在 call kmain 前不碰 a0，
       故必须在任何函数调用前最先捕获。-smp 4 下引导核不确定，唤醒一个确定不同的 target。*/
    unsigned long boot_hart;
    asm volatile("mv %0, a0" : "=r"(boot_hart));
    unsigned long target = (boot_hart == 0) ? 1 : 0;

    kputs("\n[S06e] real IPI (CLINT MSIP via SBI) + multi-core PLIC claim arbitration\n");
    kputs("boot hart=");
    kputdec(boot_hart);
    kputs(", target hart=");
    kputdec(target);
    console_putchar('\n');

    g_mbox.phase = PHASE_INIT;
    trap_init();                 /* 引导核 stvec */
    plic_ctx_init(boot_hart);    /* 引导核 S-context（使能 UART 源）*/
    soft_irq_off();              /* 引导核不收中断：claim 用轮询 */
    ext_irq_off();
    smp_fence();

    long r = sbi_hart_start(target, (uint64_t)secondary_entry, 0);
    kputs("sbi_hart_start ret=");
    kputdec((uint64_t)r);
    console_putchar('\n');

    int armed = wait_ge(&g_mbox.target_armed, 1);
    if (armed) kputs("target hart armed (stvec+PLIC ctx+SSIE ready)\n");
    else       kputs("ARM_MISS target not armed\n");

    /* - 1. IPI 段 - */
    g_mbox.phase = PHASE_IPI;
    smp_fence();
    sbi_send_ipi(1UL, target);   /* 给 target 发 IPI：经 OpenSBI 写其 CLINT MSIP → 目标 scause=1 */
    int ipi_fired = wait_ge(&g_mbox.ipi_count, 1);
    int ipi_ok = ipi_fired && (g_mbox.last_sw_scause & SCAUSE_INT_BIT) &&
                 ((g_mbox.last_sw_scause & 0xff) == SCAUSE_S_SOFT);
    if (ipi_ok) {
        kputs("IPI delivered to target, scause=");
        kputhex(g_mbox.last_sw_scause);
        console_putchar('\n');
        kputs("IPI_PASS\n");
    } else {
        kputs("IPI_MISS count=");
        kputdec(g_mbox.ipi_count);
        kputs(" scause=");
        kputhex(g_mbox.last_sw_scause);
        console_putchar('\n');
    }

    /* - 2. CLAIM 段（开回环→控制台静默；只记录，恢复后再打印） - */
    g_mbox.phase = PHASE_CLAIM;
    smp_fence();
    uart_irq_loopback_init(UART0_BASE);                 /* 控制台从此静默 */
    uart_reg_write(UART0_BASE, UART_THR, TEST_BYTE);    /* 自激：UART→PLIC src10 置 pending */
    for (uint64_t i = 0; i < SPIN_LIMIT; i++) {         /* 确认 pending 已立（DR 置位）*/
        if (uart_reg_read(UART0_BASE, UART_LSR) & LSR_DR) break;
    }
    __sync_fetch_and_add(&g_mbox.barrier, 1);           /* 引导核到达屏障 */
    for (uint64_t i = 0; i < SPIN_LIMIT && g_mbox.barrier < 2; i++) smp_fence();
    plic_claim_one(boot_hart);                          /* 与 target 同时读 claim */
    for (uint64_t i = 0; i < SPIN_LIMIT &&
         (g_mbox.claim_nonzero + g_mbox.claim_zero + g_mbox.claim_other) < 2; i++) smp_fence();

    uart_reg_write(UART0_BASE, UART_IER, 0);            /* 关回环，恢复控制台 */
    uart_reg_write(UART0_BASE, UART_MCR, 0);

    int claim_ok = (g_mbox.claim_nonzero == 1) && (g_mbox.claim_zero == 1) &&
                   (g_mbox.claim_other == 0) && (g_mbox.claim_winner != 0) &&
                   (g_mbox.claim_byte == (uint8_t)TEST_BYTE);
    if (claim_ok) {
        kputs("claim race: winner hart=");
        kputdec(g_mbox.claim_winner - 1);
        kputs(" got irq=10 byte=");
        kputhex(g_mbox.claim_byte);
        kputs(", loser read claim=0\n");
        kputs("CLAIM_PASS\n");
    } else {
        kputs("CLAIM_MISS nonzero=");
        kputdec(g_mbox.claim_nonzero);
        kputs(" zero=");
        kputdec(g_mbox.claim_zero);
        kputs(" other=");
        kputdec(g_mbox.claim_other);
        kputs(" byte=");
        kputhex(g_mbox.claim_byte);
        console_putchar('\n');
    }

    /* - 3. 收尾 + SMP 汇总 - */
    g_mbox.phase = PHASE_DONE;
    smp_fence();
    int done = wait_ge(&g_mbox.target_done, 1);

    int smp_ok = g_mbox.target_online && armed && done && ipi_ok && claim_ok;
    if (smp_ok) {
        kputs("SMP_PASS\n");
    } else {
        kputs("SMP_MISS online=");
        kputdec(g_mbox.target_online);
        kputs(" done=");
        kputdec((uint64_t)done);
        console_putchar('\n');
    }
    /* kmain 返回 → entry.S 调 k_shutdown，qemu 退出。 */
}
