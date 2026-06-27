/* 形态 F6 · 混合内核（hybrid）—— C 参考解。
 *
 * 母题：宏内核(F1)把所有服务塞内核态，syscall→fn call，几纳秒，最快但
 *       一个 driver bug 全系统崩；微内核(F2)把服务全赶到用户态，跨服务都走
 *       IPC，最隔离但每次都要陷入+切换+拷贝+回复，慢。
 *
 * 混合内核(Windows NT / macOS XNU)想「两全其美」：把性能关键服务留在内核态
 * 直调(快)，把需要隔离的服务放用户态走消息(隔离)。本 demo 用最朴素的软件模型
 * 把这个折中演示出来——同一份工作负载，按「全直调/混合/全消息」三路跑，
 * 统计开销与隔离度，亲眼看到混合「两头不靠」：比纯宏慢、比纯微不隔离。
 *
 * 学生只需填 1 个函数 route（按服务类型选直调或消息）；下方 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>

/* 放置策略：内核态直调(快,无隔离) vs 用户态消息(慢,有隔离)。 */
typedef enum { KERNEL, USER } Placement;

/* 服务类型：性能关键 vs 需要隔离。 */
typedef enum { PERF, ISOLATE } Kind;

/* 抽象开销模型（以「时钟拍」计）。 */
#define COST_DIRECT  1u  /* 内核态直调：一次 fn call */
#define COST_IPC     10u /* 用户态消息往返：陷入+切地址空间+拷贝+回复 */
#define MSGS_PER_IPC 2u  /* 每次用户态调用 = 请求 + 回复 两条消息 */

typedef uint64_t (*WorkFn)(uint64_t);

typedef struct {
    const char *name;
    Kind kind;
    WorkFn work;
} Service;

static uint64_t w_double(uint64_t x) { return x * 2; }
static uint64_t w_inc(uint64_t x) { return x + 1; }
static uint64_t w_sq(uint64_t x) { return (x * x) & 0xFFFFu; }

/* 7 个服务：3 个性能关键 + 4 个需要隔离。 */
static const Service SERVICES[] = {
    {"sched", PERF, w_double},
    {"timer", PERF, w_inc},
    {"mm", PERF, w_sq},
    {"fs", ISOLATE, w_double},
    {"net", ISOLATE, w_inc},
    {"driver", ISOLATE, w_sq},
    {"audio", ISOLATE, w_double},
};
#define N_SVC (sizeof(SERVICES) / sizeof(SERVICES[0]))

/* ════════════════════════════════════════════════════════════════
 * 学生填空区：唯一要填的服务路由
 * ════════════════════════════════════════════════════════════════ */

/* 按服务类型选放置策略：
 * 性能关键(PERF) → 内核态直调(KERNEL,快)；需要隔离(ISOLATE) → 用户态消息(USER,隔离)。
 * 这正是混合内核 Cutler(NT)/XNU 的设计直觉：热路径留 ring0，可替换/危险代码推用户态。 */
static Placement route(Kind kind) {
    switch (kind) {
    case PERF:
        return KERNEL;
    case ISOLATE:
        return USER;
    }
    return KERNEL;
}

/* ════════════════════════════════════════════════════════════════
 * 测试 harness（给定，勿改）
 * ════════════════════════════════════════════════════════════════ */

typedef struct {
    uint64_t kcalls; /* 内核态直调发生次数 */
    uint64_t umsgs;  /* 用户态消息条数(每次调用 2 条) */
    uint64_t cost;   /* 累计开销(时钟拍) */
} Metrics;

/* 派发一次服务调用：按路由决定走直调还是消息，累计开销，返回计算结果。
 * 关键：结果与放置无关——放哪都算对，放置只改变「开销」与「隔离」。 */
static uint64_t dispatch(const Service *s, uint64_t x, Metrics *m) {
    if (route(s->kind) == KERNEL) {
        m->kcalls += 1;
        m->cost += COST_DIRECT;
    } else {
        m->umsgs += MSGS_PER_IPC;
        m->cost += COST_IPC;
    }
    return s->work(x);
}

