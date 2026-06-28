/* 正经·S16 · AMP（大小核）非对称多处理：拓扑识别 + 角色分派 + 任务亲和/迁移 + 负载统计。
 *
 * 承接 S13（多核启动）与 S05（调度器）。单镜像、按 hartid 分支：
 *   - hart0 = 大核（BIG）：跑调度器，做角色分派、亲和调度、负载均衡（迁移）。
 *   - 另一些 hart = 小核（LITTLE）：跑后台任务。
 *
 * 判据：
 *   ROLE_PASS      角色分派表合法（hart0=大核跑调度；至少 1 大核 + 1 小核；角色齐全）。
 *   AFFINITY_PASS  每个任务落在满足其亲和的核上；负载均衡迁移把各类核的峰值负载降下来；
 *                  迁移前后保持亲和、负载守恒。
 *   ALL_PASS       全部通过。
 *
 * 另有一段“真·多核” live 演示：hart0 用 SBI HSM 唤醒一颗从核（小核），
 * 在直接映射区（物理地址，satp=0 恒等映射）经共享变量 + fence 回收其后台任务结果。
 */
#include "kernel.h"

/* ------------------------------------------------------------------ 拓扑与角色 */
#define NCORE 4
enum Role { ROLE_NONE = 0, ROLE_BIG = 1, ROLE_LITTLE = 2 };

/*
 * 角色分派表：把每颗 hart 映射到大核/小核。
 * 约定：hart0 要跑调度器，必须是大核；整机至少 1 大核 + 1 小核。
 * 本机拓扑（big.LITTLE 2+2）：core0/1 = 大核，core2/3 = 小核。
 */
static int cpu_role[NCORE] = { ROLE_BIG, ROLE_BIG, ROLE_LITTLE, ROLE_LITTLE };

/* ------------------------------------------------------------------ 任务表 */
enum Aff { AFF_BIG = 1, AFF_LITTLE = 2 };
struct Task {
    const char *name;
    int aff;     /* 亲和：AFF_BIG 必须上大核；AFF_LITTLE 必须上小核 */
    int weight;  /* 负载权重 */
    int core;    /* 当前所在核（-1=未分派） */
};

#define NTASK 8
static struct Task tasks[NTASK] = {
    { "render",    AFF_BIG,    40, -1 },   /* 重算力：只能上大核 */
    { "physics",   AFF_BIG,    32, -1 },
    { "decode",    AFF_BIG,    20, -1 },
    { "ai",        AFF_BIG,    16, -1 },
    { "sensors",   AFF_LITTLE,  6, -1 },   /* 后台：上小核 */
    { "logd",      AFF_LITTLE,  4, -1 },
    { "telemetry", AFF_LITTLE,  3, -1 },
    { "watchdog",  AFF_LITTLE,  2, -1 },
};

static int load[NCORE];          /* 各核负载统计 */
static int nmigrate;             /* 迁移次数 */

/* ------------------------------------------------------------------ 跨核共享（直接映射区，volatile + fence） */
static volatile uint64_t g_sec_done;     /* 从核完成标志 */
static volatile uint64_t g_sec_hartid;   /* 实际被唤醒的从核 id */
static volatile uint64_t g_sec_checksum; /* 从核后台任务结果 */
#define SEC_EXPECT 500500ULL             /* 1+2+...+1000 */

extern void hart_entry(void);            /* boot.S：从核入口 */

/* SBI HSM 扩展：hart_start(EID 0x48534D, fid 0, a0=hartid, a1=start_addr, a2=opaque) */
static long sbi_hart_start(unsigned long hartid, unsigned long addr, unsigned long opaque) {
    return sbi_call(0x48534D, 0, (long)hartid, (long)addr, (long)opaque);
}

/* 从核（小核）主体：跑一个后台任务（累加），把结果写进直接映射区共享变量。
 * 注意：从核不碰 SBI 控制台（避免与 hart0 输出交错），只经共享内存 + fence 汇报。 */
void secondary_main(unsigned long hartid) {
    uint64_t s = 0;
    for (int i = 1; i <= 1000; i++) s += (uint64_t)i;
    g_sec_hartid   = hartid;
    g_sec_checksum = s;
    __sync_synchronize();          /* fence：先让数据落地，再置完成标志 */
    g_sec_done = 1;
    for (;;) asm volatile("wfi");
}

