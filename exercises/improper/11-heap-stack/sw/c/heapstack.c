/* 11 堆与栈：SP 是一根指针，allocator 是一个记账员 —— C。
 * 四段逐题递进（同一可执行里逐段点亮 *_PASS）：
 *   11.1 单设备单栈：SP 向下生长             → STACK_PASS
 *   11.2 两块设备：栈在 A、堆在 B，互不侵犯   → HEAP_INDEP_PASS
 *   11.3 一块设备：堆↑ 栈↓ 对向生长 + 碰撞检测 → COEXIST_PASS
 *   11.4 把 allocator 手动注册到全局指针      → GLOBAL_PASS
 * 你只需填各结构体函数 + 11.4 的「注册」一行；下方 harness（勿改）打印 *_PASS。
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

/* ===== 11.1 单设备单栈：SP 是一根向下生长的指针 ===== */

typedef struct {
    uint32_t mem[4];
    size_t base, top, sp; /* 满递减栈：sp 指向最近压入的槽；空时 sp==top */
} Stack;

static void stack_new(Stack *s) {
    for (int i = 0; i < 4; i++) s->mem[i] = 0;
    s->base = 0; s->top = 4; s->sp = 4;
}

static void sp_init(Stack *s) {
    /* TODO: 把 SP 初始化到内存区「顶端」（向下生长，故初值 = top）。
     * HINT: s->sp = s->top; */
    s->sp = s->base; /* ← 占位（错误：放到了栈底，一压就溢出） */
}

static int stack_push(Stack *s, uint32_t w) {
    /* TODO: 满递减栈 push = 先 sp-=1 再写；到 base 仍压 → 返回 -1（STACK_OVERFLOW）。
     * HINT: if (s->sp <= s->base) return -1; s->sp-=1; s->mem[s->sp]=w; return 0; */
    (void)s; (void)w;
    return -1; /* ← 占位 */
}

static int stack_pop(Stack *s, uint32_t *out) {
    /* TODO: pop = 先读 mem[sp] 再 sp+=1；空栈返回 -1，值经 *out 带回。 */
    (void)s;
    *out = 0;
    return -1; /* ← 占位 */
}

static int exp_stack(void) {
    Stack s; stack_new(&s);
    sp_init(&s);
    if (s.sp != s.top) { printf("STACK_FAIL sp_init 未把 SP 置到区顶\n"); return 0; }
    uint32_t data[4] = { 0x11, 0x22, 0x33, 0x44 };
    for (int i = 0; i < 4; i++)
        if (stack_push(&s, data[i]) != 0) { printf("STACK_FAIL 容量内 push 不应失败\n"); return 0; }
    if (stack_push(&s, 0x55) == 0) { printf("STACK_FAIL 越界 push 未被检出\n"); return 0; }
    for (int i = 3; i >= 0; i--) {
        uint32_t v;
        if (stack_pop(&s, &v) != 0 || v != data[i]) { printf("STACK_FAIL LIFO 还原错\n"); return 0; }
    }
    uint32_t dummy;
    if (stack_pop(&s, &dummy) == 0) { printf("STACK_FAIL 空栈 pop 应失败\n"); return 0; }
    printf("STACK_PASS\n");
    return 1;
}

/* ===== 11.2 两块设备：栈在 A、堆在 B，各管各的地盘 ===== */

typedef struct { uint32_t mem[16]; size_t cap, top; } HeapDev;

static void heap_new(HeapDev *h, size_t cap) {
    for (size_t i = 0; i < cap; i++) h->mem[i] = 0;
    h->cap = cap; h->top = 0;
}
static void heap_init(HeapDev *h) { h->top = 0; }

static int heap_alloc(HeapDev *h, size_t n, size_t *off) {
    /* TODO: bump 向上分配 n 个字：越上限（top+n > cap）→ 返回 -1（OOM）；
     *       否则 *off = top; top += n; 返回 0。 */
    (void)h; (void)n; (void)off;
    return -1; /* ← 占位 */
}

