/* S06e · 真实 IPI（CLINT MSIP，经 SBI）+ 多核 PLIC claim 仲裁 本地头（给定）。
 *
 * 两条主线，都在 qemu-virt -smp 4 的真内核上跑：
 *   1) 软件中断/IPI：S 态用 SBI 的 IPI 扩展给「另一个 hart」发中断。OpenSBI 在 M 态
 *      实现时正是去写目标 hart 的 CLINT MSIP（0x0200_0000 + 4*hart），触发 M 态软件中断，
 *      再把 mip.SSIP 反射给 S 态 - 于是目标 hart 取到 scause=1 的 S 态软件中断（复用 S02 trap 框架）。
 *   2) 多核 PLIC claim 仲裁：把同一外设源（UART src=10）同时使能到两个 hart 的 S-context，
 *      两核在一个 barrier 处同时读各自的 claim 寄存器 - PLIC 硬件保证只有一个核拿到非零 irq、
 *      另一个读到 0（claim 是「谁先读谁得到」的原子领取）。这就是跨核「只一个 hart 处理该 IRQ」。
 */
#ifndef S6E_IPI_H
#define S6E_IPI_H
#include <stdint.h>
#include "kernel.h"
#include "riscv.h"

/* ============ NS16550 UART（qemu virt：MMIO，reg-shift=0，IRQ=10）============ */
#define UART0_BASE 0x10000000UL
#define UART0_IRQ  10
#define UART_RBR 0   /* 读：接收保持寄存器（读它清 LSR.DR、撤 UART 中断线） */
#define UART_THR 0   /* 写：发送保持寄存器 */
#define UART_IER 1   /* 中断使能：bit0=ERBFI（收到数据中断）*/
#define UART_MCR 4   /* modem 控制：bit4=LOOP（回环自激）*/
#define UART_LSR 5   /* 线路状态：bit0=DR（有数据）*/

#define IER_ERBFI 0x01
#define MCR_LOOP  0x10
#define LSR_DR    0x01

uint8_t uart_reg_read(uint64_t base, int off);
void    uart_reg_write(uint64_t base, int off, uint8_t v);
void    uart_irq_loopback_init(uint64_t base);

/* ============ PLIC（qemu virt @ 0x0c000000，沿用 S06c 布局）============
 *   priority[src]       = BASE + 4*src
 *   enable[ctx] (位图)  = BASE + 0x2000   + ctx*0x80   ，源 src 在第 src 位
 *   threshold[ctx]      = BASE + 0x200000 + ctx*0x1000
 *   claim/complete[ctx] = BASE + 0x200000 + ctx*0x1000 + 4
 * hartN 的 S-context 号 = 2N+1。本实验把 UART 源同时使能到「两个 hart」的 S-context。*/
#define PLIC_BASE           0x0c000000UL
#define PLIC_PRIORITY(src)  (PLIC_BASE + 4UL * (uint64_t)(src))
#define PLIC_ENABLE(ctx)    (PLIC_BASE + 0x2000UL + (uint64_t)(ctx) * 0x80UL)
#define PLIC_THRESHOLD(ctx) (PLIC_BASE + 0x200000UL + (uint64_t)(ctx) * 0x1000UL)
#define PLIC_CLAIM(ctx)     (PLIC_BASE + 0x200000UL + (uint64_t)(ctx) * 0x1000UL + 4UL)
#define PLIC_S_CTX(hart)    (2 * (int)(hart) + 1)

static inline uint32_t plic_read(uint64_t addr)            { return *(volatile uint32_t *)addr; }
static inline void     plic_write(uint64_t addr, uint32_t v){ *(volatile uint32_t *)addr = v; }

void plic_ctx_init(unsigned long hartid);  /* 配本 hart 的 S-context：UART 源优先级/使能/阈值 */
void plic_claim_one(unsigned long hartid); /* 本 hart 在 barrier 处读 claim 参与仲裁（学生填）*/

/* ============ CLINT（仅文档：S 态不能直接写，IPI 走 SBI；OpenSBI 在 M 态写这里）============ */
#define CLINT_BASE      0x02000000UL
#define CLINT_MSIP(hart)(CLINT_BASE + 4UL * (uint64_t)(hart)) /* M 态软件中断挂起位，S 态无权 */

