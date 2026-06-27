/* 共享内存（软件建模）—— C 参考解。
 * 母题：让两个映射指向同一块物理字节——一处写、另一处立刻看见。
 * 从「进程↔进程」一路推到「设备↔OS（MMIO）」：本质相同——
 *   同一份物理字节 + 一套不踩脚的协调协议。
 *
 * 五段逐题递进（学生只填标了「学生填」的函数体/两行；harness 勿改）：
 *   1) 页表别名      → ALIAS_PASS / ISOLATED_PASS
 *   2) mmap 共享/私有 → SHARED_PASS / PRIVATE_PASS
 *   3) 邮箱握手      → MAILBOX_PASS
 *   4) 共享环(结构体) → RING_PASS
 *   5) 共享环(MMIO)   → MMIO_SHM_PASS
 * 全过再打印 ALL_PASS。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define PAGE_WORDS 4u
#define MAGIC      0xCAFEF00Du
#define NVPN       16
#define NFRAMES    64

/* ───────────────────────── 1) 页表别名 ───────────────────────── */
typedef struct { int valid; int ppn; } Pte;

/* translate：已给（勿改）。返回物理字地址，-1 表示无效。 */
static int translate(const Pte *pt, int npte, unsigned va) {
    unsigned vpn = va / PAGE_WORDS, off = va % PAGE_WORDS;
    if ((int)vpn >= npte || !pt[vpn].valid) return -1;
    return pt[vpn].ppn * (int)PAGE_WORDS + (int)off;
}

/* 学生填：把虚拟页 vpn 映射到物理页 ppn（写一个有效 PTE）。 */
static void map(Pte *pt, int vpn, int ppn) {
    pt[vpn].valid = 1;
    pt[vpn].ppn = ppn;
}

static int pwrite_(uint32_t *phys, const Pte *pt, int npte, unsigned va, uint32_t val) {
    int pa = translate(pt, npte, va);
    if (pa < 0) return 0;
    phys[pa] = val;
    return 1;
}
/* 读：成功置 *out 并返回 1；无效返回 0。 */
static int pread_(const uint32_t *phys, const Pte *pt, int npte, unsigned va, uint32_t *out) {
    int pa = translate(pt, npte, va);
    if (pa < 0) return 0;
    *out = phys[pa];
    return 1;
}

static int sub_alias(void) {
    uint32_t phys[NFRAMES * PAGE_WORDS];
    Pte pt[NVPN];
    memset(phys, 0, sizeof(phys));
    memset(pt, 0, sizeof(pt));
    int ppn_shared = 7, vpn_a = 2, vpn_b = 5;

    /* 学生填：让 vpn_a、vpn_b 映射到同一物理页 ppn_shared（别名 = 两 PTE 同 PPN）。 */
    map(pt, vpn_a, ppn_shared);
    map(pt, vpn_b, ppn_shared);

    unsigned va_a = vpn_a * PAGE_WORDS + 1; /* 页内偏移相同才落到同一物理字 */
    unsigned va_b = vpn_b * PAGE_WORDS + 1;
    if (!pwrite_(phys, pt, NVPN, va_a, MAGIC)) {
        printf("ALIAS_FAIL 经 va_a 写入失败（map 没生效？）\n");
        return 0;
    }
    uint32_t got;
    if (!pread_(phys, pt, NVPN, va_b, &got) || got != MAGIC) {
        printf("ALIAS_FAIL 经 va_b 读到 0x%X，期望 0x%X\n", got, MAGIC);
        return 0;
    }
    printf("ALIAS_PASS\n");

    /* 对照组：不同 PPN → 互不可见。 */
    uint32_t phys2[NFRAMES * PAGE_WORDS];
    Pte pt2[NVPN];
    memset(phys2, 0, sizeof(phys2));
    memset(pt2, 0, sizeof(pt2));
    map(pt2, vpn_a, 1);
    map(pt2, vpn_b, 9);
    pwrite_(phys2, pt2, NVPN, va_a, MAGIC);
    uint32_t g2 = 0;
    if (pread_(phys2, pt2, NVPN, va_b, &g2) && g2 != MAGIC) {
        printf("ISOLATED_PASS\n");
        return 1;
    }
    printf("ISOLATED_FAIL 不同物理页竟互相可见\n");
    return 0;
}

