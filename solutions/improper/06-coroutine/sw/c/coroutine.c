/* 无栈协程：被 poll 出来的「状态机」绿色线程 —— C 参考解。
 *
 * 主线：顺序代码 → 状态机 → 谁来生成这台状态机。
 *   06.1 手写「暂停—恢复」状态机（poll 的本质）  → YIELD_PASS / STATEMACHINE_PASS
 *   06.2 极简协作执行器（合作式调度 / 退化批处理）→ EXEC_PASS / BATCH_PASS
 *   06.3 就绪与唤醒（别空转 busy-poll）           → WAKER_PASS
 *   06.4 库式封装（函数指针「状态机库」）         → LIB_PASS（辅助分）
 *
 * 「无栈」的肉身体验：凡是跨让出点还要活着的局部（i / acc），都必须放进协程结构体，
 * 而不是函数栈上——因为根本没有「每任务一根独立栈」。
 *
 * 注意：C 没有 async/await（这正是要点），所以 06.4 只能停在「函数指针状态机库」这一层；
 *      「编译器替你生成状态机」的体验留给 Rust 版与 essay。
 *
 * 学生只填带 TODO 的函数体；下方测试 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>

/* poll 的返回：让出一个值（PENDING）或结束并给出终值（READY）。 */
typedef enum { ST_PENDING, ST_READY } StKind;
typedef struct { StKind kind; uint32_t val; } Step;

/* 一台无栈协程：从 start 起、步长 step、共让出 count 次，终值 = seed + 让出值之和。
 * 状态号就是 i：poll 一次推进一步，进度记在自己身上。 */
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

/* poll 一次：把状态机往前推一步。
 * 状态号 = c->i：i < count 时让出 start + i*step 并累加；i == count 时收尾。 */
static Step co_poll(Coro *c) {
    Step s;
    /* TODO[a] 显式状态字（i 即状态）+ 分支： */
    if (c->i < c->count) {
        uint32_t v = c->start + c->i * c->step; /* v 是「本步」局部 */
        c->acc += v;                            /* acc 必须活到收尾——所以它在 struct 里 */
        c->i += 1;
        s.kind = ST_PENDING; s.val = v;
    } else {
        c->done = 1;
        s.kind = ST_READY; s.val = c->seed + c->acc;
    }
    return s;
    /* ELSE[b] 也可用 protothread 宏（Duff's device）：
     *   #define PT_BEGIN(c) switch((c)->i){ case 0:
     *   #define PT_YIELD(c,x) do{ (c)->i=__LINE__; return (x); case __LINE__:; }while(0)
     *   #define PT_END(c)   }
     * ——用 __LINE__ 自动生成 case，「宏即编译器」，但局部仍须塞进 struct。 */
}

/* ════════════════ 06.2 极简协作执行器（学生填）════════════════ */

typedef struct {
    uint32_t trace[64]; int ntr;   /* 各任务让出值，按 poll 顺序交错 */
    uint32_t finals[16]; int nfin; /* 各任务终值，按完成顺序 */
} RunLog;

/* round-robin 依次 poll 每个未完成任务：Pending 记下让出值；Ready 记下终值。直到全部完成。 */
static void exec_run(Coro *cs, int n, RunLog *out) {
    out->ntr = 0; out->nfin = 0;
    /* TODO[a] 简单数组轮询：反复扫一遍所有任务，未完成的各 poll 一次。 */
    int remaining = n;
    while (remaining > 0) {
        for (int k = 0; k < n; k++) {
            if (cs[k].done) continue;
            Step s = co_poll(&cs[k]);
            if (s.kind == ST_PENDING) {
                out->trace[out->ntr++] = s.val;
            } else {
                out->finals[out->nfin++] = s.val;
                remaining--;
            }
        }
    }
    /* ELSE[b] 也可维护一个显式就绪队列（环形数组），出队 poll、Pending 再入队。 */
}

/* ════════════════ 06.3 就绪与唤醒（学生填）════════════════ */

/* 玩具 reactor + waker：每个 Pending 只「登记一次唤醒」，执行器只重 poll 被唤醒者，
 * 而不是把所有 Pending 反复空转。回填 *polls / *wakes。
 * 模型：就绪队列初始装入全部任务（首次 poll，不算唤醒）；
 *      之后每让出一次（Pending）= reactor 登记一次唤醒并重新入队。
 *      于是 总 poll = 任务数 + 唤醒数，绝不忙等。 */
static void reactor_run(Coro *cs, int n, uint32_t *polls, uint32_t *wakes) {
    *polls = 0; *wakes = 0;
    /* TODO[a] 极简就绪队列：只 poll 队列里的任务（= 被唤醒的）。 */
    int ready[256]; int head = 0, tail = 0;
    for (int k = 0; k < n; k++) ready[tail++] = k; /* 初始全部入队 */
    while (head < tail) {
        int id = ready[head++];
        Step s = co_poll(&cs[id]);
        (*polls)++;
        if (s.kind == ST_PENDING) {
            (*wakes)++;            /* 事件到了，重新入队（= 一次唤醒）*/
            ready[tail++] = id;
        }
        /* Ready：完成，不再入队 */
    }
    /* ELSE[b] 也可用 ready-flag 位图：只置位的才 poll。 */
}

/* ════════════════ 06.4 库式封装：函数指针「状态机库」（已提供，辅助分）════════════════
 *
 * C 没有 async/await，但可以把「一台被 poll 的状态机」抽象成一个通用 Task：
 *   一个 step 函数指针 + 一个不透明的 self。这就是 protothreads / 事件循环库的内核。
 * 下面这台通用执行器对任何「实现了 step 协议」的对象一视同仁——库式封装的精髓。 */

typedef Step (*StepFn)(void *self);

typedef struct {
    StepFn step;   /* 多态：怎么往前推一步 */
    void  *self;   /* 协程自己的状态（这里就是 Coro*）*/
    int    done;
    uint32_t result;
} Task;

static Step coro_adapter(void *self) { return co_poll((Coro *)self); } /* 把 Coro 适配成 Task */

/* 通用库执行器：round-robin 跑任意 Task，回填交错让出序列。 */
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
    Coro co = co_new(0, 10, 10, 3); /* 让出 [10,20,30]，终值 60 */
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
    Coro cs[2] = { co_new(0, 1, 1, 3), co_new(0, 10, 10, 2) }; /* 期望交错 [1,10,2,20,3] */
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
    /* 库式封装版应与手写执行器交错一致：[1,10,2,20,3] */
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
