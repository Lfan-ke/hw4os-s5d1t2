/* 异步事件 · 信号：软件建模一个「进程」的信号机制 —— C 参考解。
 *
 * 把一个「进程」抽象成三样东西 + 一个动作：
 *   - handler 表 handlers[NSIG] : 每个信号号注册的处理函数（NULL = 默认忽略）。
 *   - pending  位集 (uint32_t)  : 暂时投递不出去、挂起等待的信号（「位」不是「计数」）。
 *   - mask     位集 (uint32_t)  : 被屏蔽的信号；屏蔽期间来的信号只能进 pending。
 *   动作 raise(sig)：被屏蔽 → 入 pending；否则 → 立刻跑 handler（投递）。
 *   动作 unmask(sig)：解屏蔽，并把这期间攒下的 pending「补投递」出去。
 *
 * 四段逐题递进：
 *   1. DELIVER   —— 注册 handler，raise → handler 真的跑了、改了标志。
 *   2. MASK      —— 先 mask 再 raise → handler 不跑、信号进 pending。
 *   3. PENDING   —— unmask → 把 pending 补投递出来。
 *   4. REENTRANT —— handler 执行中再来同号信号 → 合并/不丢、且绝不递归重入崩。
 *
 * 学生只需填 raise / unmask 两个函数体；下方 run_handler 与测试 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>

#define NSIG 8

struct Proc;
typedef void (*Handler)(struct Proc *, int);

struct Proc {
    Handler handlers[NSIG];   /* 信号表：每号一个 handler（NULL=默认忽略） */
    uint32_t pending;         /* 挂起位集 */
    uint32_t mask;            /* 屏蔽位集 */
    uint32_t in_handler;      /* 正在处理中的信号（可重入守卫用） */
    uint32_t run_count[NSIG]; /* 每号 handler 实际跑了几次 */
    uint32_t depth;           /* 当前 handler 嵌套深度 */
    uint32_t max_depth;       /* 历史最大嵌套深度（应恒为 1） */
    uint32_t flag[NSIG];      /* handler 可改的标志 */
    uint32_t reent_left;      /* 可重入测试：首次进入时再触发几次同号信号 */
};

static uint32_t bit_(int sig) { return 1u << sig; }
static int is_masked(struct Proc *p, int sig) { return (p->mask & bit_(sig)) != 0; }
static int is_pending(struct Proc *p, int sig) { return (p->pending & bit_(sig)) != 0; }
static int is_in_handler(struct Proc *p, int sig) { return (p->in_handler & bit_(sig)) != 0; }

/* 注册 handler（given）。 */
static void install(struct Proc *p, int sig, Handler h) { p->handlers[sig] = h; }

/* 屏蔽一个信号（given）：只置 mask 位。 */
static void mask_sig(struct Proc *p, int sig) { p->mask |= bit_(sig); }

/* 真正把 handler 跑起来（given，勿改）。两个职责：
 *   1. 可重入守卫：若同号 handler 正在运行，绝不递归进入——合并进 pending 后返回。
 *   2. 返回时补投递：handler 跑完，若期间攒了同号 pending（且没被屏蔽），补投递一次。
 * 这正是「标准信号」语义：运行期间多次同号信号合并成一个，返回后只补送一次
 * （实时信号才会逐个排队，本模型不展开）。 */
static void run_handler(struct Proc *p, int sig) {
    /* —— 可重入守卫 —— */
    if (is_in_handler(p, sig)) {
        p->pending |= bit_(sig); /* 合并：handler 运行中再来同号，挂起、不递归 */
        return;
    }
    p->in_handler |= bit_(sig);
    p->depth += 1;
    if (p->depth > p->max_depth)
        p->max_depth = p->depth;
    if (p->handlers[sig]) {
        p->handlers[sig](p, sig); /* handler 里若再 raise 同号 → 经 raise→run_handler 被守卫拦下 */
        p->run_count[sig] += 1;
    }
    p->depth -= 1;
    p->in_handler &= ~bit_(sig);
    /* —— 返回补投递（合并后的那一个）—— */
    if (is_pending(p, sig) && !is_masked(p, sig)) {
        p->pending &= ~bit_(sig);
        run_handler(p, sig); /* 此时 in_handler 已清，不会被守卫拦 */
    }
}

/* ════════════════════════════════════════════════════════════════
 * 学生填空区：raise / unmask 两个函数
 * ════════════════════════════════════════════════════════════════ */

/* raise(sig)：投递一个信号 —— 「立刻投递」还是「先入 pending」？ */
static void raise_sig(struct Proc *p, int sig) {
    /* 被屏蔽 → 入 pending；否则 → 立刻投递。 */
    if (is_masked(p, sig))
        p->pending |= bit_(sig);
    else
        run_handler(p, sig);
}

