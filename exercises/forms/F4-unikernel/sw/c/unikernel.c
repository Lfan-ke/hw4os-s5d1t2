/* 形态 F4 · 库OS / Unikernel —— C（host 软件直觉 demo，不是真内核）。
 *
 * 本质权衡，用最朴素的软件模型演出来：
 *   1. OS 例程就是被 app 直接链接的库函数 —— uni_write / uni_alloc / uni_clock。
 *   2. app→OS 是直接函数调用，替代「陷入」(trap)：传统模型每次系统服务 traps+1，
 *      unikernel 模型直接 call，陷入计数恒为 0。
 *   3. 单应用 ⇒ 编译期特化裁剪：app 不用的模块(net/blk/fs)整段裁掉，镜像变小。
 *   4. capstone：一镜像 = app + OS，同地址空间、零陷入、只含用到的模块。
 *
 * 你只需填 6 个函数体（标 TODO 处）；下方测试 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

/* ── 镜像模块表：每个子系统是一个可单独选/不选的「微库」(仿 unikraft 88 lib) ── */
#define MOD_CONSOLE (1u << 0)
#define MOD_ALLOC   (1u << 1)
#define MOD_CLOCK   (1u << 2)
#define MOD_NET     (1u << 3)
#define MOD_BLK     (1u << 4)
#define MOD_FS      (1u << 5)

/* 全量「通用内核」：链入所有模块（像传统 OS 什么都带着）。 */
#define ALL_MODS (MOD_CONSOLE | MOD_ALLOC | MOD_CLOCK | MOD_NET | MOD_BLK | MOD_FS)
/* 本 app 实际用到的：只有 console / alloc / clock —— 特化镜像就只链这三个。 */
#define APP_USES (MOD_CONSOLE | MOD_ALLOC | MOD_CLOCK)

/* ── 系统服务号（传统模型里是 syscall 号，unikernel 里只是 switch 分支）── */
#define SVC_WRITE 0u
#define SVC_ALLOC 1u
#define SVC_CLOCK 2u

struct module {
    const char *name;
    uint32_t bit;
    uint32_t size; /* 模块的「符号数 / 代码量」——链入它镜像就变大这么多 */
};

static const struct module MODULES[6] = {
    {"console", MOD_CONSOLE, 10},
    {"alloc",   MOD_ALLOC,   14},
    {"clock",   MOD_CLOCK,   6},
    {"net",     MOD_NET,     40},
    {"blk",     MOD_BLK,     32},
    {"fs",      MOD_FS,      50},
};

/* 「被链接进 app 的那点 OS 状态」——和 app 同处一个地址空间。 */
struct unikernel {
    size_t console_len; /* 写进控制台的字节数 */
    size_t heap_top;    /* bump 分配器游标 */
    uint64_t ticks;     /* 单调时钟 */
};

static void k_init(struct unikernel *k) {
    k->console_len = 0;
    k->heap_top = 0;
    k->ticks = 0;
}

/* ════════════════════════════════════════════════════════════════
 * 学生填空区：3 个 OS 例程 + 1 个直接绑定 + 2 个特化开关
 * ════════════════════════════════════════════════════════════════ */

/* ── OS 例程：普通库函数，app 直接 call（同地址空间，无陷入）── */

/* console 写：把 n 字节追加进控制台，返回写入字节数。 */
static size_t uni_write(struct unikernel *k, size_t n) {
    /* TODO: k->console_len += n; 返回 n。 */
    (void)k;
    (void)n;
    return 0; /* 占位 */
}

/* bump 分配：返回分配前的 heap_top，再把游标前移 n（连续分配区间不重叠）。 */
static size_t uni_alloc(struct unikernel *k, size_t n) {
    /* TODO: size_t off = k->heap_top; k->heap_top += n; 返回 off。 */
    (void)k;
    (void)n;
    return 0; /* 占位 */
}

/* 单调时钟：返回当前 tick，再自增。 */
static uint64_t uni_clock(struct unikernel *k) {
    /* TODO: uint64_t t = k->ticks; k->ticks += 1; 返回 t。 */
    (void)k;
    return 0; /* 占位 */
}

/* ── 直接绑定：app→OS 用直接函数调用替代 syscall 陷入 ── */

/* 直接派发：按 svc 直接调对应的 uni_* 例程——绝不碰陷入计数器。
 * 对比 harness 的 dispatch_trap：内核代码相同，差别只在那一次 *traps += 1。 */
