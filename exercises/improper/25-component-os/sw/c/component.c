/* 不正经赛道 · 25 组件化 OS —— C 参考解（host 软件直觉 demo，不是真内核）。
 *
 * 母题（arceos 的精髓）：内核不必从头写。把可复用的「组件」(分配器/调度器/控制台)
 * 当积木，用「特性开关」(cargo features 的心智模型) 按需组装——同一套组件，
 * 不同拼法就拼出不同形态的 OS：
 *   ① 组件：每个有统一接口(函数指针 vtable) + 1~2 个可替换实现
 *      Allocator：bump / freelist；Scheduler：fifo / rr；Console：plain。
 *   ② 组装：KernelConfig 用特性开关选「装哪些组件 / 用哪个实现 / 要不要 syscall 边界」。
 *   ③ 两种形态：UNIKERNEL(最小组件 + app 直链、无 syscall 边界、零陷入)
 *              vs MONOLITHIC(更多组件 + syscall 边界、有陷入)——同源组件不同拼法。
 *   ④ 热替换：把分配器/调度器换一个实现，OS 仍正常工作。
 *
 * 学生只填两处：build_kernel(按特性组装/wiring) + freelist_alloc(替换一个组件实现)。
 * 其余 harness 勿改。Rust/C 两份逻辑逐字对应。
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define SLOT 64u  /* freelist 的固定槽大小 */
#define CAP  16u  /* freelist 的槽数上限 */
#define MAXTRACE 16

/* 分配器组件共享的那点状态（bump 用 top，freelist 用 slots）。 */
struct alloc_state {
    size_t top;    /* bump 游标 */
    uint32_t slots; /* freelist 已用槽数 */
};

/* Allocator / Console / Scheduler 三个组件的 vtable（名字 + 函数指针）。 */
struct allocator {
    const char *name;
    int64_t (*alloc)(struct alloc_state *, size_t);
};
struct console {
    const char *name;
    int64_t (*write)(size_t *, size_t);
};
struct task {
    uint32_t id;
    uint32_t burst;
};
struct scheduler {
    const char *name;
    size_t (*run)(const struct task *, size_t, uint32_t *);
};

/* 前置声明（注册表会引用 freelist_alloc）。 */
static int64_t freelist_alloc(struct alloc_state *s, size_t n);

/* ── 组件实现：分配器两种 ───────────────────────────────────────── */

/* bump 分配：紧凑前移，off = top，top += n（连续区间紧贴、不重叠）。 */
static int64_t bump_alloc(struct alloc_state *s, size_t n) {
    int64_t off = (int64_t)s->top;
    s->top += n;
    return off;
}

/* ── 组件实现：调度器两种 ───────────────────────────────────────── */

/* FIFO（运行到完成）：一个任务跑光它的 burst 步，再轮下一个。 */
static size_t fifo_run(const struct task *tasks, size_t nt, uint32_t *out) {
    size_t idx = 0, i;
    uint32_t b;
    for (i = 0; i < nt; i++)
        for (b = 0; b < tasks[i].burst; b++)
            out[idx++] = tasks[i].id;
    return idx;
}

/* Round-Robin（时间片=1）：每轮每个未完成任务走一步，轮转直到全部排空。 */
static size_t rr_run(const struct task *tasks, size_t nt, uint32_t *out) {
    uint32_t rem[MAXTRACE];
    size_t idx = 0, i;
    uint32_t left = 0;
    for (i = 0; i < nt; i++) {
        rem[i] = tasks[i].burst;
        left += tasks[i].burst;
    }
    while (left > 0) {
        for (i = 0; i < nt; i++) {
            if (rem[i] > 0) {
                out[idx++] = tasks[i].id;
                rem[i]--;
                left--;
            }
        }
    }
    return idx;
}

/* ── 组件实现：控制台一种 ───────────────────────────────────────── */

static int64_t console_plain(size_t *len, size_t n) {
    *len += n;
    return (int64_t)n;
}

/* ── 组件「注册表 / 工厂」：按特性枚举取对应实现（给定）────────────── */

enum alloc_kind { ALLOC_BUMP, ALLOC_FREELIST };
enum sched_kind { SCHED_NONE, SCHED_FIFO, SCHED_RR };

static struct allocator make_allocator(enum alloc_kind kind) {
    struct allocator a;
    if (kind == ALLOC_FREELIST) {
        a.name = "freelist";
        a.alloc = freelist_alloc;
    } else {
        a.name = "bump";
        a.alloc = bump_alloc;
    }
    return a;
}
static struct scheduler make_scheduler(enum sched_kind kind) {
    struct scheduler s;
    if (kind == SCHED_RR) {
        s.name = "rr";
        s.run = rr_run;
    } else { /* NONE 也给个缺省，has_sched=0 时不会被用到 */
        s.name = "fifo";
        s.run = fifo_run;
    }
    return s;
}
static struct console make_console(void) {
    struct console c;
    c.name = "plain";
    c.write = console_plain;
    return c;
}

