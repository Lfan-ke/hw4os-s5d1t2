/* 板级入门 · BSP 与设备树 —— C。
 * 母题：BSP = 把"散落的硬编码板级常量"收敛成一层可替换的板级胶水；
 *       设备树（DT）= firmware<->OS 的稳定 ABI，让同一个 kmain 跑遍多块板。
 * 四段递进：
 *   1) bsp_probe   —— 硬编码 BSP 表（换块板就跑飞的痛）
 *   2) parse_dt    —— 删掉 match 表，改"读平面图"（设备树）
 *   3) driver_bind —— compatible 字符串匹配，驱动与硬件解耦
 *   4) parse_dt_v2 —— bootloader/DT 不变，OS 升级仍向后兼容
 * 你只需填四个函数体；下方测试 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define A_UART_BASE 0x10000000u
#define A_CLK_HZ    10000000u
#define B_UART_BASE 0x10020000u
#define B_CLK_HZ    50000000u
#define DEFAULT_CLK_HZ 10000000u
#define UART_WIN    0x1000u

#define MAXDEV  8
#define MAXPROP 8
#define MAXNODE 8
#define CAPSZ   64

typedef struct { uint32_t uart_base, clk_hz; } BoardConfig;

/* ── 用内存数组模拟 MMIO 总线（设备 = 一个地址窗口 + 捕获缓冲）── */
typedef struct {
    uint32_t base, size;
    uint8_t  captured[CAPSZ];
    int      caplen;
} Device;
typedef struct {
    Device devs[MAXDEV];
    int    ndev;
    int    faults;
} Bus;

static void bus_write8(Bus *b, uint32_t addr, uint8_t byte) {
    for (int i = 0; i < b->ndev; i++) {
        Device *d = &b->devs[i];
        if (addr >= d->base && addr < d->base + d->size) {
            if (addr == d->base && d->caplen < CAPSZ) d->captured[d->caplen++] = byte; /* THR */
            return;
        }
    }
    b->faults++; /* 地址不落在任何设备 → 写飞了 */
}
static Device *bus_dev_at(Bus *b, uint32_t base) {
    for (int i = 0; i < b->ndev; i++) if (b->devs[i].base == base) return &b->devs[i];
    return NULL;
}
static void make_uart_bus(Bus *b, uint32_t uart_base) {
    b->ndev = 0; b->faults = 0;
    b->devs[0].base = uart_base; b->devs[0].size = UART_WIN; b->devs[0].caplen = 0;
    b->ndev = 1;
}

static const char BANNER[] = "vlab-os\n";
#define BANNER_LEN 8

/* 通用内核入口：只认 cfg.uart_base，板级细节一概不知。 */
static void kmain(Bus *b, BoardConfig cfg) {
    for (int i = 0; i < BANNER_LEN; i++) bus_write8(b, cfg.uart_base, (uint8_t)BANNER[i]);
}
static int banner_ok(Device *d) {
    if (!d || d->caplen != BANNER_LEN) return 0;
    for (int i = 0; i < BANNER_LEN; i++) if (d->captured[i] != (uint8_t)BANNER[i]) return 0;
    return 1;
}

/* ── 设备树（mini-DT：扁平节点数组 + 命名属性，代替真实 FDT 二进制）── */
typedef struct { const char *name; uint32_t val; } DtProp;
typedef struct {
    const char *compatible;
    uint32_t    reg_base, reg_size, irq;
    DtProp      props[MAXPROP];
    int         nprop;
} DtNode;
typedef struct { DtNode nodes[MAXNODE]; int n; } Dtb;

