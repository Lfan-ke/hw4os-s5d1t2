/* S06d · 类型化寄存器图的设备操作（参考解）。
 * 与 S06 的区别：不再 uart_reg_read(base, off)，而是直接对 struct 成员/位段 union 读写 - 
 * 寄存器图把「偏移 + 掩码」沉淀进类型，调用点只见命名字段。 */
#include "regmap.h"

#define SPIN_MAX 2000000

/* NS16550 回环自测：经 typed regmap 置 MCR.LOOP，逐字节写 THR、从 RBR 读回比对，最后恢复 MCR。
 * THRE/DR 用 union 位段解码（u->lsr 整字节 → lsr_bits.b.thre / .dr）。 */
int ns16550_loopback(struct ns16550_regs *u) {
    const char *msg = "RV64";
    uint8_t saved = u->mcr;
    mcr_bits m = { .raw = saved };
    m.b.loop = 1;
    u->mcr = m.raw;

    int ok = 1;
    for (int i = 0; msg[i]; i++) {
        int spin = 0;
        for (;;) {
            lsr_bits s = { .raw = u->lsr };
            if (s.b.thre) break;
            if (++spin > SPIN_MAX) { ok = 0; break; }
        }
        u->thr_rbr = (uint8_t)msg[i];
        spin = 0;
        for (;;) {
            lsr_bits s = { .raw = u->lsr };
            if (s.b.dr) break;
            if (++spin > SPIN_MAX) { ok = 0; break; }
        }
        if (u->thr_rbr != (uint8_t)msg[i]) ok = 0;
    }

    u->mcr = saved; /* 恢复，避免回环位污染后续控制台 */
    return ok;
}

/* 经 typed regmap 轮询发送（/dev I/O 用）：等 LSR.THRE 再写 THR。 */
void ns16550_emit(struct ns16550_regs *u, const char *s) {
    while (*s) {
        for (int spin = 0; spin < SPIN_MAX; spin++) {
            lsr_bits st = { .raw = u->lsr };
            if (st.b.thre) break;
        }
        u->thr_rbr = (uint8_t)*s++;
    }
}

/* 经 typed regmap 配置 PLIC 的当前 S-context：源优先级非 0、使能该源、阈值清零；随后回读校验。
 * 用三个 typed 子结构定位：plic_prio()->prio[irq] / plic_enable(ctx)->word[] / plic_context(ctx)->threshold。 */
int plic_regmap_config(int ctx, int irq) {
    plic_prio()->prio[irq] = 1;
    plic_enable(ctx)->word[irq / 32] |= (1u << (irq % 32));
    plic_context(ctx)->threshold = 0;

    if (plic_prio()->prio[irq] != 1) return 0;
    if (!(plic_enable(ctx)->word[irq / 32] & (1u << (irq % 32)))) return 0;
    if (plic_context(ctx)->threshold != 0) return 0;
    return 1;
}
