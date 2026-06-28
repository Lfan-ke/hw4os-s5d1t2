/* 16a · 总线与缓存 - C（参考解）。软件总线模型 = 地址区间译码分发 + cache 层 + 突发计时。
 * env=gcc-rv64：整程序编成 riscv64 静态 ELF 跑在 qemu-user，直通访问用真 `fence rw,rw`。
 *   ① 仲裁 = 地址区间译码（§2.2）  ② cache 暂存（regdev 类内存）
 *   ③ 直通 / uncached（sensor/switchdev 反例）  ④ 突发摊薄握手（nanosleep，§2.3）。
 */
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define REG_BASE 0x40000000u
#define REG_END  0x40001000u
#define SEN_BASE 0x40010000u
#define SEN_END  0x40010010u
#define SW_BASE  0x40020000u
#define SW_END   0x40020010u

enum { DEV_REG = 0, DEV_SENSOR = 1, DEV_SWITCH = 2, DEV_ERR = 3 };

static int decode(uint32_t a) {
    if (a >= REG_BASE && a < REG_END) return DEV_REG;
    if (a >= SEN_BASE && a < SEN_END) return DEV_SENSOR;
    if (a >= SW_BASE  && a < SW_END)  return DEV_SWITCH;
    return DEV_ERR;
}

static int level_arb(void) {
    static const uint32_t addrs[8] = {
        0x40000000u, 0x40000FFCu, 0x40010000u, 0x4001000Cu,
        0x40020000u, 0x40020008u, 0x40030000u, 0x3FFFFFFCu };
    static const int exp[8] = {
        DEV_REG, DEV_REG, DEV_SENSOR, DEV_SENSOR,
        DEV_SWITCH, DEV_SWITCH, DEV_ERR, DEV_ERR };
    for (int i = 0; i < 8; i++)
        if (decode(addrs[i]) != exp[i]) return 0;
    printf("ARB_PASS bus arbitration = address-range decode (8/8 routed)\n");
    return 1;
}

/* 设备后端（被内存背书的 toy 模型） */
static volatile uint32_t reg_mem;      /* regdev：可缓存载荷（类内存） */
static volatile uint32_t sensor_tick;  /* sensor：每次直通读 +1（TEMP 每 tick 变） */
static volatile uint32_t switch_state; /* switchdev：写翻转副作用 */

static inline void barrier(void) { __asm__ volatile("fence rw,rw" ::: "memory"); }

/* 直通（uncached）：MMIO 非存储语义，每次都直达设备，fence 保证可见性/顺序 */
static uint32_t sensor_read_passthrough(void) { barrier(); return ++sensor_tick; }
static void switch_write_passthrough(uint32_t v) { barrier(); switch_state ^= v & 1u; barrier(); }

/* cache 层：仅可缓存区（regdev）走缓存；一行直接映射 + 写直达的最小模型 */
static int reg_cache_valid;
static uint32_t reg_cache_val;
static int dev_reads;

static uint32_t reg_read_cached(void) {
    if (reg_cache_valid) return reg_cache_val;   /* HIT */
    dev_reads++;                                  /* MISS → 取设备 */
    reg_cache_val = reg_mem;
    reg_cache_valid = 1;
    return reg_cache_val;
}

static int level_cache(void) {
    reg_mem = 0xA5u; reg_cache_valid = 0; dev_reads = 0;
    uint32_t r0 = reg_read_cached();   /* miss */
    uint32_t r1 = reg_read_cached();   /* hit  */
    if (r0 != r1 || dev_reads != 1) return 0;

    sensor_tick = 0;
    uint32_t s_first = sensor_read_passthrough();  /* tick=1 */
    uint32_t stale = s_first;                       /* 若缓存：第二次仍返回 s_first */
    uint32_t fresh = sensor_read_passthrough();    /* tick=2：直通取到新值 */
    if (fresh == stale) return 0;

    switch_state = 0;
    switch_write_passthrough(1u);                   /* 直通：副作用下达 */
    int switched = (switch_state == 1u);
    if (!switched) return 0;

    printf("CACHE regdev: miss->hit consistent, 1 device read saved (cacheable=memory-like)\n");
    printf("CACHE sensor: passthrough FRESH=%u (cached would be STALE=%u)\n", fresh, stale);
    printf("CACHE switchdev: passthrough SWITCHED state=%d (cached would be MISSED_SIDEEFFECT)\n", switched);
    printf("UNCACHED sensor/switchdev bypass cache: MMIO != memory (why volatile/fence)\n");
    printf("CACHE_PASS\n");
    return 1;
}

#define HANDSHAKE_NS 200000000L /* 单次总线握手 = 0.2s（§2.3） */
#define PAYLOAD_N    24         /* 载荷 24B */

static void handshake(void) { struct timespec t = {0, HANDSHAKE_NS}; nanosleep(&t, 0); }
static double now_s(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static int level_burst(void) {
    int hs_byte = PAYLOAD_N;  /* 逐字节：每字节一次握手 */
    int hs_burst = 1;         /* 突发（MODE=burst 且 ≥3 单位）：整块一次握手 */

    double a = now_s();
    for (int i = 0; i < hs_byte; i++) handshake();
    double byte_meas = now_s() - a;

    a = now_s();
    for (int i = 0; i < hs_burst; i++) handshake();
    double burst_meas = now_s() - a;

    double byte_t = hs_byte * 0.2, burst_t = hs_burst * 0.2;
    int speedup = hs_byte / hs_burst;

    if (byte_meas < 4.0 || burst_meas > 1.0 || byte_meas < burst_meas * 5.0) return 0;
    if (speedup != 24) return 0;

    printf("BURST byte-by-byte=%d handshakes  burst=%d handshake (>=3 units)\n", hs_byte, hs_burst);
    printf("BYTE_T=%.1f BURST_T=%.1f SPEEDUP=%d\n", byte_t, burst_t, speedup);
    printf("BURST_PASS\n");
    return 1;
}

int main(void) {
    int ok = 1;
    ok &= level_arb();
    ok &= level_cache();
    ok &= level_burst();
    if (ok) {
        printf("ALL_PASS\n");
        return 0;
    }
    printf("SOME_FAIL\n");
    return 1;
}