/* ── 组装出来的内核：一组被选中的组件 + 它们的状态 ────────────────── */

struct kernel_config {
    enum alloc_kind alloc_kind;
    enum sched_kind sched_kind;
    int syscall; /* 1=有 syscall 边界(mono)，0=直链(uni) */
};

struct kernel {
    struct console console;
    size_t console_len;
    struct allocator alloc;
    struct alloc_state astate;
    struct scheduler sched;
    int has_sched;        /* unikernel 单应用：根本不链调度器 */
    int syscall_boundary; /* mono：app→OS 过 syscall（陷入）；uni：直链（无陷入） */
    uint64_t traps;
};

/* 系统服务号（mono 里是 syscall 号，uni 里只是 switch 分支）。 */
#define SVC_WRITE 0u
#define SVC_ALLOC 1u

/* ════════════════════════════════════════════════════════════════
 * 学生填空区 ①：组装 / wiring —— 按 cfg 的特性开关选组件实现并接线
 * ════════════════════════════════════════════════════════════════ */

/* 按配置组装一个内核：选分配器实现、选调度器实现（或不装）、设 syscall 边界。
 * 这就是「同一套组件、按特性拼出不同形态」的那道组装工序。 */
static struct kernel build_kernel(const struct kernel_config *cfg) {
    /* TODO: 按 cfg 的特性开关组装：
     *   k.alloc = make_allocator(cfg->alloc_kind)
     *   k.has_sched = (cfg->sched_kind != SCHED_NONE)；k.sched = make_scheduler(cfg->sched_kind)
     *   k.syscall_boundary = cfg->syscall
     * 占位：永远拼成「最小直链」一种形态（忽略 cfg）——能编译，但 MONO/SWAP 拼不出来。 */
    struct kernel k;
    (void)cfg;
    k.console = make_console();
    k.console_len = 0;
    k.alloc = make_allocator(ALLOC_BUMP);
    k.astate.top = 0;
    k.astate.slots = 0;
    k.sched = make_scheduler(SCHED_FIFO);
    k.has_sched = 0;
    k.syscall_boundary = 0;
    k.traps = 0;
    return k;
}

/* ════════════════════════════════════════════════════════════════
 * 学生填空区 ②：替换一个组件实现 —— freelist 分配器
 * ════════════════════════════════════════════════════════════════ */

/* freelist 分配：固定槽分配——off = slots*SLOT，slots += 1；过大或满返回 -1。
 * 和 bump 策略不同，但同样满足「连续分配区间不重叠」这条 OS 级契约（故可热替换）。 */
static int64_t freelist_alloc(struct alloc_state *s, size_t n) {
    /* TODO: 固定槽分配——若 n > SLOT 或 s->slots >= CAP 返回 -1；
     *       否则 off = (int64_t)((size_t)s->slots * SLOT)；s->slots += 1；返回 off。 */
    (void)s;
    (void)n;
    return -1; /* 占位：永远 OOM（换上它的 SWAP 会失败） */
}

/* ════════════════════════════════════════════════════════════════
 * 测试 harness（给定，勿改）
 * ════════════════════════════════════════════════════════════════ */

/* app→OS 的统一入口：mono 过 syscall 边界(traps+1)，uni 直链(不碰陷入)。
 * 不管哪种形态，最终都路由到同一批组件函数——差别只在那道陷入墙。 */
static int64_t kcall(struct kernel *k, uint32_t svc, uint32_t arg) {
    if (k->syscall_boundary)
        k->traps += 1; /* ← 陷入开销：unikernel 形态没有这一行 */
    switch (svc) {
        case SVC_WRITE: return k->console.write(&k->console_len, (size_t)arg);
        case SVC_ALLOC: return k->alloc.alloc(&k->astate, (size_t)arg);
        default: return -1;
    }
}

/* 校验一串 (off,n) 分配区间：都 >=0 且两两不重叠（OS 级不变量，与用哪个分配器无关）。 */
static int allocs_disjoint(const int64_t *off, const size_t *len, size_t cnt) {
    size_t i, j;
    for (i = 0; i < cnt; i++)
        if (off[i] < 0)
            return 0;
    for (i = 0; i < cnt; i++)
        for (j = i + 1; j < cnt; j++) {
            int64_t a0 = off[i], a1 = off[i] + (int64_t)len[i];
            int64_t b0 = off[j], b1 = off[j] + (int64_t)len[j];
            if (a0 < b1 && b0 < a1)
                return 0; /* 重叠 */
        }
    return 1;
}