/* ─────────────────── 2) mmap：共享 vs 私有 ─────────────────── */
#define SHARED  0u
#define PRIVATE 1u

typedef struct {
    uint32_t phys[NFRAMES * PAGE_WORDS];
    int next_ppn;
    uint32_t keys[NFRAMES];
    int ppns[NFRAMES];
    int nreg;
} World;

static void world_init(World *w) {
    memset(w->phys, 0, sizeof(w->phys));
    w->next_ppn = 0;
    w->nreg = 0;
}
static int frame_alloc(World *w) { return w->next_ppn++; }

/* 学生填：mmap 的两个分支。 */
static void do_mmap(World *w, Pte *pt, int vpn, uint32_t key, uint32_t flags) {
    int ppn;
    if (flags == SHARED) {
        /* TODO[a] MAP_SHARED：查命名段注册表，命中复用；未命中再 alloc 并登记。 */
        ppn = -1;
        for (int i = 0; i < w->nreg; i++)
            if (w->keys[i] == key) { ppn = w->ppns[i]; break; }
        if (ppn < 0) {
            ppn = frame_alloc(w);
            w->keys[w->nreg] = key;
            w->ppns[w->nreg] = ppn;
            w->nreg++;
        }
    } else {
        /* ELSE[b] MAP_PRIVATE：永远分配一块新的匿名（零）帧。 */
        ppn = frame_alloc(w);
    }
    map(pt, vpn, ppn);
}

static int sub_mmap(void) {
    static World w;
    world_init(&w);
    Pte pt1[NVPN], pt2[NVPN];
    memset(pt1, 0, sizeof(pt1));
    memset(pt2, 0, sizeof(pt2));
    uint32_t key = 0x55u;
    int vpn = 3;
    unsigned va = 3 * PAGE_WORDS + 2;

    do_mmap(&w, pt1, vpn, key, SHARED);
    if (!pwrite_(w.phys, pt1, NVPN, va, MAGIC)) {
        printf("SHARED_FAIL AS1 写入失败\n");
        return 0;
    }
    do_mmap(&w, pt2, vpn, key, SHARED);
    uint32_t got = 0;
    if (!pread_(w.phys, pt2, NVPN, va, &got) || got != MAGIC) {
        printf("SHARED_FAIL AS2 未读到共享值，得 0x%X\n", got);
        return 0;
    }
    printf("SHARED_PASS\n");

    int vpn_p = 4;
    unsigned va_p = 4 * PAGE_WORDS + 2;
    do_mmap(&w, pt2, vpn_p, key, PRIVATE);
    uint32_t gp = 1;
    if (pread_(w.phys, pt2, NVPN, va_p, &gp) && gp == 0) {
        printf("PRIVATE_PASS\n");
        return 1;
    }
    printf("PRIVATE_FAIL 私有页应为零，得 0x%X\n", gp);
    return 0;
}

/* ─────────────────── 3) 共享区上的握手（邮箱）─────────────────── */
#define MB_N 3

typedef struct { uint32_t data[MB_N]; int ready; } Mailbox;

/* 学生填：生产者一步——先写满 data，最后才置 ready。 */
static void producer_step(Mailbox *mb, const uint32_t *payload) {
    for (int i = 0; i < MB_N; i++) mb->data[i] = payload[i];
    mb->ready = 1; /* 置位务必在写入之后 */
}

/* 学生填：消费者一步——仅当 ready 才拷贝；拷贝后清 ready。
 * 返回 1 表示读到（out 已填），0 表示无数据。 */
static int consumer_step(Mailbox *mb, uint32_t *out) {
    if (!mb->ready) return 0;
    for (int i = 0; i < MB_N; i++) out[i] = mb->data[i];
    mb->ready = 0;
    return 1;
}

