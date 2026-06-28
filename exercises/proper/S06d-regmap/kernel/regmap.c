/* S06d · 类型化寄存器图的设备操作（学生填空）。
 * 与 S06 的区别：不再 uart_reg_read(base, off)，而是直接对 struct 成员 / 位段 union 读写 - 
 * 寄存器图把「偏移 + 掩码」沉淀进类型（regmap.h 已给），这里只在「命名字段」上做读写。 */
#include "regmap.h"

#define SPIN_MAX 2000000

/* TODO(1): NS16550 回环自测 - 经 typed regmap 完成，1=回环字节一致。
 *   步骤（全用 struct 成员 / union 位段，别用裸指针 + 偏移）：
 *     · 存下 u->mcr；用 mcr_bits 把 .b.loop 置 1 写回 u->mcr（开回环）。
 *     · 对 "RV64" 每个字节：轮询 (lsr_bits){.raw=u->lsr}.b.thre 为 1 → 写 u->thr_rbr；
 *       再轮询 .b.dr 为 1 → 读回 u->thr_rbr 与原字节比对（不一致则置 ok=0）。
 *     · 最后把 u->mcr 写回原值（否则回环位会污染后续 SBI 控制台）。
 *   现在的占位恒返回 0：回环永不一致 → REGMAP_MISS，且 ns16550_probe 失败 → 不会 bind。 */
int ns16550_loopback(struct ns16550_regs *u) {
    (void)u;
    (void)SPIN_MAX;
    return 0; /* TODO: 经 typed regmap 实现回环自测 */
}

/* 经 typed regmap 轮询发送（/dev I/O 用，已给）：等 LSR.THRE 再写 THR。 */
void ns16550_emit(struct ns16550_regs *u, const char *s) {
    while (*s) {
        for (int spin = 0; spin < SPIN_MAX; spin++) {
            lsr_bits st = { .raw = u->lsr };
            if (st.b.thre) break;
        }
        u->thr_rbr = (uint8_t)*s++;
    }
}

/* TODO(2): 经 typed regmap 配置 PLIC 当前 S-context，再回读校验，1=ok。
 *   用三个 typed 子结构（regmap.h 已给）定位，别用裸地址算术：
 *     · plic_prio()->prio[irq] = 1;                              （源优先级非 0）
 *     · plic_enable(ctx)->word[irq/32] |= 1u << (irq % 32);      （使能该源）
 *     · plic_context(ctx)->threshold = 0;                        （放行所有 >0 优先级）
 *   然后回读这三处确认写进去了（任一不符返回 0）。
 *   现在的占位恒返回 0：PLIC 未配置 → REGMAP_MISS，且 plic_probe 失败。 */
int plic_regmap_config(int ctx, int irq) {
    (void)ctx;
    (void)irq;
    return 0; /* TODO: 经三个 typed 子结构配置 + 回读校验 */
}
