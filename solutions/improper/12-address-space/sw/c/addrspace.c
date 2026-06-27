/* 12 地址空间：软件 MMU 与「偷梁换柱」的稀疏映射 —— C 参考解。
 *
 * 一句话母题：用户程序眼里内存「无限大」，物理机却只有几帧。
 * 你写一层「软件 MMU」当中间人——只把真正用到的 vpn 偷偷接到几块真 ppn 上。
 *
 * 五段逐题递进（学生只填带「学生填」标注的纯函数；下方 harness 勿改）：
 *   E1 稀疏映射 · E2 直接映射区「拍卖行」· E3 多槽 SMP · E4 两级映射 · E5 SV39 草图
 */
#include <stdio.h>
#include <stdint.h>

#define PAGE_WORDS 512u
#define NFRAMES    8u

#define DIRECT_BASE 0x80000000ull
#define DIRECT_SIZE 0x1000ull
#define AUCTION_VA  DIRECT_BASE
#define PRIV_VA     0x100000ull

#define SMP_BASE    0x90000000ull
#define SLOT        8ull
#define NHARTS      4u

#define PTE_V 1ull
#define PTE_R 2ull
#define PTE_W 4ull
#define PTE_X 8ull
#define PTE_U 16ull

/* ═════════════════════ E1 · 软件 MMU 稀疏映射 ═════════════════════ */
typedef struct {
    uint64_t vpn[64], ppn[64];
    int n;
} Pt1;

/* ── 学生填（E1）── */
/* 记一条 vpn→ppn 映射。 */
static void map1(Pt1 *pt, uint64_t vpn, uint64_t ppn) {
    pt->vpn[pt->n] = vpn;
    pt->ppn[pt->n] = ppn;
    pt->n++;
}
/* 翻译：va = vpn(高位)|off(低 12 位)；命中返回 1 且 *pa=ppn<<12|off，否则返回 0。 */
static int translate(const Pt1 *pt, uint64_t va, uint64_t *pa) {
    uint64_t vpn = va >> 12, off = va & 0xfff;
    for (int i = 0; i < pt->n; i++)
        if (pt->vpn[i] == vpn) { *pa = (pt->ppn[i] << 12) | off; return 1; }
    return 0;
}

/* ── E1 harness（勿改）── */
typedef struct {
    uint64_t mem[NFRAMES * PAGE_WORDS];
    uint64_t next;
    Pt1 pt;
} World1;

static int w1_alloc(World1 *w, uint64_t *ppn) {
    if (w->next < NFRAMES) { *ppn = w->next++; return 1; }
    return 0;
}
static void w1_pw(World1 *w, uint64_t pa, uint64_t val) {
    uint64_t i = (pa >> 12) * PAGE_WORDS + ((pa & 0xfff) >> 3);
    if (i < NFRAMES * PAGE_WORDS) w->mem[i] = val;
}
static uint64_t w1_pr(const World1 *w, uint64_t pa) {
    uint64_t i = (pa >> 12) * PAGE_WORDS + ((pa & 0xfff) >> 3);
    return i < NFRAMES * PAGE_WORDS ? w->mem[i] : 0;
}
static int store1(World1 *w, uint64_t va, uint64_t val) {
    uint64_t pa;
    if (!translate(&w->pt, va, &pa)) {
        uint64_t ppn;
        if (!w1_alloc(w, &ppn)) return 0;
        map1(&w->pt, va >> 12, ppn);
        if (!translate(&w->pt, va, &pa)) return 0;
    }
    w1_pw(w, pa, val);
    return 1;
}
static int load1(const World1 *w, uint64_t va, uint64_t *out) {
    uint64_t pa;
    if (!translate(&w->pt, va, &pa)) return 0;
    *out = w1_pr(w, pa);
    return 1;
}

static int stage_e1(void) {
    static World1 w;
    w.next = 0; w.pt.n = 0;
    uint64_t addrs[3] = {0x0ull, 0x100000ull, 0x100000000000ull};
    uint64_t magic[3] = {0xA1ull, 0xB2ull, 0xC3ull};
    int ok = 1;
    for (int i = 0; i < 3; i++)
        if (!store1(&w, addrs[i], magic[i])) { printf("SPARSE_FAIL store va=%#llx 失败\n", (unsigned long long)addrs[i]); ok = 0; }
    for (int i = 0; i < 3; i++) {
        uint64_t v;
        if (!load1(&w, addrs[i], &v) || v != magic[i]) {
            printf("SPARSE_FAIL va=%#llx 读回错\n", (unsigned long long)addrs[i]); ok = 0;
        }
    }
    if (w.next > 3) { printf("SPARSE_FAIL 实占帧数=%llu 超过 3\n", (unsigned long long)w.next); ok = 0; }
    if (ok) printf("SPARSE_PASS\n");
    return ok;
}