static int exp_indep(void) {
    /* 选「小而快的设备 A 作栈、大的设备 B 作堆」。
     *   TODO[a] A（小）作栈、B（大）作堆——栈帧小、堆需要大块连续空间。
     *   ELSE[b] 选 B 作栈也行，只要两设备各管各的。 这里取 TODO[a]。 */
    Stack a; stack_new(&a); sp_init(&a);
    HeapDev b; heap_new(&b, 16); heap_init(&b);

    size_t off;
    if (heap_alloc(&b, 2, &off) != 0) { printf("HEAP_FAIL 堆 B 首次 alloc 不应 OOM\n"); return 0; }
    b.mem[off] = 0xDEADBEEFu;
    b.mem[off + 1] = 0x0BADF00Du;

    uint32_t pushed = 0;
    while (stack_push(&a, 0xA5A50000u | pushed) == 0) {
        pushed++;
        if (pushed > 100u) { printf("HEAP_FAIL 栈 A 永不溢出？\n"); return 0; }
    }
    if (pushed != 4u) { printf("HEAP_FAIL 栈 A 容量应为 4，实测 %u\n", pushed); return 0; }

    size_t got = 2, tmp;
    while (heap_alloc(&b, 2, &tmp) == 0) {
        got += 2;
        if (got > 100u) { printf("HEAP_FAIL 堆 B 永不 OOM？\n"); return 0; }
    }
    if (got != 16u) { printf("HEAP_FAIL 堆 B 容量应为 16，实测 %zu\n", got); return 0; }

    if (b.mem[off] != 0xDEADBEEFu || b.mem[off + 1] != 0x0BADF00Du) {
        printf("HEAP_FAIL 栈 A 溢出竟改写了堆 B 的数据\n"); return 0;
    }
    uint32_t v;
    if (stack_pop(&a, &v) != 0 || v != (0xA5A50000u | 3u)) { printf("HEAP_FAIL 栈 A 数据被破坏\n"); return 0; }

    printf("HEAP_INDEP_PASS\n");
    return 1;
}

/* ===== 11.3 一块设备：堆↑ 栈↓ 对向生长，手动防侵犯 ===== */

typedef struct { uint32_t mem[8]; size_t cap, heap_top, sp; } OneDev;

static void onedev_new(OneDev *m, size_t cap) {
    for (size_t i = 0; i < cap; i++) m->mem[i] = 0;
    m->cap = cap; m->heap_top = 0; m->sp = cap;
}

static int onedev_alloc(OneDev *m, size_t n, size_t *off) {
    /* TODO: 堆 alloc 必须自查 heap_top + n <= sp，否则返回 -1（OOM）。
     *       没有独立设备兜底，碰撞全靠你手算这一句比较！ */
    (void)m; (void)n; (void)off;
    return -1; /* ← 占位 */
}

static int onedev_push(OneDev *m, uint32_t w) {
    /* TODO: 栈 push 必须自查 sp - 1 >= heap_top（即 sp > heap_top），
     *       否则返回 -1（STACK_OVERFLOW）；否则 sp-=1; mem[sp]=w。 */
    (void)m; (void)w;
    return -1; /* ← 占位 */
}

static int onedev_pop(OneDev *m, uint32_t *out) {
    if (m->sp >= m->cap) return -1;
    *out = m->mem[m->sp];
    m->sp += 1;
    return 0;
}

