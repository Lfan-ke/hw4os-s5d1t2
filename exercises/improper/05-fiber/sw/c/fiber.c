/* 纤程（有栈协程）—— C（qemu-user / RV64 真汇编）。
 * 你要填 5 处：① switch_ctx 汇编  ② spawn 首次跳板  ③ yield_now
 *            ④ 批处理让出点(body_inter)  ⑤ 类库阶段(ucontext) resume 序列。
 * 其余（运行时骨架 + 各阶段判定 harness）勿改。
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>

/* ── 上下文 = callee-saved 子集（rcore TaskContext 风格）── */
typedef struct { uint64_t ra, sp, s[12]; } TaskContext;   /* ra, sp, s0..s11 共 14×u64 */
extern void switch_ctx(TaskContext *old, TaskContext *next);

/* ① 手写 RV64 上下文切换（核心）。占位是空 ret（不切换）——运行后各阶段判 FAIL。
 *   要点：把 ra/sp/s0–s11 用 sd 存进 *old(=a0)，从 *next(=a1) 用 ld 读回，ret 跳走。
 *   偏移：ra@0 sp@8 s0@16 s1@24 … s11@104（每个 8 字节）。
 */
__asm__(
    ".globl switch_ctx\n"
    "switch_ctx:\n"
    /* TODO[a]: 在此写 14 条 sd 存旧 + 14 条 ld 载新，例如：
     *   sd ra,0(a0)  sd sp,8(a0)  sd s0,16(a0) … sd s11,104(a0)
     *   ld ra,0(a1)  ld sp,8(a1)  ld s0,16(a1) … ld s11,104(a1)
     * ELSE[b]: 也可把这段汇编单独放进 .S 文件（本课用 global_asm 内联）。 */
    "  ret\n"                      /* ← 占位：空切换 */
);

/* ── 极简纤程运行时 ── */
#define MAXF 8
#define STK  (64 * 1024)
enum { READY, RUNNING, DONE };
typedef struct { TaskContext ctx; unsigned char *stack; int state; void (*fn)(long); long arg; } Fiber;

static Fiber fibers[MAXF];
static int   nf;
static int   rq[100032], rh, rt_;      /* 就绪队列（大容量含安全阀余量）*/
static int   cur;
static TaskContext main_ctx;

/* ── 输出日志（各阶段复用，便于精确判定）── */
static char LOG[64][40];
static int  LOGN;
static void logput(const char *s) { size_t n = strlen(s); if (n > 39) n = 39; memcpy(LOG[LOGN], s, n); LOG[LOGN][n] = 0; LOGN++; }

static void rt_reset(void) {
    for (int i = 0; i < nf; i++) free(fibers[i].stack);
    nf = 0; rh = rt_ = 0; cur = -1; LOGN = 0;
}

void trampoline(void) {                /* 新纤程首次被调度的落点 */
    int i = cur;
    fibers[i].fn(fibers[i].arg);       /* 跑任务体 */
    fibers[i].state = DONE;
    switch_ctx(&fibers[i].ctx, &main_ctx);   /* 退场，永不返回 */
}

static void spawn(void (*fn)(long), long arg) {
    int i = nf++;
    fibers[i].stack = (unsigned char *)malloc(STK);
    fibers[i].state = READY;
    fibers[i].fn = fn; fibers[i].arg = arg;
    uint64_t top = ((uint64_t)(fibers[i].stack + STK)) & ~(uint64_t)15;  /* 16 对齐栈顶 */
    memset(&fibers[i].ctx, 0, sizeof(TaskContext));

    /* ② 首次跳板：让第一次被 switch 进来时一个 ret 就落进 trampoline、并用上新栈。
     *   占位什么都不设(ra=sp=0)——配合空 switch_ctx 不会崩，但纤程永不真正运行 → FAIL。 */
    /* TODO: fibers[i].ctx.ra = (uint64_t)trampoline;   // 首次 ret 的目标
     *       fibers[i].ctx.sp = top;                    // 指向新栈顶            */
    (void)top;

    rq[rt_++] = i;
}

static void yield_now(void) {
    /* ③ 让出：标记自己 READY，再 switch_ctx(自己, 调度器)。占位为空 → 不让出。 */
    /* TODO: fibers[cur].state = READY;
     *       switch_ctx(&fibers[cur].ctx, &main_ctx);                          */
}

static void run(void) {
    int steps = 0;                      /* 安全阀：未实现 switch_ctx 时防止空转死循环 */
    while (rh < rt_ && steps++ < 100000) {
        int i = rq[rh++];
        cur = i;
        fibers[i].state = RUNNING;
        switch_ctx(&main_ctx, &fibers[i].ctx);
        if (fibers[i].state != DONE) rq[rt_++] = i;   /* 让出 → 重新入队 */
    }
}

/* ── 各阶段任务体 ── */
#define PP_N 3
#define SCHED_ROUNDS 2
static void body_pingpong(long arg) { const char *s = arg ? "PONG" : "PING";
    for (int k = 0; k < PP_N; k++) { logput(s); yield_now(); } }
static void body_sched(long arg) { char b[8];
    for (int r = 1; r <= SCHED_ROUNDS; r++) { snprintf(b, sizeof b, "%c%d", (int)('A' + arg), r); logput(b); yield_now(); } }
static void body_seq(long arg) { char b[40]; volatile unsigned long acc = 0;
    for (int k = 0; k < 1000; k++) acc += (unsigned)k;
    snprintf(b, sizeof b, "task%ld_done(sum=%lu)", arg, acc); logput(b); }