/* ═══════════════ E2 · 直接映射区「拍卖行」交换 ═══════════════ */
typedef struct {
    uint64_t mem[NFRAMES * PAGE_WORDS];
    uint64_t next;
    uint64_t direct[PAGE_WORDS];
} World2;

static int in_direct(uint64_t va) { return va >= DIRECT_BASE && va < DIRECT_BASE + DIRECT_SIZE; }

static int w2_alloc(World2 *w, uint64_t *ppn) {
    if (w->next < NFRAMES) { *ppn = w->next++; return 1; }
    return 0;
}

/* ── 学生填（E2）── */
/* 路由一个虚拟地址到物理地址。
 * 落在直接映射窗口 → identity（pa==va，跨空间/跨核共识的「拍卖行」）；
 * 否则走各空间私有页表翻译，缺页按需分配。 */
static uint64_t route(World2 *w, Pt1 *pt, uint64_t va) {
    /* TODO[a]: 直接映射窗口走 identity */
    if (in_direct(va)) return va;
    /* ELSE[b]: 虚拟段走翻译（缺页 demand alloc） */
    uint64_t pa;
    if (translate(pt, va, &pa)) return pa;
    uint64_t ppn;
    if (w2_alloc(w, &ppn)) {
        map1(pt, va >> 12, ppn);
        if (translate(pt, va, &pa)) return pa;
    }
    return ~0ull;
}

/* ── E2 harness（勿改）── */
static void phys_w2(World2 *w, uint64_t pa, uint64_t val) {
    if (in_direct(pa)) {
        uint64_t i = (pa - DIRECT_BASE) >> 3;
        if (i < PAGE_WORDS) w->direct[i] = val;
    } else {
        uint64_t i = (pa >> 12) * PAGE_WORDS + ((pa & 0xfff) >> 3);
        if (i < NFRAMES * PAGE_WORDS) w->mem[i] = val;
    }
}
static uint64_t phys_r2(const World2 *w, uint64_t pa) {
    if (in_direct(pa)) {
        uint64_t i = (pa - DIRECT_BASE) >> 3;
        return i < PAGE_WORDS ? w->direct[i] : 0;
    }
    uint64_t i = (pa >> 12) * PAGE_WORDS + ((pa & 0xfff) >> 3);
    return i < NFRAMES * PAGE_WORDS ? w->mem[i] : 0;
}

static int stage_e2(void) {
    static World2 w;
    static Pt1 a, b;
    w.next = 0; a.n = 0; b.n = 0;
    int ok = 1;

    uint64_t bid = 0xB1D5ull;
    uint64_t pa = route(&w, &a, AUCTION_VA);
    phys_w2(&w, pa, bid);
    pa = route(&w, &b, AUCTION_VA);
    uint64_t got = phys_r2(&w, pa);
    if (got != bid) { printf("DIRECT_FAIL B 在拍卖行读到 %#llx 应为 %#llx\n", (unsigned long long)got, (unsigned long long)bid); ok = 0; }
    else printf("DIRECT_PASS\n");

    uint64_t secret = 0x5ECull;
    pa = route(&w, &a, PRIV_VA);
    phys_w2(&w, pa, secret);
    pa = route(&w, &b, PRIV_VA);
    uint64_t leak = phys_r2(&w, pa);
    if (leak == secret) { printf("EXCHANGE_FAIL 私有区串扰：B 读到了 A 的秘密 %#llx\n", (unsigned long long)leak); ok = 0; }
    else printf("EXCHANGE_PASS\n");
    return ok;
}

/* ═══════════════════ E3 · 多槽位 SMP 拍卖 ═══════════════════ */
/* ── 学生填（E3）── */
/* 第 hart 号核的私有槽位地址 = 基址 + hart*SLOT。 */
static uint64_t slot_addr(unsigned hart) {
    return SMP_BASE + (uint64_t)hart * SLOT;
}
/* 屏障后由 0 号核归约：把各核私有槽位求和（无锁，因互不写同一槽）。 */
static uint64_t reduce_slots(const uint64_t *slots, unsigned n) {
    uint64_t s = 0;
    for (unsigned i = 0; i < n; i++) s += slots[i];
    return s;
}

