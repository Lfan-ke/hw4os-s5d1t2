/* S13 · 内核入口/测试驱动（学生填空）：hart0 用 SBI HSM 唤醒 hart1，
 * 经直接映射区共享邮箱握手，核对上线与槽位交换。 */
#include "smp.h"

/* 跨 hart 共享邮箱（内核 .bss → 直接映射，物理地址各 hart 共识）。 */
volatile struct smp_mbox g_mbox;

/* ===== hart1 主体：在这里写「hart1 入口逻辑」 ===== */
void hart1_main(uint64_t hartid) {
    (void)hartid; /* 占位：避免未使用告警；实现后删掉本行 */

    /* TODO(hart1 入口)：
     *   1) 报到：g_mbox.hart_id = hartid; smp_fence(); g_mbox.online = 1;
     *   2) 交换两槽位：a=g_mbox.slot[0]; b=g_mbox.slot[1];
     *                  g_mbox.slot[0]=b; g_mbox.slot[1]=a;
     *   3) 举完成旗：smp_fence(); g_mbox.slot_done = 1;
     *   注意每步「先写数据后举旗」之间用 smp_fence() 保证可见与顺序。
     * 现在留空：hart1 即便被唤醒也不上线 → 不会出 SMP_BOOT_PASS（但不死锁、不崩）。 */
}

void kmain(void) {
    kputs("\n[S13] multicore: SBI HSM hart_start + shared-slot handshake\n");

    /* 初始化邮箱：清上线标志，预置两个待交换槽位（在唤醒 hart1 之前写好）。 */
    g_mbox.online    = 0;
    g_mbox.hart_id   = 0;
    g_mbox.slot[0]   = SLOT_A;
    g_mbox.slot[1]   = SLOT_B;
    g_mbox.slot_done = 0;
    smp_fence(); /* 先把邮箱写出去，再唤醒 hart1 */

    kputs("hart0 online (boot hart), starting hart1 via sbi_hart_start...\n");
    /* ===== 在这里写「hart_start 调用」 ===== */
    /* TODO(hart_start)：唤醒 hartid=HART1_ID，入口=hart1_entry（物理地址），opaque=0：
     *   r = sbi_hart_start(HART1_ID, (uint64_t)hart1_entry, 0);
     * 现在用占位值 -1：hart1 不被唤醒 → 不会出 SMP_BOOT_PASS（但能编译、能跑、不死锁）。 */
    long r = -1;
    kputs("sbi_hart_start ret=");
    kputdec((uint64_t)r);
    console_putchar('\n');

    /* —— 等 hart1 上线（有界自旋，未唤醒也不死锁） —— */
    int boot_ok = wait_flag(&g_mbox.online, SPIN_LIMIT);
    if (boot_ok) {
        smp_fence();
        kputs("hart1 reported online, hartid=");
        kputdec(g_mbox.hart_id);
        console_putchar('\n');
        kputs("SMP_BOOT_PASS\n");
    } else {
        kputs("hart1 did not come online within bound\n");
    }

    /* —— 等槽位交换完成并核对（必须 hart1 已上线才有意义） —— */
    int slot_ok = 0;
    if (boot_ok && wait_flag(&g_mbox.slot_done, SPIN_LIMIT)) {
        smp_fence();
        kputs("after swap: slot[0]=");
        kputhex(g_mbox.slot[0]);
        kputs(" slot[1]=");
        kputhex(g_mbox.slot[1]);
        console_putchar('\n');
        slot_ok = (g_mbox.slot[0] == SLOT_B) && (g_mbox.slot[1] == SLOT_A);
    }
    if (slot_ok) {
        kputs("SLOT_PASS\n");
    } else {
        kputs("slot exchange not observed\n");
    }

    if (boot_ok && slot_ok) {
        kputs("ALL_PASS\n");
    }
    /* kmain 返回 → entry.S 调 k_shutdown，qemu 退出。 */
}
