/* 内存管理：分层、Swap 与统一地址空间 —— C 参考解。
 *
 * 两块"设备" = 两个软件数组：
 *   FAST —— 小、快（每次访问代价 +1）、断电即失（reboot 清零）。
 *   SLOW —— 大、慢（每次访问代价 +10）、断电不丢（reboot 保留）。
 *
 * 同样两块设备，按三种"想要"拼出三种内存系统（见 README §0）：
 *   场景一（要速度）/ 场景二（建 Swap，缺页换入换出+脏页回写）/ 场景三（平坦大内存）。
 *
 * 你只需填 5 处「核心逻辑」函数体；下方测试 harness（校验 + *_PASS 打印）勿改。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define FAST_COST 1u  /* 快设备每次块访问代价 */
#define SLOW_COST 10u /* 慢设备每次块访问代价（分层成立） */
#define MAXBLK 64

/* 一块 RAM 块设备：data 是块数组（1 块 = 1 个 u64），cost 是累计访问代价。 */
typedef struct {
    uint64_t data[MAXBLK];
    int blocks;
    uint64_t per_access;
    uint64_t cost;
} Dev;

static void dev_init(Dev *d, int blocks, uint64_t per_access) {
    memset(d, 0, sizeof(*d));
    d->blocks = blocks;
    d->per_access = per_access;
}
/* 断电：仅 FAST 调用（易失），SLOW 不调用（持久）。 */
static void dev_power_cycle(Dev *d) {
    memset(d->data, 0, sizeof(d->data));
}

/* ════════════════════════════════════════════════════════════════════
 *  填空区 ①：设备抽象（10.1）——统一块读写接口
 * ════════════════════════════════════════════════════════════════════ */

/* 读一块：累加访问代价，返回该块内容。 */
static uint64_t blk_read(Dev *dev, int blk) {
    dev->cost += dev->per_access;
    return dev->data[blk];
}

/* 写一块：累加访问代价，写入该块。 */
static void blk_write(Dev *dev, int blk, uint64_t val) {
    dev->cost += dev->per_access;
    dev->data[blk] = val;
}

/* ════════════════════════════════════════════════════════════════════
 *  填空区 ②：场景一（10.2）——小内存 + 大存储（要速度）
 * ════════════════════════════════════════════════════════════════════ */

/* 搬入：把 SLOW 上 [0,n) 拷到 FAST 的 [0,n)。 */
static void stage_in(Dev *fast, Dev *slow, int n) {
    for (int i = 0; i < n; i++)
        blk_write(fast, i, blk_read(slow, i));
}

/* 搬出：把 FAST 上 [0,n) 的结果写回 SLOW 的 [0,n)（持久化）。 */
static void stage_out(Dev *fast, Dev *slow, int n) {
    for (int i = 0; i < n; i++)
        blk_write(slow, i, blk_read(fast, i));
}

/* ════════════════════════════════════════════════════════════════════
 *  填空区 ③④：场景二（10.3 + 10.4）——建 Swap
 * ════════════════════════════════════════════════════════════════════ */

#define MAXPAGE 16
#define MAXFRAME 16

typedef struct { int present, frame, dirty; } Pte;

/* 单级页表 + 软件 MMU 垫片：FAST 当物理帧，SLOW 当 swap（槽号 = vpn）。 */
typedef struct {
    Pte ptes[MAXPAGE];
    int owner[MAXFRAME];    /* frame -> vpn（-1 = 空） */
    int fifo[MAXFRAME];     /* 已占用帧的换出顺序（FIFO 环形队列） */
    int fifo_head, fifo_len;
    int free_top;           /* 下一个未用过的空闲帧号；< num_frames 时仍有空帧 */
    int num_frames, num_pages;
    Dev fast;               /* 物理帧：frame i = fast.data[i] */
    Dev slow;               /* swap：slot vpn = slow.data[vpn] */
} Pager;

static void pager_init(Pager *p, int num_frames, int num_pages) {
    memset(p, 0, sizeof(*p));
    p->num_frames = num_frames;
    p->num_pages = num_pages;
    p->free_top = 0;
    p->fifo_head = 0;
    p->fifo_len = 0;
    for (int i = 0; i < MAXFRAME; i++) p->owner[i] = -1;
    dev_init(&p->fast, num_frames, FAST_COST);
    dev_init(&p->slow, num_pages, SLOW_COST);
}

/* 缺页路径核心：返回 vpn 当前驻留的帧号。
 *   命中：直接返回帧号。
 *   缺页：若有空闲帧（free_top < num_frames）直接用；否则按 FIFO 选 victim，
 *         victim 脏则回写其 swap 槽，腾出帧；再从槽 vpn 把页读入帧，置 present/frame、清 dirty。 */