static void body_inter(long arg) { char b[8];
    snprintf(b, sizeof b, "%lda", arg); logput(b);
    /* ④ 让出点：
     *   TODO[a]: 不插 yield —— 任务一口气跑完，三段退化为顺序(INTERLEAVE 判 FAIL)。
     *   ELSE[b]: 在此插入 yield_now(); 模拟 I/O 让出 —— 三个任务才会交错。       */
    snprintf(b, sizeof b, "%ldb", arg); logput(b); }

/* ── 阶段判定（harness 勿改）── */
static int stage_ctxsw(void) {
    rt_reset();
    spawn(body_pingpong, 0); spawn(body_pingpong, 1);
    run();
    for (int i = 0; i < LOGN; i++) printf("%s\n", LOG[i]);
    int ok = (LOGN == 2 * PP_N);
    for (int i = 0; ok && i < LOGN; i++) ok = !strcmp(LOG[i], (i & 1) ? "PONG" : "PING");
    if (ok) { printf("CTXSW_PASS\n"); return 1; }
    printf("CTXSW_FAIL\n"); return 0;
}
static int stage_sched(void) {
    rt_reset();
    spawn(body_sched, 0); spawn(body_sched, 1); spawn(body_sched, 2);
    run();
    for (int i = 0; i < LOGN; i++) printf("%s%c", LOG[i], i + 1 < LOGN ? ' ' : '\n');
    const char *want[] = { "A1", "B1", "C1", "A2", "B2", "C2" };
    int ok = (LOGN == 6);
    for (int i = 0; ok && i < 6; i++) ok = !strcmp(LOG[i], want[i]);
    if (ok) { printf("SCHED_PASS\n"); return 1; }
    printf("SCHED_FAIL\n"); return 0;
}
static int stage_seq(void) {
    rt_reset();
    for (long i = 0; i < 3; i++) spawn(body_seq, i);
    run();
    for (int i = 0; i < LOGN; i++) printf("%s%c", LOG[i], i + 1 < LOGN ? ' ' : '\n');
    int ok = (LOGN == 3)
        && !strncmp(LOG[0], "task0_done", 10)
        && !strncmp(LOG[1], "task1_done", 10)
        && !strncmp(LOG[2], "task2_done", 10);
    if (ok) { printf("SEQ_PASS\n"); return 1; }
    printf("SEQ_FAIL\n"); return 0;
}
static int stage_interleave(void) {
    rt_reset();
    for (long i = 0; i < 3; i++) spawn(body_inter, i);
    run();
    for (int i = 0; i < LOGN; i++) printf("%s%c", LOG[i], i + 1 < LOGN ? ' ' : '\n');
    int first_b = LOGN;
    for (int i = 0; i < LOGN; i++) if (LOG[i][strlen(LOG[i]) - 1] == 'b') { first_b = i; break; }
    int a_before = 0;
    for (int i = 0; i < first_b; i++) if (LOG[i][strlen(LOG[i]) - 1] == 'a') a_before++;
    if (LOGN == 6 && a_before == 3) { printf("INTERLEAVE_PASS\n"); return 1; }
    printf("INTERLEAVE_FAIL\n"); return 0;
}

/* ── 阶段 5：用类库设施（ucontext 有栈协程）复现同序列（Gen 勿改）── */
typedef struct { ucontext_t ctx, ret; long *vals; int n, i, done; long out; } Gen;
static Gen *g_cur;
static void gen_entry(void) {
    Gen *g = g_cur;
    for (g->i = 0; g->i < g->n; g->i++) { g->out = g->vals[g->i]; swapcontext(&g->ctx, &g->ret); }
    g->done = 1; swapcontext(&g->ctx, &g->ret);
}
static void gen_init(Gen *g, long *vals, int n) {
    g->vals = vals; g->n = n; g->i = 0; g->done = 0;
    getcontext(&g->ctx);
    g->ctx.uc_stack.ss_sp = malloc(STK);
    g->ctx.uc_stack.ss_size = STK;
    g->ctx.uc_link = NULL;
    makecontext(&g->ctx, gen_entry, 0);
}
static long gen_next(Gen *g) { g_cur = g; swapcontext(&g->ret, &g->ctx); return g->done ? -1 : g->out; }

static int stage_lib(void) {
    long va[] = { 1, 2, 3 }, vb[] = { 10, 20 };
    Gen a, b; gen_init(&a, va, 3); gen_init(&b, vb, 2);
    int order[] = { 0, 1, 0, 1, 0 };
    long seq[8]; int sn = 0;
    /* ⑤ 交替 resume 两个生成器(顺序 A B A B A)，把每次产出收进 seq —— 与手写 round-robin 同序。 */
    /* TODO: for (int k = 0; k < 5; k++) {
     *           long x = gen_next(order[k] ? &b : &a);
     *           if (x != -1) seq[sn++] = x;
     *       }                                                                  */
    (void)order; (void)gen_next;

    printf("[");
    for (int i = 0; i < sn; i++) printf("%ld%s", seq[i], i + 1 < sn ? ", " : "");
    printf("]\n");
    long want[] = { 1, 10, 2, 20, 3 };
    int ok = (sn == 5);
    for (int i = 0; ok && i < 5; i++) ok = (seq[i] == want[i]);
    free(a.ctx.uc_stack.ss_sp); free(b.ctx.uc_stack.ss_sp);
    if (ok) { printf("LIB_PASS\n"); return 1; }
    printf("LIB_FAIL\n"); return 0;
}

int main(void) {
    int all = 1;
    all &= stage_ctxsw();
    all &= stage_sched();
    all &= stage_seq();
    all &= stage_interleave();
    all &= stage_lib();
    if (all) { printf("ALL_PASS\n"); return 0; }
    return 1;
}
