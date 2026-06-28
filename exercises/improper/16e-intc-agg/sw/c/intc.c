/* 16e · 中断聚合 + 多核仲裁 - C（学生填空版，规矩·自底向上）。
 * CLINT(IPI) + PLIC(外设) + 多核 claim 竞争：同一 IRQ 源对 N 个 hart-context，
 * claim/complete 的 gateway 保证只一个 hart 处理；IPI 跨核协调，barrier 管内存序。
 * env=gcc-rv64：整程序编成 riscv64 静态 ELF 跑在 qemu-user。
 * 你只需填两处：claim 的原子原语 amoswap_w（①）与 IPI 的 MSIP 写 send_ipi（②）；其余 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>

#define N_HART  4
#define IRQ_ID  7u
#define WINNER  0
#define PAYLOAD 0x0000ABCDu

/* ── RISC-V 原子/访存原语（仲裁的硬件本相）── */
static inline uint32_t amoswap_w(volatile uint32_t *p, uint32_t v) {
    /* TODO①: 用 amoswap.w 原子地把 *p 换成 v 并返回旧值（这是“多核仲裁只一个赢”的硬件本相）。
     *   HINT: uint32_t old;
     *         __asm__ volatile("amoswap.w %0, %2, (%1)" : "=&r"(old) : "r"(p), "r"(v) : "memory");
     *         return old; */
    (void)p; (void)v;
    return 0; /* ← 占位：恒返回 0 → 每个 hart 都以为自己把 gateway 从 0 翻到 1 → 仲裁崩溃 */
}
static inline void mmio_w(volatile uint32_t *p, uint32_t v) {
    __asm__ volatile("sw %0, 0(%1)" : : "r"(v), "r"(p) : "memory");
}
static inline uint32_t mmio_r(volatile uint32_t *p) {
    uint32_t v;
    __asm__ volatile("lw %0, 0(%1)" : "=r"(v) : "r"(p) : "memory");
    return v;
}
static inline void barrier(void) { __asm__ volatile("fence rw,rw" ::: "memory"); }

/* ── PLIC gateway + per-hart context ── */
static volatile uint32_t plic_inflight; /* gateway：0 空闲 / 1 已 claim 未 complete */
static int plic_pending;                 /* 设备在拉 IRQ */
static int enabled[N_HART];              /* 每 hart-context 对该 IRQ 的使能（prio>thresh）*/

/* claim = 原子拿 gateway：只有把 inflight 从 0 翻到 1 的那个 hart 拿到 IRQ_ID，其余读 0。 */
static uint32_t plic_claim(int hart) {
    if (!enabled[hart] || !plic_pending) return 0;
    uint32_t old = amoswap_w(&plic_inflight, 1u);
    return (old == 0u) ? IRQ_ID : 0u;
}
/* complete = 还 gateway：写回 IRQ_ID，inflight 归 0，源可再次投递。 */
static void plic_complete(int hart, uint32_t id) {
    (void)hart;
    if (id == IRQ_ID) { barrier(); plic_inflight = 0u; }
}

/* ── CLINT 软件中断/IPI ── */
static volatile uint32_t msip[N_HART];
static volatile uint32_t shared_payload;
static uint32_t seen[N_HART];

/* send_ipi = 写目标 hart 的 MSIP（sw），点亮其软件中断。 */
static void send_ipi(int target) {
    /* TODO②: 用 mmio_w 把目标 hart 的 MSIP 置 1（点亮软件中断；CLINT 写 MSIP=敲 IPI）。
     *   HINT: mmio_w(&msip[target], 1u); */
    (void)target; /* ← 占位：没敲 IPI → 从核永远见不到软件中断 */
}

static int phase_arbiter(void) {
    int i, winners = 0, who = -1;
    plic_pending = 1;
    plic_inflight = 0;
    for (i = 0; i < N_HART; i++) enabled[i] = 1;

    uint32_t r0 = plic_claim(WINNER);
    if (r0 != IRQ_ID) return 0;
    printf("CLAIM_PASS  hart%d amoswap.w 抢到 gateway，claim=IRQ%u\n", WINNER, IRQ_ID);

    if (r0 == IRQ_ID) { winners = 1; who = WINNER; }
    for (i = 0; i < N_HART; i++) {
        if (i == WINNER) continue;
        if (plic_claim(i) == IRQ_ID) { winners++; who = i; }
    }
    if (winners != 1 || who != WINNER) return 0;
    printf("ARBITER_PASS 同一 IRQ%u 仅 hart%d 处理：%d 个竞争者 claim 到 0\n",
           IRQ_ID, WINNER, N_HART - 1);
    return 1;
}

static int phase_complete(void) {
    /* 漏 complete：gateway 仍 inflight，再 claim 读 0（中断丢失 / 设备卡住）。 */
    if (plic_claim(WINNER) != 0) return 0;
    plic_complete(WINNER, IRQ_ID);
    /* complete 后 gateway 重新武装，再 claim 又能拿到 IRQ_ID。 */
    if (plic_claim(WINNER) != IRQ_ID) return 0;
    plic_complete(WINNER, IRQ_ID);
    printf("COMPLETE_PASS gateway complete 后重新武装（漏 complete 则源永久卡住）\n");
    return 1;
}

static int phase_ipi(void) {
    int t, woken = 0;
    for (t = 0; t < N_HART; t++) { msip[t] = 0; seen[t] = 0; }
    shared_payload = 0;

    /* 生产者 hart0：先写共享数据，barrier，再敲 IPI（写 MSIP）。 */
    mmio_w(&shared_payload, PAYLOAD);
    barrier();
    for (t = 0; t < N_HART; t++)
        if (t != WINNER) send_ipi(t);

    /* 消费者各 hart：见 MSIP→barrier→读到 barrier 之前写入的 payload→清自己的 MSIP。 */
    for (t = 0; t < N_HART; t++) {
        if (t == WINNER) continue;
        if (mmio_r(&msip[t]) == 1u) {
            barrier();
            seen[t] = mmio_r(&shared_payload);
            mmio_w(&msip[t], 0u);
            woken++;
        }
    }
    if (woken != N_HART - 1 || msip[WINNER] != 0) return 0;
    for (t = 0; t < N_HART; t++) {
        if (t == WINNER) continue;
        if (seen[t] != PAYLOAD || msip[t] != 0) return 0;
    }
    printf("IPI_PASS    hart%d 经 barrier 敲 %d 路 IPI，从核见 MSIP 读到 payload=%04X\n",
           WINNER, N_HART - 1, PAYLOAD);
    return 1;
}

int main(void) {
    int ok = 1;
    ok &= phase_arbiter();
    ok &= phase_complete();
    ok &= phase_ipi();
    if (ok) {
        printf("ALL_PASS\n");
        return 0;
    }
    printf("SOME_FAIL\n");
    return 1;
}