/* ============ SBI 扩展：HSM（启核）+ IPI（软件中断）============ */
#define SBI_EID_HSM 0x48534DL   /* "HSM"：fid=0 hart_start */
#define SBI_EID_IPI 0x735049L   /* "sPI"：fid=0 send_ipi（向目标 hart 置 sip.SSIP）*/

static inline long sbi_hart_start(uint64_t hartid, uint64_t addr, uint64_t opaque) {
    return sbi_call(SBI_EID_HSM, 0, (long)hartid, (long)addr, (long)opaque);
}
/* hart_mask 的第 (i-base) 位为 1 → 给 hart i 发 IPI；返回 0 成功。 */
static inline long sbi_send_ipi(unsigned long hart_mask, unsigned long base) {
    return sbi_call(SBI_EID_IPI, 0, (long)hart_mask, (long)base, 0);
}

/* ============ scause / sie / sip 扩展（riscv.h 只有 timer，软/外中断本地补）============ */
#define SCAUSE_S_SOFT     1UL        /* S 态软件中断 code（IPI）*/
#define SCAUSE_S_EXTERNAL 9UL        /* S 态外部中断 code（PLIC）*/
#define SIE_SSIE (1UL << 1)          /* sie.SSIE：S 态软件中断使能 */
#define SIE_SEIE (1UL << 9)          /* sie.SEIE：S 态外部中断使能 */
#define SIP_SSIP (1UL << 1)          /* sip.SSIP：S 态软件中断挂起（写 0 即 ack）*/

static inline void soft_irq_on(void)  { asm volatile("csrs sie, %0" :: "r"(SIE_SSIE)); }
static inline void soft_irq_off(void) { asm volatile("csrc sie, %0" :: "r"(SIE_SSIE)); }
static inline void ext_irq_on(void)   { asm volatile("csrs sie, %0" :: "r"(SIE_SEIE)); }
static inline void ext_irq_off(void)  { asm volatile("csrc sie, %0" :: "r"(SIE_SEIE)); }
static inline void soft_irq_ack(void) { asm volatile("csrc sip, %0" :: "r"(SIP_SSIP)); }

/* ============ 多核协同（跨 hart 共享邮箱，内核 .bss → 直接映射，物理地址各 hart 共识）============ */
#define PHASE_INIT  0
#define PHASE_IPI   1
#define PHASE_CLAIM 2
#define PHASE_DONE  3

struct ipi_mbox {
    uint64_t target_online;     /* target 进入 secondary_main 即置 1 */
    uint64_t target_armed;      /* target 配好 trap/PLIC/中断，可收 IPI 后置 1 */
    uint64_t phase;             /* 引导核驱动：INIT→IPI→CLAIM→DONE */
    uint64_t ipi_count;         /* target 的软件中断处理累加（IPI 到达计数）*/
    uint64_t last_sw_scause;    /* 最近一次软件中断的 scause（验证 code=1）*/
    uint64_t barrier;           /* claim 仲裁屏障：两核都到齐才一起读 claim */
    uint64_t claim_nonzero;     /* 领到非零 irq==UART 的核数（应恰为 1：仲裁赢家）*/
    uint64_t claim_zero;        /* 读到 0 的核数（应恰为 1：仲裁输家）*/
    uint64_t claim_other;       /* 领到意外 irq 的核数（应为 0）*/
    uint64_t claim_winner;      /* 赢家 hartid + 1（避 0 歧义）*/
    uint64_t claim_byte;        /* 赢家从 UART 读回的字节 */
    uint64_t target_done;       /* target 收尾后置 1 */
};
extern volatile struct ipi_mbox g_mbox;

void secondary_entry(void);                 /* secondary.S：SBI 跳入的裸入口（设栈→调 secondary_main）*/
void secondary_main(unsigned long hartid);  /* C：target hart 主体 */

/* 全屏障：保证邮箱写在另一 hart 读到之前可见（多核可见性与顺序）。 */
static inline void smp_fence(void) { asm volatile("fence rw, rw" ::: "memory"); }

#endif
