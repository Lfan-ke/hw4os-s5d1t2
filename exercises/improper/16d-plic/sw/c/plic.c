/* 16d · 核外中断 PLIC - C（学生填空版）。
 * 平台级共享外设中断路由器：priority/enable/threshold → claim/complete。
 * env=gcc-rv64：编成 riscv64 静态 ELF 跑在 qemu-user；claim 读 / complete 写用 `fence rw,rw` 定序。
 * 你只需补全 arbitrate()（仲裁器核心）；其余 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>

#define NSRC      4
#define MASK_ALL  0x1Eu /* 源 1..4 = bit1..bit4 */
#define ENABLE_M  0x0Eu /* 使能 {1,2,3} */
#define THRESH    1u

/* PLIC 寄存器背书（设备侧状态）：bit i = 源 i。 */
static volatile uint32_t prio[NSRC + 1];
static volatile uint32_t pending;
static volatile uint32_t enable_m;
static volatile uint32_t threshold;

static void plic_config(void) {
    prio[1] = 1; prio[2] = 2; prio[3] = 3; prio[4] = 3;
    enable_m = ENABLE_M;
    threshold = THRESH;
    pending = 0;
}

/* 设备侧组合仲裁器：取最高优先级的合格源，同优先级取最小 id。 */
static uint32_t arbitrate(void) {
    /* TODO: 合格源 eligible = pending & enable & (prio > threshold)；
     *   在所有合格源里取最高优先级（同优先级取最小 id，自底向上用 strict `>`）。
     *   HINT:
     *     uint32_t best = 0, bp = 0;
     *     for (uint32_t id = 1; id <= NSRC; id++) {
     *       if (!((pending >> id) & 1u)) continue;
     *       if (!((enable_m >> id) & 1u)) continue;
     *       uint32_t p = prio[id];
     *       if (p <= threshold) continue;
     *       if (p > bp) { bp = p; best = id; }
     *     }
     *     return best;
     */
    return 0; /* ← 占位：永远无源可路由 → ROUTE_FAIL */
}

static void plic_raise(uint32_t mask) { pending |= mask; }

/* claim：读 CLAIM 寄存器 = 取顶源并清其 pending（in-service）；fence 定序。 */
static uint32_t plic_claim(void) {
    uint32_t id = arbitrate();
    if (id) pending &= ~(1u << id);
    __asm__ volatile("fence rw,rw" ::: "memory");
    return id;
}
/* complete：写 CLAIM 寄存器 = EOI；toy 设备应答（fence 定序）。 */
static void plic_complete(uint32_t id) {
    __asm__ volatile("fence rw,rw" ::: "memory");
    (void)id;
}

static int level_route(void) {
    plic_config();
    plic_raise(MASK_ALL);
    uint32_t top = arbitrate();
    if (top != 3) return 0;
    printf("ROUTE_PASS top=%u prio=%u (max-priority arbitration)\n", top, prio[top]);
    return 1;
}

static int level_thresh(void) {
    plic_config(); plic_raise(1u << 1); /* 源1 prio1 ≤ 阈值1 */
    if (arbitrate() != 0) return 0;
    plic_config(); plic_raise(1u << 4); /* 源4 prio3 但未使能 */
    if (arbitrate() != 0) return 0;
    plic_config(); plic_raise(1u << 2); /* 源2 prio2 > 阈值且使能 */
    if (arbitrate() != 2) return 0;
    printf("THRESH_PASS threshold blocks s1, enable blocks s4, s2 eligible\n");
    return 1;
}

static int level_claim(void) {
    plic_config();
    plic_raise(MASK_ALL);
    uint32_t c1 = plic_claim(); plic_complete(c1);
    uint32_t c2 = plic_claim(); plic_complete(c2);
    uint32_t c3 = plic_claim(); plic_complete(c3);
    if (c1 != 3 || c2 != 2 || c3 != 0) return 0;
    if (pending != 0x12u) return 0; /* 余 {1,4} 被阈值/使能挡住 */
    plic_raise(1u << 3); /* 源3 重新触发 */
    uint32_t c4 = plic_claim(); plic_complete(c4);
    if (c4 != 3) return 0;
    printf("CLAIM_PASS seq=%u,%u,%u refire=%u\n", c1, c2, c3, c4);
    return 1;
}

int main(void) {
    int ok = 1;
    ok &= level_route();
    ok &= level_thresh();
    ok &= level_claim();
    if (ok) {
        printf("ALL_PASS\n");
        return 0;
    }
    printf("SOME_FAIL\n");
    return 1;
}