static Dtb dtb_a(void) {
    Dtb d; d.n = 0;
    DtNode u = { "vlab,uart",  A_UART_BASE, UART_WIN, 1, { { "clock-frequency", A_CLK_HZ } }, 1 };
    DtNode t = { "vlab,timer", 0x02000000u, 0x1000u,  7, { { "clock-frequency", A_CLK_HZ } }, 1 };
    d.nodes[d.n++] = u; d.nodes[d.n++] = t;
    return d;
}
static Dtb dtb_b(void) {
    Dtb d; d.n = 0;
    DtNode u = { "vlab,uart",  B_UART_BASE, UART_WIN, 2, { { "clock-frequency", B_CLK_HZ } }, 1 };
    DtNode t = { "vlab,timer", 0x02000000u, 0x1000u,  7, { { "clock-frequency", B_CLK_HZ } }, 1 };
    d.nodes[d.n++] = u; d.nodes[d.n++] = t;
    return d;
}
/* 老 bootloader 产出的 DT：UART 节点无 clock-frequency，且带 v2 不认识的新属性。 */
static Dtb dtb_a_old(void) {
    Dtb d; d.n = 0;
    DtNode u = { "vlab,uart",  A_UART_BASE, UART_WIN, 1, { { "vlab,unknown-feature", 0xdeadu } }, 1 };
    DtNode t = { "vlab,timer", 0x02000000u, 0x1000u,  7, { { NULL, 0 } }, 0 };
    d.nodes[d.n++] = u; d.nodes[d.n++] = t;
    return d;
}

/* ── 驱动注册表 ── */
typedef struct { const char *compatible; int bound; } DriverRec;

/* ═══════════════ 四段核心逻辑（学生填）═══════════════ */

/* 1) 硬编码 BSP 表：board_id -> BoardConfig。 */
static BoardConfig bsp_probe(uint32_t board_id) {
    (void)board_id;
    /* TODO: 按 board_id 返回对应板的 BoardConfig：
     *   board 0 -> { A_UART_BASE, A_CLK_HZ }；board 1 -> { B_UART_BASE, B_CLK_HZ }。
     * 分支择一：
     *   // TODO[a] 用 switch(board_id) 静态表
     *   // ELSE[b] 用按 id 索引的数组 BOARDS[board_id] */
    BoardConfig c; c.uart_base = 0; c.clk_hz = 0; /* ← 占位：base=0 → 写飞 → 判 FAIL */
    return c;
}

/* 2) 用设备树替代硬编码：找 compatible=="vlab,uart" 的节点取 reg/clk。 */
static BoardConfig parse_dt(const Dtb *blob) {
    (void)blob;
    /* TODO: 遍历 blob->nodes，strcmp(compatible,"vlab,uart")==0 时：
     *   uart_base = reg_base；在 props 里找 name=="clock-frequency" 取 val -> clk_hz。
     * 分支择一：
     *   // TODO[a] 顺序扫描所有节点匹配 compatible
     *   // ELSE[b] 直接按已知偏移 nodes[0] 取（约定 uart 恒为首节点） */
    BoardConfig c; c.uart_base = 0; c.clk_hz = 0; /* ← 占位：判 FAIL */
    return c;
}

/* 3) compatible 字符串匹配：每节点 × 驱动表，相等则 probe，统计绑定数。 */
static int driver_bind(const Dtb *blob, DriverRec *regs, int nreg) {
    (void)blob; (void)regs; (void)nreg;
    /* TODO: 对每个 node，遍历 regs[0..nreg)；strcmp(regs[k].compatible, node.compatible)==0 则：
     *   regs[k].bound++; total++; break。返回 total。
     * HINT: 字符串相等即"这块驱动认领这个设备"（真实内核此处会 probe(reg, irq)）。 */
    return 0; /* ← 占位：0 绑定 → 判 FAIL */
}

/* 4) 向后兼容的 parse_dt_v2：clock-frequency 可选（缺失则默认）；未知属性跳过。 */
static BoardConfig parse_dt_v2(const Dtb *blob) {
    (void)blob;
    /* TODO: 同 parse_dt 找 uart_base；但 clk_hz 初值设为 DEFAULT_CLK_HZ，
     *   只有遇到 name=="clock-frequency" 才覆盖；其余未知属性跳过（不要报错）。
     * 分支择一：
     *   // TODO[a] 缺省值兜底：clk_hz 先置 DEFAULT_CLK_HZ，命中才改
     *   // ELSE[b] 显式标志：found 标记是否扫到，最后 found?val:DEFAULT_CLK_HZ */
    BoardConfig c; c.uart_base = 0; c.clk_hz = 0; /* ← 占位：判 FAIL */
    return c;
}

