/* 无栈协程：被 poll 出来的「状态机」绿色线程 —— C。
 *
 * 主线：顺序代码 → 状态机 → 谁来生成这台状态机。
 *   06.1 手写「暂停—恢复」状态机（poll 的本质）  → YIELD_PASS / STATEMACHINE_PASS
 *   06.2 极简协作执行器（合作式调度 / 退化批处理）→ EXEC_PASS / BATCH_PASS
 *   06.3 就绪与唤醒（别空转 busy-poll）           → WAKER_PASS
 *   06.4 库式封装（函数指针「状态机库」）         → LIB_PASS（辅助分，已提供）
 *
 * 「无栈」的肉身体验：凡是跨让出点还要活着的局部（i / acc），都必须放进协程结构体。
 * 注意：C 没有 async/await（这正是要点），「编译器替你生成状态机」的体验留给 Rust 版与 essay。
 *
 * 你只需填带 TODO 的函数体；下方测试 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>

typedef enum { ST_PENDING, ST_READY } StKind;
typedef struct { StKind kind; uint32_t val; } Step;

/* 一台无栈协程：状态号就是 i。 */
typedef struct {
    uint32_t seed, start, step, count;
    /* ↓↓↓ 跨让出点存活的局部，全部塞进结构体（这就是「无栈」）↓↓↓ */
    uint32_t i, acc;
    int done;
} Coro;

static Coro co_new(uint32_t seed, uint32_t start, uint32_t step, uint32_t count) {
    Coro c;
    c.seed = seed; c.start = start; c.step = step; c.count = count;
    c.i = 0; c.acc = 0; c.done = 0;
    return c;
}

/* ════════════════ 06.1 手写状态机（学生填）════════════════ */

/* poll 一次：把状态机往前推一步。状态号 = c->i。 */
static Step co_poll(Coro *c) {
    Step s;
    /* TODO[a] 显式状态字（i 即状态）+ 分支：
     *   若 c->i < c->count：算 v = start + i*step；acc += v；i += 1；返回 {ST_PENDING, v}。
     *   否则：置 c->done = 1；返回 {ST_READY, seed + acc}。
     * HINT: acc/i 必须留在 c 里——它们要跨让出点存活（这就是「无栈」）。
     * ELSE[b] 也可用 protothread 宏（Duff's device，用 __LINE__ 自动生成 case）。 */
    c->done = 1;
    s.kind = ST_READY; s.val = 0; /* ← 占位：直接结束、不让出，会判 FAIL */
    return s;
}

/* ════════════════ 06.2 极简协作执行器（学生填）════════════════ */

typedef struct {
    uint32_t trace[64]; int ntr;
    uint32_t finals[16]; int nfin;
} RunLog;

/* round-robin 依次 poll 每个未完成任务：Pending 记让出值；Ready 记终值。直到全部完成。 */
static void exec_run(Coro *cs, int n, RunLog *out) {
    out->ntr = 0; out->nfin = 0;
    /* TODO[a] 简单数组轮询：外层 while 还有未完成任务，内层 for 扫一遍所有任务，
     *   未完成的各 poll 一次：Pending → out->trace[out->ntr++]=v；
     *   Ready → out->finals[out->nfin++]=f 并把它标记完成（remaining--）。
     * HINT: 用一个 remaining 计数控制外层循环。
     * ELSE[b] 也可维护显式就绪队列（环形数组）。 */
    /* ← 占位：空账本，会判 FAIL */
    (void)cs; (void)n;
}

/* ════════════════ 06.3 就绪与唤醒（学生填）════════════════ */

/* 玩具 reactor + waker：每个 Pending 只「登记一次唤醒」，执行器只重 poll 被唤醒者。
 * 回填 *polls / *wakes。 */
static void reactor_run(Coro *cs, int n, uint32_t *polls, uint32_t *wakes) {
    *polls = 0; *wakes = 0;
    /* TODO[a] 极简就绪队列：ready 初始装入全部任务下标（首次 poll，不计唤醒）。
     *   循环出队一个 id，poll 它，(*polls)++；
     *   若 Pending → (*wakes)++ 且把 id 重新入队；若 Ready → 不再入队。队空即止。
     * HINT: 这样 总 poll = 任务数 + 唤醒数，绝不忙等。
     * ELSE[b] 也可用 ready-flag 位图：只置位的才 poll。 */
    /* ← 占位：什么都没跑，任务未完成会判 FAIL */
    (void)cs; (void)n;
}

/* ════════════════ 06.4 库式封装：函数指针「状态机库」（已提供，辅助分）════════════════
 *
 * C 没有 async/await，但可以把「一台被 poll 的状态机」抽象成通用 Task：
 *   一个 step 函数指针 + 一个不透明的 self。这就是 protothreads / 事件循环库的内核。 */

typedef Step (*StepFn)(void *self);

typedef struct {
    StepFn step;
    void  *self;
    int    done;
    uint32_t result;
} Task;

static Step coro_adapter(void *self) { return co_poll((Coro *)self); }