/* 校验调度迹：每个任务恰好出现 burst 次、总步数对、无杂质 id（= 所有任务都跑完）。 */
static int sched_complete(const struct task *tasks, size_t nt, const uint32_t *trace, size_t n) {
    uint32_t total = 0;
    size_t i, j;
    for (i = 0; i < nt; i++)
        total += tasks[i].burst;
    if ((uint32_t)n != total)
        return 0;
    for (i = 0; i < nt; i++) {
        uint32_t cnt = 0;
        for (j = 0; j < n; j++)
            if (trace[j] == tasks[i].id)
                cnt++;
        if (cnt != tasks[i].burst)
            return 0;
    }
    for (j = 0; j < n; j++) {
        int known = 0;
        for (i = 0; i < nt; i++)
            if (trace[j] == tasks[i].id)
                known = 1;
        if (!known)
            return 0;
    }
    return 1;
}

static const struct task DEMO_TASKS[3] = {
    {1, 2}, {2, 3}, {3, 1},
};
#define NTASK 3

/* 判据 1：组件可独立使用、且可按特性选/换。 */
static int check_component(void) {
    int ok = 1;

    /* (a) 注册表按特性选实现。 */
    if (strcmp(make_allocator(ALLOC_BUMP).name, "bump") != 0 ||
        strcmp(make_allocator(ALLOC_FREELIST).name, "freelist") != 0 ||
        strcmp(make_scheduler(SCHED_FIFO).name, "fifo") != 0 ||
        strcmp(make_scheduler(SCHED_RR).name, "rr") != 0 ||
        strcmp(make_console().name, "plain") != 0) {
        printf("COMPONENT_FAIL 注册表按特性选错了实现\n");
        ok = 0;
    }

    /* (b) 分配器组件独立可用：bump 紧凑分配 0,32，游标 48。 */
    struct alloc_state s = {0, 0};
    int64_t a0 = bump_alloc(&s, 32);
    int64_t a1 = bump_alloc(&s, 16);
    if (a0 != 0 || a1 != 32 || s.top != 48) {
        printf("COMPONENT_FAIL bump 分配器 a0=%lld a1=%lld top=%zu 应=(0,32,48)\n",
               (long long)a0, (long long)a1, s.top);
        ok = 0;
    }

    /* (c) 控制台组件独立可用。 */
    size_t len = 0;
    int64_t w = console_plain(&len, 5);
    console_plain(&len, 3);
    if (w != 5 || len != 8) {
        printf("COMPONENT_FAIL console 组件 w=%lld len=%zu 应=(5,8)\n", (long long)w, len);
        ok = 0;
    }

    /* (d) 两个调度器组件都把所有任务跑完。 */
    uint32_t tf[MAXTRACE], tr[MAXTRACE];
    size_t nf = fifo_run(DEMO_TASKS, NTASK, tf);
    size_t nr = rr_run(DEMO_TASKS, NTASK, tr);
    if (!sched_complete(DEMO_TASKS, NTASK, tf, nf) ||
        !sched_complete(DEMO_TASKS, NTASK, tr, nr)) {
        printf("COMPONENT_FAIL 调度器组件未把任务跑完\n");
        ok = 0;
    }

    if (ok)
        printf("COMPONENT_PASS\n");
    return ok;
}

/* 判据 2：把组件组装成 UNIKERNEL 形态并跑通。 */
static int check_compose_uni(void) {
    int ok = 1;
    struct kernel_config cfg = {ALLOC_BUMP, SCHED_NONE, 0};
    struct kernel k = build_kernel(&cfg);

    if (k.has_sched) {
        printf("COMPOSE_UNI_FAIL unikernel 形态不该装调度器\n");
        ok = 0;
    }
    if (k.syscall_boundary) {
        printf("COMPOSE_UNI_FAIL unikernel 形态不该有 syscall 边界\n");
        ok = 0;
    }

    int64_t banner = kcall(&k, SVC_WRITE, 6);
    int64_t off[2];
    size_t len[2] = {32, 16};
    off[0] = kcall(&k, SVC_ALLOC, 32);
    off[1] = kcall(&k, SVC_ALLOC, 16);

    if (banner != 6 || k.console_len != 6) {
        printf("COMPOSE_UNI_FAIL banner ret=%lld len=%zu 应=(6,6)\n",
               (long long)banner, k.console_len);
        ok = 0;
    }
    if (!allocs_disjoint(off, len, 2)) {
        printf("COMPOSE_UNI_FAIL 分配区重叠 p0=%lld p1=%lld\n",
               (long long)off[0], (long long)off[1]);
        ok = 0;
    }
    if (k.traps != 0) {
        printf("TRAP_LEAK_FAIL unikernel 直链却产生了 %llu 次陷入\n",
               (unsigned long long)k.traps);
        ok = 0;
    }

    printf("FORM_UNI 组件={console:%s,alloc:%s} sched=无 陷入=%llu\n",
           k.console.name, k.alloc.name, (unsigned long long)k.traps);
    if (ok)
        printf("COMPOSE_UNI_PASS\n");
    return ok;
}