/* ═══════════════ 测试 harness（勿改）═══════════════ */

static int check_probe(const char *tag, uint32_t board_id, uint32_t want_base) {
    BoardConfig cfg = bsp_probe(board_id);
    Bus b; make_uart_bus(&b, want_base);
    kmain(&b, cfg); /* 同一个 kmain，换板只换 cfg */
    int ok = (b.faults == 0) && banner_ok(bus_dev_at(&b, want_base));
    if (ok) printf("PROBE_%s_PASS\n", tag);
    else    printf("PROBE_%s_FAIL board=%u base=0x%08x faults=%d\n", tag, board_id, cfg.uart_base, b.faults);
    return ok;
}

static int check_dt(const char *tag, const Dtb *blob, uint32_t want_base, uint32_t want_clk) {
    BoardConfig cfg = parse_dt(blob);
    Bus b; make_uart_bus(&b, want_base);
    kmain(&b, cfg);
    int ok = cfg.uart_base == want_base && cfg.clk_hz == want_clk
          && b.faults == 0 && banner_ok(bus_dev_at(&b, want_base));
    if (ok) printf("DT_%s_PASS\n", tag);
    else    printf("DT_%s_FAIL base=0x%08x(exp 0x%08x) clk=%u(exp %u) faults=%d\n",
                   tag, cfg.uart_base, want_base, cfg.clk_hz, want_clk, b.faults);
    return ok;
}

static int check_bind(const Dtb *blob) {
    DriverRec regs[2] = { { "vlab,uart", 0 }, { "vlab,timer", 0 } };
    int total = driver_bind(blob, regs, 2);
    int ok = 1;
    int uart_bound = regs[0].bound, timer_bound = regs[1].bound;
    if (uart_bound == 1) printf("BIND_uart_PASS\n");
    else { printf("BIND_uart_FAIL bound=%d\n", uart_bound); ok = 0; }
    if (timer_bound == 1) printf("BIND_timer_PASS\n");
    else { printf("BIND_timer_FAIL bound=%d\n", timer_bound); ok = 0; }
    if (total == blob->n) printf("BIND_PASS\n");
    else { printf("BIND_FAIL total=%d nodes=%d\n", total, blob->n); ok = 0; }
    return ok;
}

static int check_upgrade(void) {
    Dtb blob = dtb_a_old(); /* 老 DT，未改动 */
    BoardConfig cfg = parse_dt_v2(&blob);
    Bus b; make_uart_bus(&b, A_UART_BASE);
    kmain(&b, cfg);
    DriverRec regs[2] = { { "vlab,uart", 0 }, { "vlab,timer", 0 } };
    int total = driver_bind(&blob, regs, 2);
    int ok = cfg.uart_base == A_UART_BASE
          && cfg.clk_hz == DEFAULT_CLK_HZ /* 缺失 -> 兜底默认值 */
          && b.faults == 0 && banner_ok(bus_dev_at(&b, A_UART_BASE))
          && total == blob.n;
    if (ok) printf("UPGRADE_PASS\n");
    else    printf("UPGRADE_FAIL base=0x%08x clk=%u total=%d faults=%d\n",
                   cfg.uart_base, cfg.clk_hz, total, b.faults);
    return ok;
}

int main(void) {
    int all = 1;

    /* 1) 硬编码 BSP 表：同一 kmain 跑两块板 */
    all &= check_probe("A", 0, A_UART_BASE);
    all &= check_probe("B", 1, B_UART_BASE);

    /* 2) 用设备树替代硬编码：同一 parse_dt + kmain 喂两份 blob */
    { Dtb a = dtb_a(); all &= check_dt("A", &a, A_UART_BASE, A_CLK_HZ); }
    { Dtb b = dtb_b(); all &= check_dt("B", &b, B_UART_BASE, B_CLK_HZ); }

    /* 3) compatible 字符串匹配 -> 驱动绑定 */
    { Dtb a = dtb_a(); all &= check_bind(&a); }

    /* 4) bootloader/DT 不变，OS 升级（v2）仍可启动 */
    all &= check_upgrade();

    if (all) { printf("ALL_PASS\n"); return 0; }
    return 1;
}