/* unmask(sig)：解除屏蔽，并把屏蔽期间攒下的 pending「补投递」出去。 */
static void unmask_sig(struct Proc *p, int sig) {
    p->mask &= ~bit_(sig); /* 先放开屏蔽 */
    if (is_pending(p, sig)) {
        p->pending &= ~bit_(sig); /* 清 pending 位，补投递一次 */
        run_handler(p, sig);
    }
}

/* ════════════════════════════════════════════════════════════════
 * 测试用 handler（given）
 * ════════════════════════════════════════════════════════════════ */

/* 普通 handler：改一个标志，证明自己真的被调用过。 */
static void h_deliver(struct Proc *p, int sig) { p->flag[sig] = 0xA5; }

/* 可重入 handler：首次进入时连发若干次同号信号（合并成 1 个 pending）。 */
static void h_reentrant(struct Proc *p, int sig) {
    while (p->reent_left > 0) {
        p->reent_left -= 1;
        raise_sig(p, sig);
    }
}

/* ════════════════════════════════════════════════════════════════
 * 测试 harness（给定，勿改）
 * ════════════════════════════════════════════════════════════════ */

static int check_deliver(void) {
    int ok = 1;
    struct Proc p = {0};
    int sig = 3;
    install(&p, sig, h_deliver);

    /* 没注册 handler 的信号被 raise：默认忽略，不得跑、不得崩。 */
    raise_sig(&p, 5);
    if (p.run_count[5] != 0) {
        printf("DELIVER_BAD 无 handler 的信号竟被执行\n");
        ok = 0;
    }

    /* 注册后 raise：handler 必须跑一次、并改了标志。 */
    raise_sig(&p, sig);
    if (p.run_count[sig] != 1 || p.flag[sig] == 0) {
        printf("DELIVER_MISS raise 后 handler 没跑 run_count=%u flag=0x%02x\n",
               p.run_count[sig], p.flag[sig]);
        ok = 0;
    }
    if (is_pending(&p, sig)) {
        printf("DELIVER_BAD 未屏蔽却进了 pending\n");
        ok = 0;
    }

    if (ok)
        printf("DELIVER_PASS\n");
    return ok;
}

static int check_mask(void) {
    int ok = 1;
    struct Proc p = {0};
    int sig = 3;
    install(&p, sig, h_deliver);

    mask_sig(&p, sig);
    raise_sig(&p, sig);
    if (p.run_count[sig] != 0 || p.flag[sig] != 0) {
        printf("MASK_MISS 屏蔽期间 handler 竟然跑了 run_count=%u\n", p.run_count[sig]);
        ok = 0;
    }
    if (!is_pending(&p, sig)) {
        printf("MASK_BAD 屏蔽期间 raise 没有进 pending\n");
        ok = 0;
    }

    if (ok)
        printf("MASK_PASS\n");
    return ok;
}

static int check_pending(void) {
    int ok = 1;
    struct Proc p = {0};
    int sig = 3;
    install(&p, sig, h_deliver);

    mask_sig(&p, sig);
    raise_sig(&p, sig); /* 进 pending */
    raise_sig(&p, sig); /* 再来一次：合并，pending 仍只 1 个 */
    if (p.run_count[sig] != 0) {
        printf("PENDING_BAD 解屏蔽前不该执行 run_count=%u\n", p.run_count[sig]);
        ok = 0;
    }

    unmask_sig(&p, sig); /* 解屏蔽 → 补投递 */
    if (p.run_count[sig] != 1 || p.flag[sig] == 0) {
        printf("PENDING_MISS 解屏蔽后没有补投递 run_count=%u\n", p.run_count[sig]);
        ok = 0;
    }
    if (is_pending(&p, sig) || is_masked(&p, sig)) {
        printf("PENDING_BAD 补投递后 pending/mask 没清干净\n");
        ok = 0;
    }

    if (ok)
        printf("PENDING_PASS\n");
    return ok;
}

static int check_reentrant(void) {
    int ok = 1;
    struct Proc p = {0};
    int sig = 2;
    install(&p, sig, h_reentrant);
    p.reent_left = 3; /* handler 首次进入时连发 3 次同号信号 */

    raise_sig(&p, sig);

    /* 期望：初投递 1 + 合并补投递 1 = 共 2 次；从不递归(max_depth==1)；pending 清空。 */
    if (p.max_depth != 1) {
        printf("REENTRANT_BAD handler 被递归重入 max_depth=%u\n", p.max_depth);
        ok = 0;
    }
    if (p.run_count[sig] != 2) {
        printf("REENTRANT_MISS 期望执行 2 次(初投递+合并补投递)，实际 %u\n", p.run_count[sig]);
        ok = 0;
    }
    if (is_pending(&p, sig)) {
        printf("REENTRANT_BAD 收尾仍有 pending 未清\n");
        ok = 0;
    }

    if (ok)
        printf("REENTRANT_PASS\n");
    return ok;
}

int main(void) {
    int all = 1;
    all &= check_deliver();
    all &= check_mask();
    all &= check_pending();
    all &= check_reentrant();
    if (all) {
        printf("ALL_PASS\n");
        return 0;
    }
    return 1;
}