static uint64_t dispatch_direct(struct unikernel *k, uint32_t svc, uint32_t arg) {
    /* TODO: switch(svc){ case SVC_WRITE: return uni_write(k,(size_t)arg);
     *                    case SVC_ALLOC: return uni_alloc(k,(size_t)arg);
     *                    case SVC_CLOCK: return uni_clock(k); default: return 0; } */
    (void)k;
    (void)svc;
    (void)arg;
    return 0; /* 占位 */
}

/* ── 特化开关：单应用 ⇒ 编译期把没用到的模块裁掉 ── */

/* 某模块是否链入镜像：app 用到了才链入（仿 Kconfig/feature 的编译期开关）。 */
static int is_linked(uint32_t used, uint32_t module_bit) {
    /* TODO: return (used & module_bit) != 0; */
    (void)used;
    (void)module_bit;
    return 0; /* 占位 */
}

/* 镜像符号数：把所有「被链入」模块的 size 累加——没链入的不计入，镜像就变小。 */
static uint32_t image_symbols(uint32_t used) {
    /* TODO: 遍历 MODULES，is_linked(used, MODULES[i].bit) 才把 size 累加进 total。 */
    (void)used;
    return 0; /* 占位 */
}

/* ════════════════════════════════════════════════════════════════
 * 测试 harness（给定，勿改）
 * ════════════════════════════════════════════════════════════════ */

/* 传统模型的派发：每次系统服务都要 user→kernel 陷入(模式切换 / ecall)，故 *traps += 1。
 * 它调用的是同一批 uni_* 例程——唯一差别就是这道陷入墙。 */
static uint64_t dispatch_trap(struct unikernel *k, uint32_t svc, uint32_t arg, uint64_t *traps) {
    *traps += 1; /* ← 陷入开销：unikernel 的 dispatch_direct 没有这一行 */
    switch (svc) {
        case SVC_WRITE: return (uint64_t)uni_write(k, (size_t)arg);
        case SVC_ALLOC: return (uint64_t)uni_alloc(k, (size_t)arg);
        case SVC_CLOCK: return uni_clock(k);
        default: return 0;
    }
}

static int check_uni(void) {
    int ok = 1;
    struct unikernel k;
    k_init(&k);

    /* (a) console 写。 */
    size_t w1 = uni_write(&k, 5);
    size_t w2 = uni_write(&k, 3);
    if (w1 != 5 || w2 != 3 || k.console_len != 8) {
        printf("UNI_FAIL 直接写控制台 w1=%zu w2=%zu len=%zu 应=(5,3,8)\n", w1, w2, k.console_len);
        ok = 0;
    }
    /* (b) bump 分配。 */
    size_t a1 = uni_alloc(&k, 16);
    size_t a2 = uni_alloc(&k, 8);
    if (a1 != 0 || a2 != 16 || k.heap_top != 24) {
        printf("UNI_FAIL bump 分配 a1=%zu a2=%zu top=%zu 应=(0,16,24)\n", a1, a2, k.heap_top);
        ok = 0;
    }
    /* (c) 单调时钟。 */
    uint64_t c0 = uni_clock(&k);
    uint64_t c1 = uni_clock(&k);
    uint64_t c2 = uni_clock(&k);
    if (c0 != 0 || c1 != 1 || c2 != 2) {
        printf("UNI_FAIL 单调时钟 = %llu,%llu,%llu 应=0,1,2\n",
               (unsigned long long)c0, (unsigned long long)c1, (unsigned long long)c2);
        ok = 0;
    }

    if (ok)
        printf("UNI_PASS\n");
    return ok;
}

static int check_direct(void) {
    int ok = 1;
    /* 同一份工作负载：(服务, 参数)。 */
    uint32_t wsvc[5] = {SVC_WRITE, SVC_ALLOC, SVC_CLOCK, SVC_WRITE, SVC_CLOCK};
    uint32_t warg[5] = {4, 16, 0, 2, 0};
    int i;

    /* 传统模型：每次系统服务一次陷入。 */
    struct unikernel kt;
    k_init(&kt);
    uint64_t traps_trad = 0;
    uint64_t res_trad[5];
    for (i = 0; i < 5; i++)
        res_trad[i] = dispatch_trap(&kt, wsvc[i], warg[i], &traps_trad);

    /* unikernel：直接函数调用，陷入计数从不自增。 */
    struct unikernel ku;
    k_init(&ku);
    uint64_t traps_uni = 0; /* dispatch_direct 不接触陷入计数器 */
    uint64_t res_uni[5];
    for (i = 0; i < 5; i++)
        res_uni[i] = dispatch_direct(&ku, wsvc[i], warg[i]);

    int same = 1;
    for (i = 0; i < 5; i++)
        if (res_trad[i] != res_uni[i])
            same = 0;
    if (!same) {
        printf("DIRECT_FAIL 业务结果不一致(陷入与否不应改变干的活)\n");
        ok = 0;
    }
    if (traps_trad != 5) {
        printf("DIRECT_FAIL 传统模型陷入数=%llu 应=5\n", (unsigned long long)traps_trad);
        ok = 0;
    }
    if (traps_uni != 0) {
        printf("TRAP_LEAK_FAIL unikernel 直接调用却产生了 %llu 次陷入\n",
               (unsigned long long)traps_uni);
        ok = 0;
    }

    printf("TRAP_COST 传统模型陷入=%llu unikernel 陷入=%llu\n",
           (unsigned long long)traps_trad, (unsigned long long)traps_uni);
    if (ok)
        printf("DIRECT_PASS\n");
    return ok;
}