/* ── E3 harness（勿改）── */
static int stage_e3(void) {
    uint64_t slots[NHARTS] = {0};
    int ok = 1;
    for (unsigned h = 0; h < NHARTS; h++) {
        uint64_t va = slot_addr(h);
        uint64_t d = va - SMP_BASE;
        uint64_t idx = d / SLOT;
        if (d % SLOT == 0 && idx < NHARTS) slots[idx] = (uint64_t)(h + 1) * 100;
        else { printf("SLOTS_FAIL hart%u 槽地址非法 va=%#llx\n", h, (unsigned long long)va); ok = 0; }
    }
    for (unsigned h = 0; h < NHARTS; h++)
        if (slots[h] != (uint64_t)(h + 1) * 100) { printf("SLOTS_FAIL slot%u 值错\n", h); ok = 0; }
    if (ok) printf("SLOTS_PASS\n");
    uint64_t want = 0;
    for (unsigned h = 1; h <= NHARTS; h++) want += (uint64_t)h * 100;
    uint64_t got = reduce_slots(slots, NHARTS);
    if (got != want) { printf("SMP_FAIL 归约和=%llu 应=%llu\n", (unsigned long long)got, (unsigned long long)want); ok = 0; }
    else printf("SMP_PASS\n");
    return ok;
}

/* ═══════════════════ E4 · 两级地址映射 ═══════════════════ */
/* va = l1(10) | l2(10) | off(12) */
#define MAXTAB 8
typedef struct {
    int pd[1024];              /* -1=空，否则=二级表下标 */
    uint64_t tab[MAXTAB][1024]; /* pte = ppn+1（0=无效） */
    int ntab;
    uint64_t mem[NFRAMES * PAGE_WORDS];
    uint64_t next;
} World4;

static int w4_alloc(World4 *w, uint64_t *ppn) {
    if (w->next < NFRAMES) { *ppn = w->next++; return 1; }
    return 0;
}

/* ── 学生填（E4）── */
/* 建立 va→pa 映射：按需新建二级表（稀疏：用到哪张建哪张）。 */
static void map2(World4 *w, uint64_t va, uint64_t pa) {
    unsigned l1 = (va >> 22) & 0x3ff;
    unsigned l2 = (va >> 12) & 0x3ff;
    if (w->pd[l1] < 0) {
        w->pd[l1] = w->ntab;
        for (int j = 0; j < 1024; j++) w->tab[w->ntab][j] = 0;
        w->ntab++;
    }
    int t = w->pd[l1];
    w->tab[t][l2] = (pa >> 12) + 1;
}
/* 两级 walk：pde=PD[l1] → pt=tab[pde] → pte=pt[l2] → pa=ppn<<12|off。命中返回 1。 */
static int walk2(const World4 *w, uint64_t va, uint64_t *pa) {
    unsigned l1 = (va >> 22) & 0x3ff;
    unsigned l2 = (va >> 12) & 0x3ff;
    uint64_t off = va & 0xfff;
    if (w->pd[l1] < 0) return 0;
    int t = w->pd[l1];
    uint64_t e = w->tab[t][l2];
    if (e == 0) return 0;
    *pa = ((e - 1) << 12) | off;
    return 1;
}

/* ── E4 harness（勿改）── */
static void w4_pw(World4 *w, uint64_t pa, uint64_t val) {
    uint64_t i = (pa >> 12) * PAGE_WORDS + ((pa & 0xfff) >> 3);
    if (i < NFRAMES * PAGE_WORDS) w->mem[i] = val;
}
static uint64_t w4_pr(const World4 *w, uint64_t pa) {
    uint64_t i = (pa >> 12) * PAGE_WORDS + ((pa & 0xfff) >> 3);
    return i < NFRAMES * PAGE_WORDS ? w->mem[i] : 0;
}

static int stage_e4(void) {
    static World4 w;
    for (int i = 0; i < 1024; i++) w.pd[i] = -1;
    w.ntab = 0; w.next = 0;
    int ok = 1;
    uint64_t vas[3] = {0x00001000ull, 0x00401000ull, 0x00802000ull};
    uint64_t magic[3] = {0x111ull, 0x222ull, 0x333ull};
    for (int i = 0; i < 3; i++) {
        uint64_t ppn;
        if (w4_alloc(&w, &ppn)) map2(&w, vas[i], ppn << 12);
        else { printf("WALK_FAIL 帧不足\n"); ok = 0; }
    }
    for (int i = 0; i < 3; i++) {
        uint64_t pa;
        if (walk2(&w, vas[i], &pa)) w4_pw(&w, pa, magic[i]);
        else { printf("WALK_FAIL va=%#llx 未命中\n", (unsigned long long)vas[i]); ok = 0; }
    }
    for (int i = 0; i < 3; i++) {
        uint64_t pa;
        if (walk2(&w, vas[i], &pa)) {
            if (w4_pr(&w, pa) != magic[i]) { printf("WALK_FAIL va=%#llx 读回错\n", (unsigned long long)vas[i]); ok = 0; }
        } else { printf("WALK_FAIL va=%#llx 未命中\n", (unsigned long long)vas[i]); ok = 0; }
    }
    if (ok) printf("WALK_PASS\n");
    if (w.ntab > 3) { printf("TWOLEVEL_FAIL 二级表数=%d 超过 3\n", w.ntab); ok = 0; }
    else if (ok) printf("TWOLEVEL_PASS\n");
    return ok;
}