static int check_hybrid(void) {
    int ok = 1;
    size_t i;

    /* (a) 放置分布：必须既有内核直调服务，又有用户消息服务，才算「混合」。 */
    size_t nk = 0, nu = 0;
    for (i = 0; i < N_SVC; i++) {
        if (route(SERVICES[i].kind) == KERNEL)
            nk++;
        else
            nu++;
    }
    if (nk == 0) {
        printf("HYBRID_MISS 没有内核态直调服务(退化成纯微内核 F2)\n");
        ok = 0;
    }
    if (nu == 0) {
        printf("HYBRID_MISS 没有用户态消息服务(退化成纯宏内核 F1)\n");
        ok = 0;
    }

    /* (b) 两类服务都要真跑通：结果与直接调 work 一致(放置不改语义)。 */
    Metrics m = {0, 0, 0};
    for (i = 0; i < N_SVC; i++) {
        uint64_t y = dispatch(&SERVICES[i], 21, &m);
        uint64_t want = SERVICES[i].work(21);
        if (y != want) {
            printf("HYBRID_MISS 服务 %s 结果错 got=%llu want=%llu\n", SERVICES[i].name,
                   (unsigned long long)y, (unsigned long long)want);
            ok = 0;
        }
    }

    /* (c) 两条路径都要真实触发过。 */
    if (m.kcalls == 0 || m.umsgs == 0) {
        printf("HYBRID_MISS 两类调用路径未都触发 kcalls=%llu umsgs=%llu\n",
               (unsigned long long)m.kcalls, (unsigned long long)m.umsgs);
        ok = 0;
    }

    printf("PLACE 内核直调服务=%zu 用户消息服务=%zu（共 %zu）\n", nk, nu, N_SVC);
    if (ok)
        printf("HYBRID_PASS\n");
    return ok;
}

static int check_tradeoff(void) {
    int ok = 1;
    size_t i;

    /* 同一工作负载：每个服务各调用一次，按当前路由统计。 */
    Metrics m = {0, 0, 0};
    for (i = 0; i < N_SVC; i++)
        dispatch(&SERVICES[i], 10, &m);

    uint64_t n = (uint64_t)N_SVC;
    uint64_t mono = n * COST_DIRECT;  /* F1 全直调(纯宏)：最快 */
    uint64_t micro = n * COST_IPC;    /* F2 全消息(纯微)：最慢 */
    uint64_t hybrid = m.cost;         /* 混合：折中 */

    printf("COST mono(F1全直调)=%llu hybrid(混合)=%llu micro(F2全消息)=%llu\n",
           (unsigned long long)mono, (unsigned long long)hybrid, (unsigned long long)micro);
    printf("MSG  kcalls(内核直调计数)=%llu umsgs(用户消息计数)=%llu\n",
           (unsigned long long)m.kcalls, (unsigned long long)m.umsgs);

    /* (a) 折中：混合开销严格落在 F1 与 F2 之间——比纯宏慢(为隔离付费)、比纯微快(牺牲隔离)。 */
    if (!(mono < hybrid && hybrid < micro)) {
        printf("TRADEOFF_MISS 混合开销 %llu 未严格落在 F1(%llu) 与 F2(%llu) 之间\n",
               (unsigned long long)hybrid, (unsigned long long)mono, (unsigned long long)micro);
        ok = 0;
    }

    /* (b) 隔离度：混合的隔离(用户态)服务数严格介于 0(纯宏) 与 N(纯微) 之间——两头都不占满。 */
    size_t iso = 0;
    for (i = 0; i < N_SVC; i++)
        if (route(SERVICES[i].kind) == USER)
            iso++;
    if (!(iso > 0 && iso < N_SVC)) {
        printf("TRADEOFF_MISS 隔离服务数 %zu 未严格介于 0 与 %zu 之间\n", iso, N_SVC);
        ok = 0;
    }

    /* (c) 开销事件：用户态消息计数 > 内核态直调计数——IPC 的「隔离税」清晰可见。 */
    if (!(m.kcalls < m.umsgs)) {
        printf("TRADEOFF_MISS 内核直调计数 %llu 应 < 用户消息计数 %llu\n",
               (unsigned long long)m.kcalls, (unsigned long long)m.umsgs);
        ok = 0;
    }

    if (ok)
        printf("TRADEOFF_PASS\n");
    return ok;
}

int main(void) {
    int all = 1;
    all &= check_hybrid();
    all &= check_tradeoff();
    if (all) {
        printf("ALL_PASS\n");
        return 0;
    }
    return 1;
}