static int check_specialize(void) {
    int ok = 1;
    int i;

    /* (a) 单应用 ⇒ 特化：只链 app 用到的模块，镜像应比全量小。 */
    uint32_t full = image_symbols(ALL_MODS);
    uint32_t spec = image_symbols(APP_USES);
    if (spec >= full) {
        printf("SPECIALIZE_BLOAT_FAIL 特化镜像符号数=%u 未小于全量=%u\n", spec, full);
        ok = 0;
    }

    /* (b) 用到的模块必须在镜像里；没用到的必须被裁掉。 */
    for (i = 0; i < 6; i++) {
        int linked = is_linked(APP_USES, MODULES[i].bit);
        int used = (APP_USES & MODULES[i].bit) != 0;
        if (linked != used) {
            printf("SPECIALIZE_FAIL 模块 %s linked=%d 应=%d\n", MODULES[i].name, linked, used);
            ok = 0;
        }
    }

    /* (c) 特化后仍能服务 app 真正用到的调用。 */
    struct unikernel k;
    k_init(&k);
    if (dispatch_direct(&k, SVC_WRITE, 3) != 3) {
        printf("SPECIALIZE_FAIL 特化镜像无法提供 console 服务\n");
        ok = 0;
    }

    printf("IMAGE_SIZE 全量符号=%u 特化符号=%u\n", full, spec);
    if (ok)
        printf("SPECIALIZE_PASS\n");
    return ok;
}

static int check_image(void) {
    int ok = 1;

    /* 单镜像 = app + OS 同地址空间：构造 APP_USES 特化镜像并「启动」app。 */
    uint32_t img = APP_USES;
    struct unikernel k;
    k_init(&k);
    uint64_t traps = 0; /* 整个 app 生命周期内的陷入计数——全程直接调用，恒 0 */

    /* app main：全程经 dispatch_direct 直接调 OS。 */
    uint64_t banner = dispatch_direct(&k, SVC_WRITE, 6); /* 写 6 字节 banner */
    uint64_t p0 = dispatch_direct(&k, SVC_ALLOC, 32);
    uint64_t p1 = dispatch_direct(&k, SVC_ALLOC, 16);
    uint64_t t0 = dispatch_direct(&k, SVC_CLOCK, 0);
    uint64_t t1 = dispatch_direct(&k, SVC_CLOCK, 0);

    if (banner != 6 || k.console_len != 6) {
        printf("IMAGE_FAIL banner 写入异常 ret=%llu len=%zu 应=(6,6)\n",
               (unsigned long long)banner, k.console_len);
        ok = 0;
    }
    if (p0 != 0 || p1 != 32) {
        printf("IMAGE_FAIL 分配区重叠 p0=%llu p1=%llu 应=0,32\n",
               (unsigned long long)p0, (unsigned long long)p1);
        ok = 0;
    }
    if (t1 <= t0) {
        printf("IMAGE_FAIL 时钟非单调 t0=%llu t1=%llu\n",
               (unsigned long long)t0, (unsigned long long)t1);
        ok = 0;
    }
    if (traps != 0) {
        printf("TRAP_LEAK_FAIL 镜像运行期出现 %llu 次陷入\n", (unsigned long long)traps);
        ok = 0;
    }
    /* 特化镜像里不该混入未用模块。 */
    if (is_linked(img, MOD_NET) || is_linked(img, MOD_BLK) || is_linked(img, MOD_FS)) {
        printf("IMAGE_FAIL 特化镜像混入了未用模块(net/blk/fs)\n");
        ok = 0;
    }

    if (ok)
        printf("IMAGE_PASS\n");
    return ok;
}

int main(void) {
    int all = 1;
    all &= check_uni();
    all &= check_direct();
    all &= check_specialize();
    all &= check_image();
    if (all) {
        printf("ALL_PASS\n");
        return 0;
    }
    return 1;
}