/* ═══════════════════ E5 · SV39 三级 walk 草图 ═══════════════════ */
/* va39 = vpn2(9) | vpn1(9) | vpn0(9) | off(12) */
#define MAXSV 8
typedef struct {
    uint64_t tabs[MAXSV][512];
    int ntab;
    uint64_t mem[NFRAMES * PAGE_WORDS];
} Sv39;

static void sv39_build(Sv39 *s, uint64_t *va_out, uint64_t *magic_out) {
    for (int i = 0; i < MAXSV; i++) for (int j = 0; j < 512; j++) s->tabs[i][j] = 0;
    for (uint64_t i = 0; i < NFRAMES * PAGE_WORDS; i++) s->mem[i] = 0;
    s->ntab = 1; /* tabs[0] = 根 */
    uint64_t va = 0x123456000ull;
    unsigned vpn2 = (va >> 30) & 0x1ff;
    unsigned vpn1 = (va >> 21) & 0x1ff;
    unsigned vpn0 = (va >> 12) & 0x1ff;
    int i1 = s->ntab++;
    s->tabs[0][vpn2] = ((uint64_t)i1 << 10) | PTE_V;
    int i2 = s->ntab++;
    s->tabs[i1][vpn1] = ((uint64_t)i2 << 10) | PTE_V;
    uint64_t ppn_leaf = 3;
    s->tabs[i2][vpn0] = (ppn_leaf << 10) | PTE_V | PTE_R | PTE_W | PTE_U;
    uint64_t magic = 0x5F39ABCDull;
    s->mem[ppn_leaf * PAGE_WORDS] = magic;
    *va_out = va;
    *magic_out = magic;
}

/* ── 学生填（E5）── */
/* 三级 walk：从根表逐级取 PTE，检查 V；遇到带 R/W/X 的叶 PTE 即解析出 pa+flags。命中返回 1。 */
static int sv39_walk(const Sv39 *s, uint64_t va, uint64_t *pa, uint64_t *flags) {
    uint64_t off = va & 0xfff;
    unsigned vpn[3] = {(unsigned)((va >> 12) & 0x1ff), (unsigned)((va >> 21) & 0x1ff), (unsigned)((va >> 30) & 0x1ff)};
    int t = 0; /* 根表 */
    for (int level = 2; level >= 0; level--) {
        uint64_t pte = s->tabs[t][vpn[level]];
        if ((pte & PTE_V) == 0) return 0;
        uint64_t rwx = pte & (PTE_R | PTE_W | PTE_X);
        if (rwx != 0) {
            uint64_t ppn = pte >> 10;
            *pa = (ppn << 12) | off;
            *flags = pte & 0x1f;
            return 1;
        }
        t = (int)(pte >> 10);
    }
    return 0;
}

/* ── E5 harness（勿改）── */
static int stage_e5(void) {
    static Sv39 s;
    uint64_t va, magic;
    sv39_build(&s, &va, &magic);
    int ok = 1;
    uint64_t pa, flags;
    if (sv39_walk(&s, va, &pa, &flags)) {
        uint64_t i = (pa >> 12) * PAGE_WORDS + ((pa & 0xfff) >> 3);
        uint64_t val = i < NFRAMES * PAGE_WORDS ? s.mem[i] : 0;
        if (val != magic) { printf("SV39_FAIL 读回 %#llx 应 %#llx\n", (unsigned long long)val, (unsigned long long)magic); ok = 0; }
        if ((flags & PTE_V) == 0 || (flags & PTE_R) == 0 || (flags & PTE_W) == 0) {
            printf("SV39_FAIL 叶 PTE 标志位不全 flags=%#llx\n", (unsigned long long)flags); ok = 0;
        }
    } else { printf("SV39_FAIL walk 未命中\n"); ok = 0; }
    if (ok) printf("SV39_PASS\n");
    return ok;
}

int main(void) {
    int all = 1;
    all &= stage_e1();
    all &= stage_e2();
    all &= stage_e3();
    all &= stage_e4();
    all &= stage_e5();
    if (all) { printf("ALL_PASS\n"); return 0; }
    return 1;
}