static void lib_run(Task *ts, int n, uint32_t *trace, int *ntr) {
    *ntr = 0;
    int remaining = n;
    while (remaining > 0) {
        for (int k = 0; k < n; k++) {
            if (ts[k].done) continue;
            Step s = ts[k].step(ts[k].self);
            if (s.kind == ST_PENDING) trace[(*ntr)++] = s.val;
            else { ts[k].done = 1; ts[k].result = s.val; remaining--; }
        }
    }
}

/* ════════════════════════ 测试 harness（勿改）════════════════════════ */

static int check_statemachine(void) {
    Coro co = co_new(0, 10, 10, 3);
    uint32_t seq[16]; int ns = 0; uint32_t fin = 0; int got_fin = 0;
    for (int it = 0; it < 100; it++) {
        Step s = co_poll(&co);
        if (s.kind == ST_PENDING) seq[ns++] = s.val;
        else { fin = s.val; got_fin = 1; break; }
    }
    int ok = 1;
    uint32_t want[3] = {10, 20, 30};
    if (ns != 3 || seq[0] != want[0] || seq[1] != want[1] || seq[2] != want[2]) {
        printf("YIELD_FAIL 让出 n=%d 期望 [10,20,30]\n", ns); ok = 0;
    } else {
        printf("YIELD_PASS\n");
    }
    if (got_fin && fin == 60 && ns + 1 == 4 && co.done) {
        printf("STATEMACHINE_PASS\n");
    } else {
        printf("STATEMACHINE_FAIL 终值=%u poll次数=%d 期望 60 / 4\n", fin, ns + 1); ok = 0;
    }
    return ok;
}

static int check_exec(void) {
    Coro cs[2] = { co_new(0, 1, 1, 3), co_new(0, 10, 10, 2) };
    RunLog log;
    exec_run(cs, 2, &log);
    uint32_t want[5] = {1, 10, 2, 20, 3};
    if (log.ntr != 5) { printf("EXEC_FAIL 交错 n=%d 期望 5\n", log.ntr); return 0; }
    for (int i = 0; i < 5; i++)
        if (log.trace[i] != want[i]) { printf("EXEC_FAIL pos%d=%u 期望 %u\n", i, log.trace[i], want[i]); return 0; }
    printf("EXEC_PASS\n");
    return 1;
}

static int check_batch(void) {
    Coro cs[3] = { co_new(100, 0, 0, 0), co_new(200, 0, 0, 0), co_new(300, 0, 0, 0) };
    RunLog log;
    exec_run(cs, 3, &log);
    uint32_t want[3] = {100, 200, 300};
    if (log.ntr != 0) { printf("BATCH_FAIL 不应有让出，n=%d\n", log.ntr); return 0; }
    if (log.nfin != 3) { printf("BATCH_FAIL 完成数=%d 期望 3\n", log.nfin); return 0; }
    for (int i = 0; i < 3; i++)
        if (log.finals[i] != want[i]) { printf("BATCH_FAIL pos%d=%u 期望 %u\n", i, log.finals[i], want[i]); return 0; }
    printf("BATCH_PASS\n");
    return 1;
}

static int check_waker(void) {
    Coro cs[3] = { co_new(0, 1, 1, 2), co_new(0, 5, 5, 1), co_new(0, 7, 7, 3) };
    uint32_t n = 3, polls = 0, wakes = 0;
    reactor_run(cs, 3, &polls, &wakes);
    int all_done = cs[0].done && cs[1].done && cs[2].done;
    if (all_done && polls > 0 && polls <= wakes + n) {
        printf("WAKER_PASS polls=%u <= wakes(%u)+tasks(%u)\n", polls, wakes, n);
        return 1;
    }
    printf("WAKER_FAIL polls=%u wakes=%u tasks=%u all_done=%d\n", polls, wakes, n, all_done);
    return 0;
}

static int check_lib(void) {
    Coro a = co_new(0, 1, 1, 3), b = co_new(0, 10, 10, 2);
    Task ts[2] = { { coro_adapter, &a, 0, 0 }, { coro_adapter, &b, 0, 0 } };
    uint32_t trace[64]; int ntr = 0;
    lib_run(ts, 2, trace, &ntr);
    uint32_t want[5] = {1, 10, 2, 20, 3};
    if (ntr != 5) { printf("LIB_FAIL 交错 n=%d 期望 5\n", ntr); return 0; }
    for (int i = 0; i < 5; i++)
        if (trace[i] != want[i]) { printf("LIB_FAIL pos%d=%u 期望 %u\n", i, trace[i], want[i]); return 0; }
    printf("LIB_PASS\n");
    return 1;
}

int main(void) {
    int all = 1;
    all &= check_statemachine(); /* 06.1 */
    all &= check_exec();         /* 06.2 */
    all &= check_batch();        /* 06.2 */
    all &= check_waker();        /* 06.3 */
    all &= check_lib();          /* 06.4（辅助分）*/

    if (all) { printf("ALL_PASS\n"); return 0; }
    return 1;
}
