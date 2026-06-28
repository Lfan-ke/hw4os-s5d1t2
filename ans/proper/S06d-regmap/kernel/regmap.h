/* S06d · 类型化寄存器图（给定）：把 S06/S06c 的裸指针 + #define 偏移升级为
 * struct + union 的「寄存器图」。NS16550 字节寄存器铺成 struct（成员偏移=寄存器偏移），
 * 位段用 union{raw; 位域} 解码（呼应 16b 的 C 第③级）；PLIC 太大，按区间拆成 typed 子结构。
 * 这正是 Rust tock-registers register_structs!/register_bitfields! 在 C 里的同构写法。 */
#ifndef S06D_REGMAP_H
#define S06D_REGMAP_H
#include <stdint.h>
#include <stddef.h>

/* ===== NS16550 UART 类型化寄存器图（qemu virt：reg-shift=0，字节对齐）===== */
#define UART0_BASE 0x10000000UL

struct ns16550_regs {
    volatile uint8_t thr_rbr; /* 0x00 W:THR R:RBR（DLAB=1 时为 DLL） */
    volatile uint8_t ier;     /* 0x01 中断使能（DLAB=1 时为 DLM） */
    volatile uint8_t iir_fcr; /* 0x02 R:IIR W:FCR */
    volatile uint8_t lcr;     /* 0x03 线路控制（bit7=DLAB） */
    volatile uint8_t mcr;     /* 0x04 modem 控制（bit4=LOOP） */
    volatile uint8_t lsr;     /* 0x05 线路状态（bit0=DR bit5=THRE） */
    volatile uint8_t msr;     /* 0x06 modem 状态 */
    volatile uint8_t scr;     /* 0x07 便签 */
};
_Static_assert(sizeof(struct ns16550_regs) == 8, "ns16550 regmap 必须逐字节铺满");
_Static_assert(offsetof(struct ns16550_regs, lcr) == 3, "LCR 偏移=3");
_Static_assert(offsetof(struct ns16550_regs, mcr) == 4, "MCR 偏移=4");
_Static_assert(offsetof(struct ns16550_regs, lsr) == 5, "LSR 偏移=5");

/* 位段解码 union（raw 整字节 ↔ 命名位域，逐位镜像）。 */
typedef union {
    uint8_t raw;
    struct { uint8_t dr : 1, oe : 1, pe : 1, fe : 1, bi : 1, thre : 1, temt : 1, rxfe : 1; } b;
} lsr_bits;
typedef union {
    uint8_t raw;
    struct { uint8_t dtr : 1, rts : 1, out1 : 1, out2 : 1, loop : 1, rsv : 3; } b;
} mcr_bits;

static inline struct ns16550_regs *ns16550_at(uint64_t base) {
    return (struct ns16550_regs *)(uintptr_t)base;
}

/* ===== PLIC 类型化寄存器图（qemu virt @ 0x0c000000）=====
 * PLIC 地址窗口达 64MB，整窗写成一个 struct 不现实；按「功能区间」拆成三个 typed 子结构，
 * 由基址 + 区间偏移 + 上下文号定位（与 register_structs! 里大间隔的分段同构）。
 *   priority[src]            = BASE + 0x000000 + 4*src
 *   enable[ctx].word[src/32] = BASE + 0x002000 + ctx*0x80（源 src 在第 src%32 位）
 *   context[ctx].{threshold, claim} = BASE + 0x200000 + ctx*0x1000
 * context 编号：每 hart 两个，hartN 的 S-context = 2N+1。 */
#define PLIC_BASE 0x0c000000UL
#define UART0_IRQ 10

struct plic_priority { volatile uint32_t prio[1024]; };          /* +0x000000 */
struct plic_enable   { volatile uint32_t word[32]; };            /* +0x002000 + ctx*0x80 */
struct plic_context  { volatile uint32_t threshold, claim; };    /* +0x200000 + ctx*0x1000 */
_Static_assert(sizeof(struct plic_enable) == 0x80, "每 context 的 enable 位图块=0x80");

static inline struct plic_priority *plic_prio(void) {
    return (struct plic_priority *)(uintptr_t)(PLIC_BASE + 0x000000UL);
}
static inline struct plic_enable *plic_enable(int ctx) {
    return (struct plic_enable *)(uintptr_t)(PLIC_BASE + 0x002000UL + (uint64_t)ctx * 0x80UL);
}
static inline struct plic_context *plic_context(int ctx) {
    return (struct plic_context *)(uintptr_t)(PLIC_BASE + 0x200000UL + (uint64_t)ctx * 0x1000UL);
}
#define PLIC_S_CTX(hart) (2 * (int)(hart) + 1)

/* regmap.c：类型化 regmap 的设备操作（学生在 EX 里填 TODO）。 */
int  ns16550_loopback(struct ns16550_regs *u);            /* 1=经 typed regmap 回环字节一致 */
void ns16550_emit(struct ns16550_regs *u, const char *s); /* 经 typed regmap 轮询发送 */
int  plic_regmap_config(int ctx, int irq);                /* 经 typed regmap 配置并回读校验，1=ok */

#endif
