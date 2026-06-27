/* S6c · PLIC + 外部中断本地头（给定）。
 * 在 S2 的 timer-only trap 基础上，新增「外部中断」这条路径：
 *   设备(UART) → PLIC → hart0 S 态外部中断(scause=9) → claim/服务/complete。
 */
#ifndef S6C_PLIC_H
#define S6C_PLIC_H
#include <stdint.h>

/* ============ NS16550 UART（qemu virt：MMIO，reg-shift=0，IRQ=10）============ */
#define UART0_BASE 0x10000000UL
#define UART0_IRQ  10            /* qemu virt 上 UART 的 PLIC 中断源号 */
#define UART_RBR 0   /* 读：接收保持寄存器（读它清 LSR.DR、撤 UART 中断线） */
#define UART_THR 0   /* 写：发送保持寄存器 */
#define UART_IER 1   /* 中断使能：bit0=ERBFI（收到数据中断）*/
#define UART_FCR 2
#define UART_LCR 3
#define UART_MCR 4   /* modem 控制：bit4=LOOP（回环自激）*/
#define UART_LSR 5   /* 线路状态：bit0=DR（有数据）bit5=THRE（发送空）*/

#define IER_ERBFI 0x01
#define MCR_LOOP  0x10
#define LSR_DR    0x01
#define LSR_THRE  0x20

uint8_t uart_reg_read(uint64_t base, int off);
void    uart_reg_write(uint64_t base, int off, uint8_t v);
void    uart_irq_loopback_init(uint64_t base); /* 开 RX 中断 + 回环（给定）*/

/* ============ PLIC（qemu virt @ 0x0c000000）============
 * 寄存器布局（与 xv6/Linux 一致）：
 *   priority[src]            = BASE + 4*src
 *   pending                  = BASE + 0x1000
 *   enable[ctx]   (位图)     = BASE + 0x2000 + ctx*0x80   ，源 src 在第 src 位
 *   threshold[ctx]           = BASE + 0x200000 + ctx*0x1000
 *   claim/complete[ctx]      = BASE + 0x200000 + ctx*0x1000 + 4
 * context 编号：qemu virt 上每个 hart 占 2 个 context，hartN → ctx(2N)=M、ctx(2N+1)=S。
 * 注意：-smp 4 下 OpenSBI 的「启动 hart」是不确定的（每次可能不同），PLIC 又是
 * per-hart/per-context 的，所以必须按「当前运行的 hart」配置它自己的 S-context
 * （类比 xv6 的 plicinithart）。当前 hartid 由 SBI 经 a0 传入内核。 */
#define PLIC_BASE          0x0c000000UL
#define PLIC_PRIORITY(src) (PLIC_BASE + 4UL * (uint64_t)(src))
#define PLIC_PENDING       (PLIC_BASE + 0x1000UL)
#define PLIC_ENABLE(ctx)   (PLIC_BASE + 0x2000UL + (uint64_t)(ctx) * 0x80UL)
#define PLIC_THRESHOLD(ctx)(PLIC_BASE + 0x200000UL + (uint64_t)(ctx) * 0x1000UL)
#define PLIC_CLAIM(ctx)    (PLIC_BASE + 0x200000UL + (uint64_t)(ctx) * 0x1000UL + 4UL)
#define PLIC_S_CTX(hart)   (2 * (int)(hart) + 1)   /* hartN 的 S-context 号 */

static inline uint32_t plic_read(uint64_t addr) {
    return *(volatile uint32_t *)addr;
}
static inline void plic_write(uint64_t addr, uint32_t v) {
    *(volatile uint32_t *)addr = v;
}

void plic_init(unsigned long hartid); /* 配置当前 hart 的 S-context（学生填使能位）*/
void plic_external_handler(void);     /* 外部中断处理：claim→读设备→complete（学生填）*/

/* ============ scause / sie 扩展（common/riscv.h 不含外部中断，故本地定义）============ */
#define SCAUSE_S_EXTERNAL 9UL      /* S 态外部中断 code */
#define SIE_SEIE (1UL << 9)        /* sie.SEIE：S 态外部中断使能 */

static inline void ext_irq_on(void)  { asm volatile("csrs sie, %0" :: "r"(SIE_SEIE)); }
static inline void ext_irq_off(void) { asm volatile("csrc sie, %0" :: "r"(SIE_SEIE)); }

/* ============ 共享统计（trap.c / plic.c 维护，main.c 判据读取）============ */
extern volatile uint64_t g_ext_traps;   /* 进入「外部中断」分支的次数（trap.c）*/
extern volatile uint64_t g_last_scause;  /* 最近一次 trap 的 scause（trap.c）*/
extern volatile uint32_t g_claim_irq;    /* 最近一次 claim 得到的 irq（plic.c）*/
extern volatile int      g_rx_got;       /* 成功读回 UART 字节的次数（plic.c）*/
extern volatile uint8_t  g_rx_byte;      /* 读回的字节值（plic.c）*/
extern volatile int      g_plic_ctx;     /* 当前 hart 的 S-context 号（plic_init 写）*/

#endif