/* ------------------------------------------------------------------ 小工具 */
static const char *role_name(int r) {
    return r == ROLE_BIG ? "BIG" : (r == ROLE_LITTLE ? "LITTLE" : "NONE");
}
static int aff_ok(int role, int aff) {
    if (aff == AFF_BIG)    return role == ROLE_BIG;
    if (aff == AFF_LITTLE) return role == ROLE_LITTLE;
    return 0;
}
static void kv(const char *k, uint64_t v) { kputs(k); kputdec(v); }

/* ------------------------------------------------------------------ 1) 角色分派校验 */
static int check_roles(void) {
    int nbig = 0, nlittle = 0, ok = 1;
    kputs("[topo] ");
    for (int c = 0; c < NCORE; c++) {
        kputs("core"); kputdec(c); kputs("="); kputs(role_name(cpu_role[c]));
        kputs(c == NCORE - 1 ? "\n" : " ");
        if (cpu_role[c] == ROLE_BIG) nbig++;
        else if (cpu_role[c] == ROLE_LITTLE) nlittle++;
        else ok = 0;
    }
    if (cpu_role[0] != ROLE_BIG) { kputs("[warn] scheduler core (hart0) is not BIG\n"); ok = 0; }
    if (nbig < 1)    { kputs("[warn] no BIG core in topology\n");    ok = 0; }
    if (nlittle < 1) { kputs("[warn] no LITTLE core in topology\n"); ok = 0; }
    kv("[roles] big=", (uint64_t)nbig); kv(" little=", (uint64_t)nlittle); kputs("\n");
    return ok;
}

/* ------------------------------------------------------------------ 2) 亲和调度 + 迁移 */
/* 阶段 A：朴素分派——每个任务放进“第一个满足亲和的核”。亲和正确，但负载倾斜。 */
static void dispatch_naive(void) {
    for (int c = 0; c < NCORE; c++) load[c] = 0;
    for (int t = 0; t < NTASK; t++) {
        int chosen = -1;
        for (int c = 0; c < NCORE; c++) {
            if (aff_ok(cpu_role[c], tasks[t].aff)) { chosen = c; break; }
        }
        tasks[t].core = chosen;
        if (chosen >= 0) load[chosen] += tasks[t].weight;
        else { kputs("[warn] task '"); kputs(tasks[t].name);
               kputs("' has no core matching its affinity\n"); }
    }
}

/* 类内峰值负载（只看属于该角色的核） */
static int class_max(int role) {
    int m = 0;
    for (int c = 0; c < NCORE; c++)
        if (cpu_role[c] == role && load[c] > m) m = load[c];
    return m;
}
static int class_sum(int role) {
    int s = 0;
    for (int c = 0; c < NCORE; c++)
        if (cpu_role[c] == role) s += load[c];
    return s;
}

/* 阶段 B：负载均衡迁移——在同角色核之间迁移任务以削峰。每次把“最忙核”上能降低瓶颈的
 * 一个任务迁到同角色的较闲核，直到无法再降。迁移全程不破坏亲和。 */
static void rebalance(int role) {
    for (int iter = 0; iter < 64; iter++) {
        /* 最忙的本角色核 */
        int hi = -1;
        for (int c = 0; c < NCORE; c++)
            if (cpu_role[c] == role && (hi < 0 || load[c] > load[hi])) hi = c;
        if (hi < 0) return;

        int best_t = -1, best_to = -1, best_max = load[hi];
        for (int t = 0; t < NTASK; t++) {
            if (tasks[t].core != hi) continue;
            for (int to = 0; to < NCORE; to++) {
                if (to == hi || cpu_role[to] != role) continue;
                if (!aff_ok(cpu_role[to], tasks[t].aff)) continue;
                int a = load[hi] - tasks[t].weight;
                int b = load[to] + tasks[t].weight;
                int nm = a > b ? a : b;              /* 迁移后这两核的峰值 */
                if (nm < best_max) { best_max = nm; best_t = t; best_to = to; }
            }
        }
        if (best_t < 0) return;                       /* 无法再削峰 */

        load[hi]            -= tasks[best_t].weight;
        load[best_to]       += tasks[best_t].weight;
        int from            = tasks[best_t].core;
        tasks[best_t].core  = best_to;
        nmigrate++;
        kputs("[migrate] '"); kputs(tasks[best_t].name);
        kputs("' "); kputs(role_name(role));
        kputs(" core"); kputdec(from); kputs("->core"); kputdec(best_to);
        kv(" (peak ", (uint64_t)best_max); kputs(")\n");
    }
}