static int exp_coexist(void) {
    OneDev m; onedev_new(&m, 8);
    size_t heap_at[8]; uint32_t heap_val[8]; int hn = 0;
    uint32_t stack_val[8]; int sn = 0;
    uint32_t hv = 0x1000, sv = 0x9000;
    int guard = 0;
    for (;;) {
        size_t off; int did_heap = 0, did_stack = 0;
        if (onedev_alloc(&m, 1, &off) == 0) { m.mem[off] = hv; heap_at[hn] = off; heap_val[hn] = hv; hn++; hv++; did_heap = 1; }
        if (m.heap_top > m.sp) { printf("COLLIDE_UNDETECTED heap_top=%zu sp=%zu\n", m.heap_top, m.sp); return 0; }
        if (onedev_push(&m, sv) == 0) { stack_val[sn] = sv; sn++; sv++; did_stack = 1; }
        if (m.heap_top > m.sp) { printf("COLLIDE_UNDETECTED heap_top=%zu sp=%zu\n", m.heap_top, m.sp); return 0; }
        if (!did_heap && !did_stack) break;
        if (++guard > 10000) { printf("COEXIST_FAIL 循环未收敛（alloc/push 未正确自查边界）\n"); return 0; }
    }
    if (m.heap_top != m.sp) { printf("COEXIST_FAIL 未能恰好相遇于边界 heap_top=%zu sp=%zu\n", m.heap_top, m.sp); return 0; }
    for (int i = 0; i < hn; i++)
        if (m.mem[heap_at[i]] != heap_val[i]) { printf("COEXIST_FAIL 堆数据被静默覆盖\n"); return 0; }
    for (int i = sn - 1; i >= 0; i--) {
        uint32_t v;
        if (onedev_pop(&m, &v) != 0 || v != stack_val[i]) { printf("COEXIST_FAIL 栈 LIFO 还原错\n"); return 0; }
    }
    printf("COEXIST_PASS\n");
    return 1;
}

/* ===== 11.4 把 allocator 注册到 global（C 手动） ===== */

typedef struct { unsigned char *base; size_t cap, cursor; } Allocator;

static unsigned char arena_buf[4096];
static Allocator my_bump = { arena_buf, sizeof(arena_buf), 0 };

/* 全局分配器指针：C 没有语言级钩子，得手动把它接到某个 Allocator 上。 */
static Allocator *g_alloc = NULL;

static void *bump_alloc(Allocator *a, size_t n) {
    size_t aligned = (a->cursor + 15u) & ~((size_t)15u);
    if (aligned + n > a->cap) return NULL; /* OOM */
    void *p = a->base + aligned;
    a->cursor = aligned + n;
    return p;
}
static void bump_free(Allocator *a, void *p) { (void)a; (void)p; /* bump 只进不退 */ }

/* malloc/free 经全局指针 g_alloc 派发（这一段给好；你的活是「注册」g_alloc）。 */
static void *my_malloc(size_t n) {
    if (!g_alloc) return NULL; /* 没注册：返回 NULL，下方 exp_global 判 GLOBAL_FAIL */
    return bump_alloc(g_alloc, n);
}
static void my_free(void *p) {
    if (g_alloc) bump_free(g_alloc, p);
}

static int exp_global(void) {
    /* TODO（11.4 的核心一行）：注册——把你的 allocator 接到全局指针 g_alloc。
     *   这正是 rust 编译器替你做、而 C 里要你手动补的那件事。
     *   占位时缺这一行，my_malloc 因 g_alloc==NULL 返回 NULL → GLOBAL_FAIL。
     * HINT: g_alloc = &my_bump; */

    size_t before = my_bump.cursor;
    uint32_t *arr = (uint32_t *)my_malloc(40 * sizeof(uint32_t));
    int reg = (g_alloc != NULL);
    int got = (arr != NULL);
    size_t after = my_bump.cursor;
    int inside = got && (unsigned char *)arr >= my_bump.base
                     && (unsigned char *)arr < my_bump.base + my_bump.cap;
    if (got) {
        for (int i = 0; i < 40; i++) arr[i] = (uint32_t)i;
        my_free(arr);
    }
    if (reg && got && after > before && inside) { printf("GLOBAL_PASS\n"); return 1; }
    printf("GLOBAL_FAIL reg=%d got=%d grew=%d inside=%d\n", reg, got, (int)(after > before), inside);
    return 0;
}

/* ── 主流程（勿改）── */

int main(void) {
    int all = 1;
    all &= exp_stack();
    all &= exp_indep();
    all &= exp_coexist();
    all &= exp_global();
    if (all) { printf("ALL_PASS\n"); return 0; }
    return 1;
}
