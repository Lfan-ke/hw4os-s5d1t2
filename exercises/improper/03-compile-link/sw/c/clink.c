/* 编译链接（软件建模）—— C。
 * 母题：编译器只产出"带名字的碎片(section)"，是手写链接脚本决定
 *   「哪段落在哪个地址、谁先谁后、要不要塞进同一颗镜像」。
 * 本课用纯软件数组/结构体把链接器的工作建模出来，逐题递进：
 *   E1 段布局与边界符号  →  E2 自定义段 .config 落到对齐地址
 *   E3 ELF 头解析 vs 纯二进制入口偏移  →  E4 A→B→C 串接执行（app 表）
 * 学生只填带 // TODO 的函数体；下方测试 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define BASE          0x80200000ULL  /* 裸机加载基址（rcore BASE_ADDRESS） */
#define N_SEC         4              /* .text .rodata .data .bss */
#define CONFIG_MAGIC  0x00C0FFEEu
#define PAGE          0x1000ULL

typedef struct { uint32_t magic, version, nslots, flags; } Config;

/* 向上对齐到 a（a 为 2 的幂）。链接脚本的 `. = ALIGN(a);`。E2 的 place_config 会用到它。 */
static uint64_t align_up(uint64_t x, uint64_t a) __attribute__((unused));
static uint64_t align_up(uint64_t x, uint64_t a) { return (x + a - 1) & ~(a - 1); }

/* ════════════════ 学生填空区（四段核心逻辑）════════════════ */

/* E1：段布局。按 .text→.rodata→.data→.bss 顺序连续摆放，写入 starts[]。 */
static void layout(uint64_t base, const uint64_t sizes[N_SEC], uint64_t starts[N_SEC]) {
    /* TODO: starts[0]=base; starts[i]=starts[i-1]+sizes[i-1]; */
    (void)sizes;
    for (int i = 0; i < N_SEC; i++) starts[i] = base; /* 占位：全堆 base，不递增，判 FAIL */
}

/* E1：.bss 零填充——把 buf 的前 len 字节清零（模拟启动代码 memset(__bss_start..__bss_end,0)）。 */
static void clear_bss(uint8_t *buf, int len) {
    /* TODO: 把 buf[0..len) 全置 0。HINT: memset(buf, 0, len)。 */
    (void)buf; (void)len; /* 占位：未清零，.bss 仍为脏值，判 FAIL */
}

/* E2：把自定义段 .config 落到 4K 对齐地址，返回 __config_start。 */
static uint64_t place_config(uint64_t prev_end) {
    /* TODO: 向上对齐到 PAGE。HINT: align_up(prev_end, PAGE)。 */
    return prev_end; /* 占位：未对齐到 4K，判 FAIL */
}

/* E2：把 __attribute__((section(".config"))) 的结构体内容"编进"段里。 */
static Config make_config(void) {
    /* TODO: 填约定字段 magic=CONFIG_MAGIC, version=1, nslots=8, flags=0b101。 */
    Config c = { 0, 0, 0, 0 }; /* 占位：魔数为 0，判 FAIL */
    return c;
}

/* E3(a)：解析 ELF64 头部前 64 字节。magic=7F 45 4C 46；e_entry@24；e_phoff@32（小端）。
 *   通过 entry/phoff 指针回填，返回 magic 是否正确(1/0)。 */
static int parse_elf(const uint8_t *buf, int n, uint64_t *entry, uint64_t *phoff) {
    /* TODO: 校验魔数；小端从偏移 24/32 各读 8 字节回填 entry、phoff。 */
    (void)buf; (void)n;
    *entry = 0; *phoff = 0; /* 占位：未解析 */
    return 0;               /* 占位：magic 视为不通过，判 FAIL */
}

/* E3(b)：纯二进制要求 link 地址 == load 地址，返回链接基址。 */
static uint64_t bin_base(void) {
    /* TODO: 返回与 BASE 一致的链接基址。 */
    return 0; /* 占位：基址错误（!= load 地址），判 FAIL */
}

/* E3(b)：纯二进制入口相对镜像起点偏移 = entry - base。 */
static uint64_t bin_entry_offset(uint64_t base, uint64_t entry) {
    /* TODO: 返回 entry - base。 */
    (void)base; (void)entry;
    return 1; /* 占位：偏移非 0，判 FAIL */
}

/* E4：runner——按 .apps 段内顺序逐个调用 app，把标签写入 out[]，返回个数。
 *   // TODO[a] 函数指针表（本实现）  // ELSE[b] .incbin 原始二进制顺序跳转（贴近 rcore）。 */
typedef const char *(*app_fn)(void);
static int run_apps(const app_fn *table, int n, const char **out) {
    /* TODO: 按 table 顺序调用每个 app，依次写入 out[]。 */
    (void)table; (void)n; (void)out;
    return 0; /* 占位：未拉起任何 app，判 FAIL */
}

/* ─── E4：三个 app（给定，勿改）—— 被收进 .apps 表 ─── */
static const char *app_a(void) { return "APP_A"; }
static const char *app_b(void) { return "APP_B"; }
static const char *app_c(void) { return "APP_C"; }

/* ════════════════ 测试 harness（勿改）════════════════ */