static void print_loads(const char *tag) {
    kputs(tag);
    for (int c = 0; c < NCORE; c++) {
        kputs(" core"); kputdec(c); kputs("("); kputs(role_name(cpu_role[c]));
        kputs(")="); kputdec((uint64_t)load[c]);
    }
    kputs("\n");
}

static int check_affinity(void) {
    int ok = 1;

    dispatch_naive();
    int big0 = class_max(ROLE_BIG), lit0 = class_max(ROLE_LITTLE);
    int bigsum0 = class_sum(ROLE_BIG), litsum0 = class_sum(ROLE_LITTLE);
    print_loads("[load:naive ]");

    rebalance(ROLE_BIG);
    rebalance(ROLE_LITTLE);
    int big1 = class_max(ROLE_BIG), lit1 = class_max(ROLE_LITTLE);
    int bigsum1 = class_sum(ROLE_BIG), litsum1 = class_sum(ROLE_LITTLE);
    print_loads("[load:balanced]");
    kv("[migrate] total moves = ", (uint64_t)nmigrate); kputs("\n");

    /* (a) 每个任务都落在满足其亲和的核上 */
    for (int t = 0; t < NTASK; t++) {
        if (tasks[t].core < 0 || !aff_ok(cpu_role[tasks[t].core], tasks[t].aff)) {
            kputs("[warn] affinity not satisfied for '"); kputs(tasks[t].name); kputs("'\n");
            ok = 0;
        }
    }
    /* (b) 迁移确实削了两类核的峰值负载 */
    if (!(big1 < big0)) { kputs("[warn] BIG peak load not reduced by migration\n");    ok = 0; }
    if (!(lit1 < lit0)) { kputs("[warn] LITTLE peak load not reduced by migration\n"); ok = 0; }
    /* (c) 迁移不丢任务：各类核负载总量守恒 */
    if (bigsum1 != bigsum0 || litsum1 != litsum0) {
        kputs("[warn] load not conserved across migration\n"); ok = 0;
    }
    kv("[peak] BIG ", (uint64_t)big0); kv("->", (uint64_t)big1);
    kv("  LITTLE ", (uint64_t)lit0); kv("->", (uint64_t)lit1); kputs("\n");
    return ok;
}

/* ------------------------------------------------------------------ 3) 真·多核 live 演示 */
/* hart0（大核）把一个后台任务派给一颗小核（真 hart），经直接映射共享区回收结果。
 * 这是“按角色分派任务”的硬件级证据；非 PASS 门槛，但应当真正跑起来。 */
static void live_secondary_demo(void) {
    int started = -1;
    for (unsigned long h = 1; h < NCORE; h++) {       /* 试 hart 1..N-1，启第一颗 STOPPED 态从核 */
        long rc = sbi_hart_start(h, (unsigned long)(uintptr_t)hart_entry, 0);
        if (rc == 0) { started = (int)h; break; }
    }
    if (started < 0) { kputs("AMP_LIVE: no secondary hart available (model-only)\n"); return; }

    /* 有界等待从核汇报（直接映射区共享变量 + fence） */
    for (volatile uint64_t i = 0; i < 200000000ULL && !g_sec_done; i++) __sync_synchronize();
    __sync_synchronize();
    if (g_sec_done && g_sec_checksum == SEC_EXPECT) {
        kputs("AMP_LIVE: dispatched bg task to LITTLE core hart");
        kputdec(g_sec_hartid);
        kv(", checksum=", g_sec_checksum); kputs(" (real multi-hart)\n");
    } else {
        kputs("AMP_LIVE: secondary did not report in time (model-only)\n");
    }
}

/* ------------------------------------------------------------------ kmain：大核调度器主线 */
void kmain(void) {
    kputs("=== S16 AMP big.LITTLE (asymmetric multiprocessing) ===\n");

    int role_ok = check_roles();
    if (role_ok) kputs("ROLE_PASS\n");
    else         kputs("[diag] role dispatch table incomplete\n");

    int aff_ok_all = check_affinity();
    if (aff_ok_all) kputs("AFFINITY_PASS\n");
    else            kputs("[diag] affinity/migration checks not satisfied\n");

    live_secondary_demo();

    if (role_ok && aff_ok_all) kputs("ALL_PASS\n");
    else                       kputs("[diag] not all checks passed\n");
    /* kmain 返回 → entry.S 调 k_shutdown，qemu 退出 */
}
