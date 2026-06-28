/* 16b · 寄存器模型 - C（参考解，自底向上四级）。
 * 同一张 regdev 寄存器表，用四种由低到高的访问手法逐位转写同一段读写 trace：
 *   ① 裸 rv 汇编 lw/sw/fence  ② volatile 命名寄存器  ③ struct 位域+union  ④ inline-asm 访问宏。
 * env=gcc-rv64：整程序编成 riscv64 静态 ELF 跑在 qemu-user，故 ① ④ 是真 lw/sw/fence。
 * regdev 是一段“被内存背书的寄存器文件”（16b 考的是抄布局/逐位一致，不是设备活动）。
 */
#include <stdio.h>
#include <stdint.h>

#define OFF_CTRL   0x00u
#define OFF_STATUS 0x04u
#define OFF_DATA   0x08u
#define OFF_ID     0x0Cu
#define REG_MAGIC  0x52454744u /* "REGD" */

/* 同一段 trace 的预期值（四级 + 四语逐位一致）： */
#define TRACE_CTRL   0x0000000Bu /* EN=1 IE=1 MODE=2(solid) RST=0 → bit0|bit1|bit3 */
#define TRACE_STATUS 0x00000005u /* READY=1 BUSY=0 IRQ=1            → bit0|bit2     */
#define TRACE_BYTE   0x000000A5u /* DATA.BYTE[7:0]                                 */

/* regdev 背书存储：四个 u32 寄存器窗口。w[] 与命名寄存器 r 经 union 合法别名。 */
typedef struct { volatile uint32_t ctrl, status, data, id; } Regs;
static union { uint32_t w[4]; Regs r; } dev;

static void dev_reset(void) {
    dev.w[0] = dev.w[1] = dev.w[2] = 0;
    dev.w[3] = REG_MAGIC; /* ID 只读 magic */
}
/* 设备把 RO 的 STATUS 由 CTRL 推导出来（READY=EN，IRQ=EN&IE）。 */
static void dev_sync(void) {
    uint32_t c = dev.w[0], en = c & 1u, ie = (c >> 1) & 1u;
    dev.w[1] = (en ? 1u : 0u) | ((en && ie) ? 4u : 0u);
}

/* ① 裸 rv 汇编：load/store 的本相 = 算地址(base+off) + lw/sw，fence 管内存序。 */
static inline uint32_t raw_lw(uintptr_t a) {
    uint32_t v;
    __asm__ volatile("fence rw,rw\n\t lw %0,0(%1)" : "=r"(v) : "r"(a) : "memory");
    return v;
}
static inline void raw_sw(uintptr_t a, uint32_t v) {
    __asm__ volatile("sw %0,0(%1)\n\t fence rw,rw" : : "r"(v), "r"(a) : "memory");
}

static int level_raw(void) {
    uintptr_t b = (uintptr_t)&dev;
    dev_reset();
    if (raw_lw(b + OFF_ID) != REG_MAGIC) return 0;
    raw_sw(b + OFF_CTRL, TRACE_CTRL);
    dev_sync();
    if (raw_lw(b + OFF_STATUS) != TRACE_STATUS) return 0;
    raw_sw(b + OFF_DATA, TRACE_BYTE);
    if ((dev.w[2] & 0xFFu) != TRACE_BYTE) return 0;
    printf("RAW_PASS  rv lw/sw/fence: ID=%08X CTRL=%08X STATUS=%08X\n",
           REG_MAGIC, TRACE_CTRL, TRACE_STATUS);
    return 1;
}

/* ② volatile 命名寄存器：把 base+off 升级为 dev.r.ctrl 这样的具名 readl/writel。 */
static int level_vol(void) {
    dev_reset();
    if (dev.r.id != REG_MAGIC) return 0;
    dev.r.ctrl = TRACE_CTRL;
    dev_sync();
    if (dev.r.status != TRACE_STATUS) return 0;
    dev.r.data = TRACE_BYTE;
    if ((dev.w[2] & 0xFFu) != TRACE_BYTE) return 0;
    printf("VOL_PASS  volatile 具名寄存器：r.ctrl/r.status/r.data\n");
    return 1;
}

/* ③ struct 位域 + union：把整字拆成具名字段，raw 与 fields 同一存储。 */
typedef union {
    uint32_t raw;
    struct { uint32_t en : 1, ie : 1, mode : 2, rsv0 : 4, rst : 1, rsv1 : 23; } f;
} CtrlReg;
typedef union {
    uint32_t raw;
    struct { uint32_t ready : 1, busy : 1, irq : 1, rsv : 29; } f;
} StatusReg;

static int level_struct(void) {
    dev_reset();
    if (dev.r.id != REG_MAGIC) return 0;
    CtrlReg c = {0};
    c.f.en = 1; c.f.ie = 1; c.f.mode = 2; /* solid */
    dev.r.ctrl = c.raw;
    dev_sync();
    StatusReg s = {.raw = dev.r.status};
    if (c.raw != TRACE_CTRL || !s.f.ready || s.f.busy || !s.f.irq) return 0;
    printf("STRUCT_PASS 位域+union：EN=%u IE=%u MODE=%u | READY=%u IRQ=%u\n",
           c.f.en, c.f.ie, c.f.mode, s.f.ready, s.f.irq);
    return 1;
}

/* ④ inline-asm 访问宏：仿 Linux readl/writel，把 lw/sw 封进宏。 */
#define readl(a)     ({ uint32_t __v; __asm__ volatile("lw %0,0(%1)" : "=r"(__v) : "r"((uintptr_t)(a)) : "memory"); __v; })
#define writel(v, a) do { __asm__ volatile("sw %0,0(%1)" : : "r"((uint32_t)(v)), "r"((uintptr_t)(a)) : "memory"); } while (0)
#define REG(off)     ((uintptr_t)&dev + (off))

static int level_macro(void) {
    dev_reset();
    if (readl(REG(OFF_ID)) != REG_MAGIC) return 0;
    writel(TRACE_CTRL, REG(OFF_CTRL));
    dev_sync();
    if (readl(REG(OFF_STATUS)) != TRACE_STATUS) return 0;
    writel(TRACE_BYTE, REG(OFF_DATA));
    if ((dev.w[2] & 0xFFu) != TRACE_BYTE) return 0;
    printf("MACRO_PASS readl/writel 宏：封装 lw/sw\n");
    return 1;
}

/* capstone：raw↔struct 逐位镜像 - 从字段重建整字必须等于原始 raw。 */
static int level_mirror(void) {
    CtrlReg c = {.raw = TRACE_CTRL};
    uint32_t cr = (uint32_t)c.f.en | ((uint32_t)c.f.ie << 1) |
                  ((uint32_t)c.f.mode << 2) | ((uint32_t)c.f.rst << 8);
    StatusReg s = {.raw = TRACE_STATUS};
    uint32_t sr = (uint32_t)s.f.ready | ((uint32_t)s.f.busy << 1) | ((uint32_t)s.f.irq << 2);
    if (cr != c.raw || sr != s.raw || c.f.mode != 2) return 0;
    printf("MIRROR_PASS raw↔struct 逐位一致：CTRL=%08X STATUS=%08X\n", cr, sr);
    return 1;
}

int main(void) {
    int ok = 1;
    ok &= level_raw();
    ok &= level_vol();
    ok &= level_struct();
    ok &= level_macro();
    ok &= level_mirror();
    if (ok) {
        printf("ALL_PASS\n");
        return 0;
    }
    printf("SOME_FAIL\n");
    return 1;
}