static int check_layout(void) {
    uint64_t sizes[N_SEC] = { 0x180, 0x40, 0x80, 0x200 };
    uint64_t starts[N_SEC];
    layout(BASE, sizes, starts);
    int ok = 1;
    if (starts[0] != BASE) { printf("LAYOUT_FAIL .text 应钉在 %#llx，实为 %#llx\n", BASE, (unsigned long long)starts[0]); ok = 0; }
    for (int i = 1; i < N_SEC; i++) {
        if (starts[i] <= starts[i - 1]) { printf("LAYOUT_FAIL 段地址非递增 sec%d=%#llx <= sec%d=%#llx\n", i, (unsigned long long)starts[i], i - 1, (unsigned long long)starts[i - 1]); ok = 0; }
        if (starts[i] != starts[i - 1] + sizes[i - 1]) { printf("LAYOUT_FAIL 段未连续 sec%d=%#llx 应=%#llx\n", i, (unsigned long long)starts[i], (unsigned long long)(starts[i - 1] + sizes[i - 1])); ok = 0; }
    }
    uint64_t rodata_const = starts[1] + 0x10;
    if (!(starts[1] <= rodata_const && rodata_const < starts[2])) { printf("LAYOUT_FAIL 只读常量 %#llx 未落在 .rodata 区间\n", (unsigned long long)rodata_const); ok = 0; }
    uint8_t bss[0x200];
    for (int i = 0; i < (int)sizeof(bss); i++) bss[i] = 0xAA; /* 预置脏值，看是否被清零 */
    clear_bss(bss, (int)sizes[3]);
    for (int i = 0; i < (int)sizes[3]; i++) if (bss[i] != 0) { printf("LAYOUT_FAIL .bss 未清零 @%d\n", i); ok = 0; break; }
    if (ok) printf("LAYOUT_PASS\n");
    return ok;
}

static int check_section(void) {
    uint64_t prev_end = BASE + 0x4A0; /* 故意非 4K 对齐 */
    uint64_t cstart = place_config(prev_end);
    Config c = make_config();
    int ok = 1;
    if (cstart % PAGE != 0) { printf("SECTION_FAIL __config_start=%#llx 未 4K 对齐\n", (unsigned long long)cstart); ok = 0; }
    if (cstart < prev_end) { printf("SECTION_FAIL __config_start=%#llx 不应回退到 %#llx 之前\n", (unsigned long long)cstart, (unsigned long long)prev_end); ok = 0; }
    if (!(c.magic == CONFIG_MAGIC && c.version == 1 && c.nslots == 8 && c.flags == 0x5)) {
        printf("SECTION_FAIL .config 内容不符 magic=%#x ver=%u nslots=%u flags=%#x\n", c.magic, c.version, c.nslots, c.flags); ok = 0;
    }
    if (ok) printf("SECTION_PASS\n");
    return ok;
}

static void fake_elf_header(uint64_t entry, uint64_t phoff, uint8_t *h) {
    memset(h, 0, 64);
    h[0] = 0x7F; h[1] = 0x45; h[2] = 0x4C; h[3] = 0x46;
    h[4] = 2; h[5] = 1; /* ELFCLASS64 + little-endian */
    for (int i = 0; i < 8; i++) h[24 + i] = (uint8_t)(entry >> (8 * i));
    for (int i = 0; i < 8; i++) h[32 + i] = (uint8_t)(phoff >> (8 * i));
}

static int check_elf(void) {
    uint64_t want_entry = BASE, want_phoff = 64;
    uint8_t hdr[64];
    fake_elf_header(want_entry, want_phoff, hdr);
    uint64_t entry = 0, phoff = 0;
    int magic_ok = parse_elf(hdr, 64, &entry, &phoff);
    int ok = 1;
    if (!magic_ok) { printf("ELF_FAIL 魔数校验失败（应 7F 45 4C 46）\n"); ok = 0; }
    if (entry != want_entry) { printf("ELF_FAIL e_entry=%#llx 应=%#llx\n", (unsigned long long)entry, (unsigned long long)want_entry); ok = 0; }
    if (phoff != want_phoff) { printf("ELF_FAIL e_phoff=%llu 应=%llu\n", (unsigned long long)phoff, (unsigned long long)want_phoff); ok = 0; }
    if (ok) printf("ELF_PASS\n");
    return ok;
}

static int check_bin(void) {
    uint64_t load_addr = BASE, entry = BASE;
    uint64_t base = bin_base();
    uint64_t off = bin_entry_offset(base, entry);
    int ok = 1;
    if (base != load_addr) { printf("BIN_FAIL link 基址=%#llx 必须 == load 地址=%#llx\n", (unsigned long long)base, (unsigned long long)load_addr); ok = 0; }
    if (off != 0) { printf("BIN_FAIL .bin 入口偏移=%#llx 应=0（link==load 才成立）\n", (unsigned long long)off); ok = 0; }
    if (ok) printf("BIN_PASS\n");
    return ok;
}

static int check_chain(void) {
    app_fn table[3] = { app_a, app_b, app_c };
    const char *out[3];
    int m = run_apps(table, 3, out);
    const char *want[3] = { "APP_A", "APP_B", "APP_C" };
    int ok = 1;
    if (m != 3) { printf("CHAIN_FAIL app 数=%d 应=3\n", m); ok = 0; }
    else for (int i = 0; i < 3; i++) if (strcmp(out[i], want[i]) != 0) {
        printf("CHAIN_FAIL 第%d个 app=%s 应=%s（出现顺序须 == 段内顺序）\n", i, out[i], want[i]); ok = 0;
    }
    if (ok) { for (int i = 0; i < 3; i++) printf("%s\n", out[i]); printf("CHAIN_PASS\n"); }
    return ok;
}

int main(void) {
    int all = 1;
    all &= check_layout();
    all &= check_section();
    all &= check_elf();
    all &= check_bin();
    all &= check_chain();
    if (all) { printf("ALL_PASS\n"); return 0; }
    return 1;
}
