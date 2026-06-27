/* S13 · 多核：跨 hart 共享邮箱 + SBI HSM hart_start 封装（给定）。 */
#ifndef S13_SMP_H
#define S13_SMP_H
#include <stdint.h>
#include "kernel.h"

/* —— SBI HSM 扩展 —— EID = "HSM" 的 ASCII = 0x48534D；fid 0 = sbi_hart_start。
 * 约定：sbi_hart_start(hartid, start_addr, opaque)：让目标 hart 从 start_addr
 * 以 S 态、satp=0 开始执行，入口处 a0=hartid、a1=opaque。返回 0 成功。 */
#define SBI_EID_HSM   0x48534DL
#define SBI_HSM_START 0L

#define HART1_ID   1UL          /* qemu virt + OpenSBI：hart0 为引导核，唤醒 hart1 */
#define SLOT_A     0xA5A5UL     /* 待交换槽位初值 */
#define SLOT_B     0x5A5AUL
#define SPIN_LIMIT 200000000UL  /* 有界自旋上限：hart1 不上线时也不会死锁 */

/* 跨 hart 共享邮箱。放在内核 .bss —— 无分页(satp=0)下其地址即物理地址，
 * 各 hart 对「同一物理单元」达成共识；详见 README「为何用直接映射区」。 */
struct smp_mbox {
    uint64_t online;     /* hart1 上线后置 1（hart0 轮询） */
    uint64_t hart_id;    /* hart1 写入自身 hartid */
    uint64_t slot[2];    /* 待 hart1 交换的两个槽位 */
    uint64_t slot_done;  /* hart1 完成交换后置 1 */
};
extern volatile struct smp_mbox g_mbox;

void hart1_entry(void);            /* hart1.S：SBI 跳入的裸入口（设栈→调 hart1_main） */
void hart1_main(uint64_t hartid);  /* C：hart1 主体 */

static inline long sbi_hart_start(uint64_t hartid, uint64_t start_addr, uint64_t opaque) {
    return sbi_call(SBI_EID_HSM, SBI_HSM_START, (long)hartid, (long)start_addr, (long)opaque);
}

/* 全屏障：保证邮箱写在唤醒/读取之前对另一 hart 可见（多核可见性与顺序）。 */
static inline void smp_fence(void) { asm volatile("fence rw, rw" ::: "memory"); }

/* 有界自旋等待 *p 变非零；命中返回 1，超时返回 0（避免未唤醒时死锁）。 */
static inline int wait_flag(volatile uint64_t *p, uint64_t limit) {
    for (uint64_t i = 0; i < limit; i++) {
        if (*p) return 1;
        smp_fence();
    }
    return 0;
}

#endif
