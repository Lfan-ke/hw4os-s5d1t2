/* 正经赛道共享：RISC-V S 态 CSR/位 辅助。 */
#ifndef OSLAB_RISCV_H
#define OSLAB_RISCV_H
#include <stdint.h>

#define SIE_STIE    (1UL << 5) /* sie.STIE：S 态时钟中断使能 */
#define SSTATUS_SIE (1UL << 1) /* sstatus.SIE：S 态全局中断使能 */

#define SCAUSE_INT_BIT   (1UL << 63)
#define SCAUSE_S_TIMER   5UL /* S 态时钟中断 code */

static inline uint64_t r_scause(void) { uint64_t x; asm volatile("csrr %0, scause" : "=r"(x)); return x; }
static inline uint64_t r_stval(void)  { uint64_t x; asm volatile("csrr %0, stval"  : "=r"(x)); return x; }
static inline uint64_t r_sepc(void)   { uint64_t x; asm volatile("csrr %0, sepc"   : "=r"(x)); return x; }
static inline uint64_t r_time(void)   { uint64_t x; asm volatile("rdtime %0"        : "=r"(x)); return x; }
static inline void w_stvec(uint64_t x)   { asm volatile("csrw stvec, %0"   :: "r"(x)); }
static inline void set_timer_irq(void)   { asm volatile("csrs sie, %0"     :: "r"(SIE_STIE)); }
static inline void intr_on(void)         { asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SIE)); }
static inline void intr_off(void)        { asm volatile("csrc sstatus, %0" :: "r"(SSTATUS_SIE)); }

#endif