static int sub_mailbox(void) {
    Mailbox mb;
    memset(&mb, 0, sizeof(mb));
    uint32_t rounds[3][MB_N] = { {1,2,3}, {10,20,30}, {100,200,300} };
    int ok = 1;
    uint32_t out[MB_N];
    for (int i = 0; i < 3; i++) {
        /* 探针：未置位时消费者绝不能读到（无撕裂读）。 */
        if (consumer_step(&mb, out)) {
            printf("MAILBOX_FAIL 第%d轮：ready 未置位即被读取\n", i);
            ok = 0;
        }
        producer_step(&mb, rounds[i]);
        if (!consumer_step(&mb, out)) {
            printf("MAILBOX_FAIL 第%d轮：置位后仍读不到\n", i);
            ok = 0;
        } else {
            for (int k = 0; k < MB_N; k++) if (out[k] != rounds[i][k]) {
                printf("MAILBOX_FAIL 第%d轮 第%d项=%u 期望=%u\n", i, k, out[k], rounds[i][k]);
                ok = 0;
            }
        }
    }
    if (ok) printf("MAILBOX_PASS\n");
    return ok;
}

/* ─────────────────── 4) 共享环（定长 ring mailbox）─────────────────── */
#define CAP 4

typedef struct { uint32_t buf[CAP]; int head, tail, count; } Ring;

static void ring_init(Ring *r) { memset(r, 0, sizeof(*r)); }
static int ring_avail(const Ring *r) { return r->count > 0; }

/* 学生填：入队（生产侧）。满则返回 0；否则写 tail、抬 tail、count+1。 */
static int ring_push(Ring *r, uint32_t x) {
    if (r->count == CAP) return 0;
    r->buf[r->tail] = x;
    r->tail = (r->tail + 1) % CAP;
    r->count++;
    return 1;
}

/* 学生填：出队（消费侧）。空则返回 0；否则经 *out 给出 head 元素、抬 head、count-1。 */
static int ring_pop(Ring *r, uint32_t *out) {
    if (r->count == 0) return 0;
    *out = r->buf[r->head];
    r->head = (r->head + 1) % CAP;
    r->count--;
    return 1;
}

static int sub_ring(void) {
    Ring r;
    ring_init(&r);
    int ok = 1;
    uint32_t fill[4] = {11, 22, 33, 44};
    for (int i = 0; i < 4; i++)
        if (!ring_push(&r, fill[i])) { printf("RING_FAIL 入队 %u 失败\n", fill[i]); ok = 0; }
    if (ring_push(&r, 55)) { printf("RING_FAIL 满后竟接受入队\n"); ok = 0; }
    uint32_t got;
    for (int i = 0; i < 4; i++) {
        if (!ring_pop(&r, &got) || got != fill[i]) { printf("RING_FAIL 出队 期望 %u\n", fill[i]); ok = 0; }
    }
    if (ring_pop(&r, &got)) { printf("RING_FAIL 空队竟出队\n"); ok = 0; }
    uint32_t wrap[3] = {61, 62, 63};
    for (int i = 0; i < 3; i++) ring_push(&r, wrap[i]);
    for (int i = 0; i < 3; i++) {
        if (!ring_pop(&r, &got) || got != wrap[i]) { printf("RING_FAIL 环绕出队 期望 %u\n", wrap[i]); ok = 0; }
    }
    if (ok) printf("RING_PASS\n");
    return ok;
}

/* ─────────────────── 5) 同一环，换 MMIO 语义 ─────────────────── */
static int sub_mmio(void) {
    Ring mbox;
    ring_init(&mbox);
    uint32_t stream[5] = {0xD0, 0xD1, 0xD2, 0xD3, 0xD4};
    int produced = 0, ncons = 0;
    uint32_t consumed[5];
    long guard = 0;
    while (ncons < 5) {
        if (++guard > 100000) { printf("MMIO_SHM_FAIL 推进超时（push/pop 未实现？）\n"); return 0; }
        if (produced < 5 && ring_push(&mbox, stream[produced])) produced++;
        uint32_t v;
        if (ring_avail(&mbox) && ring_pop(&mbox, &v)) consumed[ncons++] = v;
    }
    for (int i = 0; i < 5; i++) if (consumed[i] != stream[i]) {
        printf("MMIO_SHM_FAIL 第%d项=%u 期望=%u\n", i, consumed[i], stream[i]);
        return 0;
    }
    printf("MMIO_SHM_PASS\n");
    return 1;
}

int main(void) {
    int all = 1;
    all &= sub_alias();
    all &= sub_mmap();
    all &= sub_mailbox();
    all &= sub_ring();
    all &= sub_mmio();
    if (all) { printf("ALL_PASS\n"); return 0; }
    return 1;
}
