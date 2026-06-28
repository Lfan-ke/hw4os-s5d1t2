/* 16a · 总线与缓存 - C（学生填空版）。软件总线模型 = 地址区间译码分发 + cache 层 + 突发计时。
 * env=gcc-rv64：整程序编成 riscv64 静态 ELF 跑在 qemu-user，直通访问用真 `fence rw,rw`。
 * 你需填三处（其余 harness/设备模型/计时骨架勿改）：
 *   ① 仲裁区间判定 decode()（§2.2）   ② 直通判定 sensor/switch 的 *_passthrough()（§2.3 反例）
 *   ③ 突发分支 level_burst() 的握手次数（§2.3 时间模型）。
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
    /* TODO①: 仲裁 = 地址区间译码。按 §2.2 判 a 落哪段区间（`a>=BASE && a<END`），
     *   命中返回 DEV_REG/DEV_SENSOR/DEV_SWITCH，全落空返回 DEV_ERR。 */
    (void)a;
    return DEV_ERR; /* ← 占位：所有地址都判成总线错误 → 路由不符 → 无 ARB_PASS */
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

/* 设备后端（被内存背书的 toy 模型，勿改） */
static volatile uint32_t reg_mem;      /* regdev：可缓存载荷（类内存） */
static volatile uint32_t sensor_tick;  /* sensor：直通读自增（TEMP 每 tick 变） */
static volatile uint32_t switch_state; /* switchdev：写翻转副作用 */

static inline void barrier(void) { __asm__ volatile("fence rw,rw" ::: "memory"); }

/* 直通（uncached）：MMIO 非存储语义，每次都直达设备，fence 保证可见性/顺序。 */
static uint32_t sensor_read_passthrough(void) {
    /* TODO②: sensor 不可缓存，应每次直通取新值：barrier(); return ++sensor_tick; */
    barrier();
    return sensor_tick; /* ← 占位：未推进 tick → 两次读相同 → STALE（缓存了 MMIO 的错误） */
}
static void switch_write_passthrough(uint32_t v) {
    /* TODO②: switchdev 写有副作用，应直通下达：barrier(); switch_state ^= v & 1u; barrier(); */
    (void)v; /* ← 占位：写被吞 → 副作用没下达 → MISSED_SIDEEFFECT */
}

/* cache 层（仅可缓存区 regdev 走缓存，勿改） */
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
    uint32_t s_first = sensor_read_passthrough();
    uint32_t stale = s_first;
    uint32_t fresh = sensor_read_passthrough();
    if (fresh == stale) return 0;

    switch_state = 0;
    switch_write_passthrough(1u);
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
    /* TODO③: 突发分支。逐字节 = 每字节一次握手 → hs_byte 应为 PAYLOAD_N；
     *   突发（MODE=burst 且 ≥3 单位）整块只一次握手 → hs_burst 应为 1。SPEEDUP 须为 24。 */
    int hs_byte = 1;  /* ← 占位：应为 PAYLOAD_N（逐字节 24 次握手） */
    int hs_burst = 1; /* ← 占位：突发分支正确即为 1 */

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
