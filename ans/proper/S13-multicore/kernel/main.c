/* S13 · 内核入口/测试驱动：hart0 用 SBI HSM 唤醒 hart1，
 * 经直接映射区共享邮箱握手，核对上线与槽位交换。 */
#include "smp.h"

/* 跨 hart 共享邮箱（内核 .bss → 直接映射，物理地址各 hart 共识）。 */
volatile struct smp_mbox g_mbox;

/* ===== hart1 主体（学生需实现的「hart1 入口」逻辑，参考解给出） ===== */
void hart1_main(uint64_t hartid) {
    /* 1) 报到：写下自身 hartid，再置上线标志（先写数据后举旗，配 fence）。 */
    g_mbox.hart_id = hartid;
    smp_fence();
    g_mbox.online = 1;

    /* 2) 槽位交换：把 hart0 预置的 slot[0]/slot[1] 互换。
     *    这两个槽位是同一物理单元，hart1 的写 hart0 能看到（靠 fence + volatile）。 */
    smp_fence();
    uint64_t a = g_mbox.slot[0];
    uint64_t b = g_mbox.slot[1];
    g_mbox.slot[0] = b;
    g_mbox.slot[1] = a;

    /* 3) 收尾旗：先把交换结果写出，再置完成标志，保证 hart0 读到的是交换后的值。 */
    smp_fence();
    g_mbox.slot_done = 1;
    /* 返回 hart1_entry 的 wfi 自旋。 */
}

void kmain(void) {
    /* 引导 hartid：OpenSBI 经 a0 传入；common/entry.S 在 call kmain 前不碰 a0，
       故必须在任何函数调用前最先捕获（否则 a0 被调用约定覆盖）。 */
    uint64_t boot_hart;
    asm volatile("mv %0, a0" : "=r"(boot_hart));
    /* -smp 下 OpenSBI 引导核不确定（可能是 hart 0/1/2…），不能写死「hart0 启 hart1」；
       改为唤醒一个确定不同于引导核的 target hart。 */
    uint64_t target = (boot_hart == 0) ? 1 : 0;

    kputs("\n[S13] multicore: SBI HSM hart_start + shared-slot handshake\n");
    kputs("boot hart = ");
    kputdec(boot_hart);
    kputs(", waking target hart = ");
    kputdec(target);
    console_putchar('\n');

    /* 初始化邮箱：清上线标志，预置两个待交换槽位（在唤醒 target 之前写好）。 */
    g_mbox.online    = 0;
    g_mbox.hart_id   = 0;
    g_mbox.slot[0]   = SLOT_A;
    g_mbox.slot[1]   = SLOT_B;
    g_mbox.slot_done = 0;
    smp_fence(); /* 先把邮箱写出去，再唤醒 target */

    kputs("boot hart online, starting target hart via sbi_hart_start...\n");
    /* ===== 「hart_start 调用」（参考解给出）：唤醒 target，入口=hart1_entry，opaque=0。 ===== */
    long r = sbi_hart_start(target, (uint64_t)hart1_entry, 0);
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