static int translate(Pager *p, int vpn) {
    if (p->ptes[vpn].present)
        return p->ptes[vpn].frame;

    int frame;
    if (p->free_top < p->num_frames) {
        frame = p->free_top++;
    } else {
        /* TODO[a] FIFO：取队首最早进入的帧作为 victim。
         * ELSE[b] Clock：可改用 ref 位 + 指针的二次机会（此处用 FIFO）。 */
        frame = p->fifo[p->fifo_head];
        p->fifo_head = (p->fifo_head + 1) % p->num_frames;
        p->fifo_len--;
        int vvpn = p->owner[frame];
        if (p->ptes[vvpn].dirty)
            blk_write(&p->slow, vvpn, blk_read(&p->fast, frame)); /* 脏页回写 */
        p->ptes[vvpn].present = 0;
        p->owner[frame] = -1;
    }
    /* 换入目标页 */
    blk_write(&p->fast, frame, blk_read(&p->slow, vpn));
    p->ptes[vpn].present = 1;
    p->ptes[vpn].frame = frame;
    p->ptes[vpn].dirty = 0;
    p->owner[frame] = vpn;
    p->fifo[(p->fifo_head + p->fifo_len) % p->num_frames] = frame;
    p->fifo_len++;
    return frame;
}

/* 同步：退出前把所有"驻留且脏"的页刷回 SLOW，保证持久。 */
static void sync_all(Pager *p) {
    for (int vpn = 0; vpn < p->num_pages; vpn++) {
        if (p->ptes[vpn].present && p->ptes[vpn].dirty) {
            blk_write(&p->slow, vpn, blk_read(&p->fast, p->ptes[vpn].frame));
            p->ptes[vpn].dirty = 0;
        }
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  填空区 ⑤：场景三（10.5）——两块都当内存：平坦大内存
 * ════════════════════════════════════════════════════════════════════ */

/* 线性地址译码：la < fast_size 落 FAST（dev=0），其余落 SLOW（dev=1）。
 * 通过出参 dev/off 返回：dev 0=FAST 1=SLOW；off 是设备内块偏移。 */
static void addr_route(int la, int fast_size, int *dev, int *off) {
    if (la < fast_size) {
        *dev = 0;
        *off = la;
    } else {
        *dev = 1;
        *off = la - fast_size;
    }
}

/* ═══════════════════════ 以下为测试 harness（勿改）═══════════════════════ */

static uint64_t pg_read(Pager *p, int vpn) {
    int f = translate(p, vpn);
    return blk_read(&p->fast, f);
}
static void pg_write(Pager *p, int vpn, uint64_t val) {
    int f = translate(p, vpn);
    blk_write(&p->fast, f, val);
    p->ptes[vpn].dirty = 1;
}

static uint64_t refv(int vpn) { return 0xA000u + (uint64_t)vpn; }
static uint64_t newv(int vpn) { return 0xB000u + (uint64_t)vpn; }

/* ── 10.1 设备探测 ── */
static int test_dev_probe(void) {
    Dev fast, slow;
    dev_init(&fast, 8, FAST_COST);
    dev_init(&slow, 64, SLOW_COST);
    int ok = 1;

    if (fast.blocks != 8 || slow.blocks != 64) {
        printf("DEV_PROBE_FAIL 容量错: fast=%d slow=%d\n", fast.blocks, slow.blocks);
        ok = 0;
    }
    blk_write(&fast, 3, 0x1234);
    blk_write(&slow, 40, 0x5678);
    if (blk_read(&fast, 3) != 0x1234 || blk_read(&slow, 40) != 0x5678) {
        printf("DEV_PROBE_FAIL 块读写不一致\n");
        ok = 0;
    }
    Dev fa, sl;
    dev_init(&fa, 8, FAST_COST);
    dev_init(&sl, 8, SLOW_COST);
    for (int i = 0; i < 8; i++) {
        blk_write(&fa, i, (uint64_t)i);
        blk_write(&sl, i, (uint64_t)i);
    }
    if (!(fa.cost < sl.cost)) {
        printf("DEV_PROBE_FAIL 分层不成立 cost(FAST)=%llu cost(SLOW)=%llu\n",
               (unsigned long long)fa.cost, (unsigned long long)sl.cost);
        ok = 0;
    }
    if (ok) printf("DEV_PROBE_PASS\n");
    return ok;
}

/* ── 10.2 场景一 ── */
static int test_scenario_a(void) {
    const int N = 6;
    Dev fast, slow;
    dev_init(&fast, 8, FAST_COST);
    dev_init(&slow, 64, SLOW_COST);
    for (int i = 0; i < N; i++) slow.data[i] = (uint64_t)i + 1;
    int ok = 1;

    stage_in(&fast, &slow, N);
    uint64_t slow_cost_before = slow.cost;
    for (int i = 0; i < N; i++) {
        uint64_t v = blk_read(&fast, i);
        blk_write(&fast, i, v * v);
    }
    if (slow.cost != slow_cost_before) {
        printf("SCENARIO_A_FAIL 计算阶段触碰了慢设备\n");
        ok = 0;
    }
    stage_out(&fast, &slow, N);

    dev_power_cycle(&fast); /* "重启"：快设备断电清零；慢设备保留 */

    for (int i = 0; i < N; i++) {
        uint64_t want = ((uint64_t)i + 1) * ((uint64_t)i + 1);
        uint64_t got = blk_read(&slow, i);
        if (got != want) {
            printf("SCENARIO_A_FAIL slot%d got=%llu want=%llu\n",
                   i, (unsigned long long)got, (unsigned long long)want);
            ok = 0;
        }
    }
    if (ok) printf("SCENARIO_A_PASS\n");
    return ok;
}

/* ── 10.3 换入，帧充足 ── */
static int test_pagein(void) {
    Pager p;
    pager_init(&p, 4, 8);
    for (int vpn = 0; vpn < 8; vpn++) p.slow.data[vpn] = refv(vpn);
    int seq[] = {0, 1, 2, 3, 0, 2, 1, 3};
    int ok = 1;
    for (int k = 0; k < 8; k++) {
        int vpn = seq[k];
        uint64_t got = pg_read(&p, vpn);
        if (got != refv(vpn)) {
            printf("PAGEIN_FAIL vpn=%d got=0x%llx want=0x%llx\n",
                   vpn, (unsigned long long)got, (unsigned long long)refv(vpn));
            ok = 0;
        }
    }
    if (ok) printf("PAGEIN_PASS\n");
    return ok;
}

/* ── 10.4 换出/回写 + 同步持久化，工作集 >> 帧数 ── */
static int test_swapout_sync(void) {
    const int FRAMES = 4, PAGES = 8;
    int ok = 1;

    Pager p;
    pager_init(&p, FRAMES, PAGES);
    for (int vpn = 0; vpn < PAGES; vpn++) p.slow.data[vpn] = refv(vpn);
    for (int vpn = 0; vpn < PAGES; vpn++) pg_write(&p, vpn, newv(vpn));
    for (int vpn = 0; vpn < PAGES; vpn++) {
        uint64_t got = pg_read(&p, vpn);
        if (got != newv(vpn)) {
            printf("SWAPOUT_FAIL vpn=%d got=0x%llx want=0x%llx\n",
                   vpn, (unsigned long long)got, (unsigned long long)newv(vpn));
            ok = 0;
        }
    }
    if (ok) printf("SWAPOUT_PASS\n");

    sync_all(&p);
    uint64_t swap_snapshot[MAXPAGE];
    memcpy(swap_snapshot, p.slow.data, sizeof(uint64_t) * PAGES);

    int ok2 = 1;
    for (int vpn = 0; vpn < PAGES; vpn++) {
        if (swap_snapshot[vpn] != newv(vpn)) {
            printf("SYNC_FAIL vpn=%d swap=0x%llx want=0x%llx\n",
                   vpn, (unsigned long long)swap_snapshot[vpn], (unsigned long long)newv(vpn));
            ok2 = 0;
        }
    }
    if (ok2) printf("SYNC_PASS\n");
    return ok && ok2;
}

/* ── 10.5 平坦大内存 ── */
static int test_unified(void) {
    const int FAST_SZ = 8, SLOW_SZ = 16;
    int total = FAST_SZ + SLOW_SZ;
    Dev fast, slow;
    dev_init(&fast, FAST_SZ, FAST_COST);
    dev_init(&slow, SLOW_SZ, SLOW_COST);
    int ok = 1;

    for (int la = 0; la < total; la++) {
        int dev, off;
        addr_route(la, FAST_SZ, &dev, &off);
        uint64_t v = (uint64_t)la * 3 + 7;
        if (dev == 0) blk_write(&fast, off, v);
        else if (dev == 1) blk_write(&slow, off, v);
        else { printf("UNIFIED_FAIL la=%d 非法 dev=%d\n", la, dev); ok = 0; }
    }
    for (int la = 0; la < total; la++) {
        int dev, off;
        addr_route(la, FAST_SZ, &dev, &off);
        uint64_t want = (uint64_t)la * 3 + 7;
        uint64_t got = (dev == 0) ? blk_read(&fast, off) : blk_read(&slow, off);
        if (got != want) {
            printf("UNIFIED_FAIL la=%d dev=%d off=%d got=%llu want=%llu\n",
                   la, dev, off, (unsigned long long)got, (unsigned long long)want);
            ok = 0;
        }
    }
    int d7, o7, d8, o8;
    addr_route(7, FAST_SZ, &d7, &o7);
    addr_route(8, FAST_SZ, &d8, &o8);
    if (!(d7 == 0 && o7 == 7 && d8 == 1 && o8 == 0)) {
        printf("UNIFIED_FAIL 边界译码错: la7->(%d,%d) la8->(%d,%d)\n", d7, o7, d8, o8);
        ok = 0;
    }
    if (ok) printf("UNIFIED_PASS\n");
    return ok;
}

int main(void) {
    int all = 1;
    all &= test_dev_probe();
    all &= test_scenario_a();
    all &= test_pagein();
    all &= test_swapout_sync();
    all &= test_unified();

    if (all) { printf("ALL_PASS\n"); return 0; }
    return 1;
}