/* 判据 3：把（更多）组件组装成 MONOLITHIC 形态并跑通。 */
static int check_compose_mono(void) {
    int ok = 1;
    struct kernel_config cfg = {ALLOC_BUMP, SCHED_FIFO, 1};
    struct kernel k = build_kernel(&cfg);

    if (!k.has_sched) {
        printf("COMPOSE_MONO_FAIL 宏内核形态应当装调度器\n");
        ok = 0;
    }
    if (!k.syscall_boundary) {
        printf("COMPOSE_MONO_FAIL 宏内核形态应当有 syscall 边界\n");
        ok = 0;
    }

    uint32_t wsvc[4] = {SVC_WRITE, SVC_ALLOC, SVC_WRITE, SVC_ALLOC};
    uint32_t warg[4] = {4, 16, 2, 8};
    int64_t off[2];
    size_t len[2] = {16, 8};
    size_t ai = 0, i;
    for (i = 0; i < 4; i++) {
        int64_t r = kcall(&k, wsvc[i], warg[i]);
        if (wsvc[i] == SVC_ALLOC)
            off[ai++] = r;
    }
    if (k.traps != 4) {
        printf("COMPOSE_MONO_FAIL 陷入数=%llu 应=4\n", (unsigned long long)k.traps);
        ok = 0;
    }
    if (!allocs_disjoint(off, len, 2)) {
        printf("COMPOSE_MONO_FAIL 分配区重叠 p0=%lld p1=%lld\n",
               (long long)off[0], (long long)off[1]);
        ok = 0;
    }

    uint32_t tr[MAXTRACE];
    size_t n = k.sched.run(DEMO_TASKS, NTASK, tr);
    if (!sched_complete(DEMO_TASKS, NTASK, tr, n)) {
        printf("COMPOSE_MONO_FAIL 调度器(%s)未把进程跑完\n", k.sched.name);
        ok = 0;
    }

    printf("FORM_MONO 组件={console:%s,alloc:%s,sched:%s} 陷入=%llu\n",
           k.console.name, k.alloc.name, k.sched.name, (unsigned long long)k.traps);
    if (ok)
        printf("COMPOSE_MONO_PASS\n");
    return ok;
}

/* 判据 4：热替换一个组件实现，OS 仍工作（同源组件、换个拼法）。 */
static int check_swap(void) {
    int ok = 1;
    struct kernel_config cfg = {ALLOC_FREELIST, SCHED_RR, 1};
    struct kernel k = build_kernel(&cfg);

    if (strcmp(k.alloc.name, "freelist") != 0 || strcmp(k.sched.name, "rr") != 0) {
        printf("SWAP_FAIL 没换上替换实现 alloc=%s sched=%s\n", k.alloc.name, k.sched.name);
        ok = 0;
    }

    uint32_t wsvc[4] = {SVC_WRITE, SVC_ALLOC, SVC_WRITE, SVC_ALLOC};
    uint32_t warg[4] = {4, 16, 2, 8};
    int64_t off[2];
    size_t len[2] = {16, 8};
    size_t ai = 0, i;
    for (i = 0; i < 4; i++) {
        int64_t r = kcall(&k, wsvc[i], warg[i]);
        if (wsvc[i] == SVC_ALLOC) {
            if (r < 0) {
                printf("SWAP_FAIL freelist 分配失败返回 %lld\n", (long long)r);
                ok = 0;
            }
            off[ai++] = r;
        }
    }
    if (!allocs_disjoint(off, len, 2)) {
        printf("SWAP_FAIL 换 freelist 后分配区重叠 p0=%lld p1=%lld\n",
               (long long)off[0], (long long)off[1]);
        ok = 0;
    }
    if (k.console_len != 6) {
        printf("SWAP_FAIL 换组件后控制台异常 len=%zu 应=6\n", k.console_len);
        ok = 0;
    }

    uint32_t tr[MAXTRACE];
    size_t n = k.sched.run(DEMO_TASKS, NTASK, tr);
    if (!sched_complete(DEMO_TASKS, NTASK, tr, n)) {
        printf("SWAP_FAIL 换 rr 后未把进程跑完\n");
        ok = 0;
    }

    printf("HOTSWAP alloc bump→%s sched fifo→%s OS 仍工作\n", k.alloc.name, k.sched.name);
    if (ok)
        printf("SWAP_PASS\n");
    return ok;
}

int main(void) {
    int all = 1;
    all &= check_component();
    all &= check_compose_uni();
    all &= check_compose_mono();
    all &= check_swap();
    if (all) {
        printf("ALL_PASS\n");
        return 0;
    }
    return 1;
}
