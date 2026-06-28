# 两实验落地设计 + 作者实现规范：链接（improper/03b · proper/S09b）

> 范围：新增两课。**A = improper/03b-dynlink-sim**（纯软件建模动态库加载，host 直接跑，
> C/Rust 双语言 + essay）；**B = proper/S09b-linking**（真工具链：真 gcc/ar/ld/readelf/objdump/dlopen
> + riscv64 PLT + qemu-user，Makefile 跑命令 echo `*_PASS`）。
> 本文是作者实现规范：含全量 meta.toml / view.toml / 参考解 / 骨架策略 / README / THINKING /
> labctl 最小改法 / info.toml 注册 / 实现顺序与验证。
>
> 仓库约定（已核对）：每课在 `exercises/<rel>/`（骨架，带 `// TODO` 占位，**按预期判 FAIL**）
> 与 `ans/<rel>/`（参考解，**全过**）各放一份；`labctl run <rel>` 跑 `exercises/`，
> `labctl run <rel> --solutions` 跑 `ans/`，`labctl verify --solutions` 全量自测。
> 判题 = 构建+运行输出里 `expect[]` 子串**全中** 且 `forbid[]` **全不中**（`judge.rs`）。
> essay 特例：答案文件非空且不含 `LABCTL_ESSAY_TODO` 即过。

---

## 关键决策摘要

| 议题 | 决策 |
| :-- | :-- |
| 03b 母题落点 | 公共库一份 `{name→(addr,size)}`（C 线性查 / Rust `HashMap` 按 key 查）；**静态=每程序各塞一份拷贝(N×Σsize)**，**动态=只存名字引用+私有 GOT(Σsize+N×nn×8)**，E4 实测 ~22.6x 差 |
| 03b 子题 | E1 静态拷贝 / E2 名字引用+私有 GOT / E3 ld.so 按 key 解析回填 / E4 共享一份+空间对比 / E5 lazy vs now / E6 dlopen+缺符号失败 |
| S09b build 类型 | 现有 7 种均不适用（`qemu-virt` 是 S 态内核，跑不了 gcc/readelf/dlopen）。**新增 `build="make-host"`**：跑变体目录 `make -s test`，Makefile echo `*_PASS`，沿用 grep 判题。labctl 改动 ~18 行 |
| S09b 的 track/env | `track="proper"`（“真·工具链”属正经赛道的“真”哲学，是 03b 软件模型的真实对照）。`env="host"`：`env` 字段是 `dead_code`（`manifest.rs` L40-43，不参与 build 选择），故 **proper 可以 env=host**，这是有意例外；真正驱动执行的是 `build="make-host"`。E4 的 RV 部分走 qemu-user（非 qemu-virt） |
| view.toml | 两课都做“**静态各塞一份(大) vs 动态一份共享+各自 GOT(小)**”对比图，用满 `node/edge/flow/iface`；`wave` 不适用（无信号，与 03-compile-link 一致省略）。labctl 无字段级配色，对比靠**节点 label 措辞 + 两条对照边链 + 逐题 flow 场景 + iface 结构文档**，颜色由 labctl 自身在变体状态层渲染（✓✗⊘! + GRN/RED/YEL/DIM） |

---

# A) improper/03b-dynlink-sim

## A.1 `meta.toml`（exercises 与 ans 同一份）

```toml
id      = "03b-dynlink-sim"
title   = "动态链接模拟 · 名字引用+私有GOT / ld.so按key解析 / lazy-now / dlopen"
track   = "improper"
require = 1               # C/Rust 任一过即过；essay 独立计辅助分
env     = "host"          # 纯软件建模 ld.so，host 直接跑
weight  = 1

[[variant]]
id    = "sw-rust"
axis  = "software"
lang  = "rust"
dir   = "sw/rust"
build = "cargo"

[[variant]]
id    = "sw-c"
axis  = "software"
lang  = "c"
dir   = "sw/c"
build = "gcc-host"

[[variant]]
id    = "essay"
axis  = "software"
lang  = "essay"
dir   = "essay"
build = "essay"

[judge]
expect    = ["STATIC_PASS", "DYNREF_PASS", "RESOLVE_PASS", "SHARE_PASS", "LAZY_PASS", "DLOPEN_PASS", "ALL_PASS"]
forbid    = ["FAIL", "panic", "ERROR"]
timeout_s = 30

[[hint]]
text = "E1 静态：每个程序把用到的库函数字节『各塞一份拷贝』——返回 Σsize（用 sum_size 求和）。这正是 -static 体积膨胀的根。"
[[hint]]
text = "E2 动态：不拷代码，只存『名字引用』+一张私有 GOT，每槽 8 字节、初值 UNRESOLVED(0)；返回 nn*PTR。"
[[hint]]
text = "E3 ld.so：对每个 needed 名字在公共库里『按 key 查表』(C 用 lib_lookup/strcmp，Rust 用 HashMap::get)，把真实 addr 回填私有 GOT；缺符号必须解析失败。"
[[hint]]
text = "E4 对比：静态=nprog*Σsize；动态=Σsize(一份共享)+nprog*nn*8(各自 GOT)。E5 lazy=首调用才解析+回填、二调缓存命中不再解析、未用到的不解析；now=装载时一次性全解析。E6 dlopen 缺符号 RTLD_NOW 立即失败。"
```

## A.2 `view.toml`（对比图：静态大 / 动态小）

```toml
# 03b-dynlink-sim 可视化：一张「省空间对比图」——
# 静态各塞一份(大) vs 动态一份共享+各自私有GOT(小)，及 ld.so 按 key 解析回填 GOT。

[[node]]
id = "progs"
label = "N 个程序（每个只写：用到 puts/printf/malloc 的『名字引用』+ 私有 GOT）"
[[node]]
id = "static"
label = "静态镜像：每程序各塞一份库代码拷贝 → N×Σsize（大·冗余）"
[[node]]
id = "lib"
label = "公共库：全局仅一份 {name→(addr,size)}（HashMap/列表按 key 查）"
[[node]]
id = "ldso"
label = "ld.so：按 key 解析 needed[] → 把真实 addr 回填各程序私有 GOT"
[[node]]
id = "got"
label = "私有 GOT：每程序 nn×8 字节小表（存解析后地址；lazy 首调回填）"
[[node]]
id = "dyn"
label = "动态总占用：一份共享库 Σsize + N 张小 GOT（小·省空间）"

[[edge]]
from = "progs"
to   = "static"
[[edge]]
from = "progs"
to   = "ldso"
[[edge]]
from = "lib"
to   = "ldso"
[[edge]]
from = "ldso"
to   = "got"
[[edge]]
from = "got"
to   = "dyn"

[[flow]]
name = "static-copy"
path = ["progs", "static"]
note = "E1：每程序把用到的库函数字节各塞一份 → STATIC_PASS"
[[flow]]
name = "dyn-ref"
path = ["progs", "got"]
note = "E2：只存名字引用+私有 GOT(初值 UNRESOLVED)，不拷代码 → DYNREF_PASS"
[[flow]]
name = "ldso-resolve"
path = ["lib", "ldso", "got"]
note = "E3：ld.so 按 key 查公共库，把 addr 回填私有 GOT → RESOLVE_PASS"
[[flow]]
name = "share-vs-copy"
path = ["progs", "lib", "dyn"]
note = "E4：一份共享+各自 GOT，对比静态 N× → SHARE_PASS（实测 ~22.6x）"
[[flow]]
name = "lazy-now"
path = ["got", "ldso"]
note = "E5：lazy 首调用才解析回填、二调直达；now 装载时全解析 → LAZY_PASS"
[[flow]]
name = "dlopen-miss"
path = ["ldso", "lib"]
note = "E6：dlopen 缺符号 RTLD_NOW 立即失败 → DLOPEN_PASS"

[[iface]]
name = "GOT_SLOT"
bits = "8 B"
note = "私有 GOT 每槽=指针宽；动态每符号只花 8 字节，不拷代码"
[[iface]]
name = "LibSym"
bits = "{name,addr,size}"
note = "公共库表项；按 name(key) 查 → 全程序共享同一份代码"
[[iface]]
name = "UNRESOLVED"
bits = "0x0"
note = "GOT 槽未解析初值；lazy 首次调用经 ld.so 回填真实 addr"
[[iface]]
name = "ratio"
bits = "static/dyn >10x"
note = "N=64 实测 ~22.6x：静态 N×Σsize vs 动态 Σsize+N×nn×8"
```

## A.3 参考解 `ans/improper/03b-dynlink-sim/sw/c/dynlink.c`（全量）

```c
/* 动态链接模拟（软件建模）—— C 参考解。
 * 母题：静态链接 = 每程序各塞一份库代码拷贝（费空间）；
 *   动态链接 = 程序只存符号『名字引用』+ 私有 GOT，公共库全局仅一份，
 *   ld.so 按 key(名字) 查表把真实地址回填到各程序私有 GOT（省空间）。
 * 逐题递进：E1 静态各塞一份 → E2 动态名字引用+私有GOT → E3 ld.so按key解析回填
 *   → E4 一份共享+空间对比 → E5 lazy vs now → E6 dlopen/dlsym+缺符号失败。
 * 学生只填带 // TODO 的函数体；下方测试 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define MAXSYM      8
#define PTR         8           /* 私有 GOT 每槽宽 = 指针 8 字节 */
#define UNRESOLVED  0           /* GOT 槽未解析初值（lazy 经 ld.so 回填） */

typedef unsigned long long ull;

/* 公共库：一张 {名字 -> (装载地址, 字节数)} 表，全局仅一份（给定，勿改）。 */
typedef struct { const char *name; uint64_t addr; uint32_t size; } LibSym;
static const LibSym LIB[] = {
    { "puts",   0x1000, 200 },
    { "printf", 0x2000, 360 },
    { "malloc", 0x3000, 280 },
    { "answer", 0x4000,  16 },
};
#define NLIB ((int)(sizeof(LIB) / sizeof(LIB[0])))

/* 给定 helper：按 name(key) 在公共库线性查表，返回下标，找不到 -1。 */
static int lib_lookup(const char *name) {
    for (int i = 0; i < NLIB; i++) if (strcmp(LIB[i].name, name) == 0) return i;
    return -1;
}
/* 给定 helper：把若干 name 对应库函数的 size 求和（供 E1/E4 用）。 */
static uint32_t sum_size(const char *const *names, int nn) {
    uint32_t s = 0;
    for (int i = 0; i < nn; i++) { int k = lib_lookup(names[i]); if (k >= 0) s += LIB[k].size; }
    return s;
}

/* ════════════════ 学生填空区（六段核心逻辑）════════════════ */

/* E1：静态链接——把每个被用到的库函数字节『拷进』本程序镜像。
 *     返回本镜像里库代码占的字节数（= Σsize；每程序各塞一份）。 */
static uint64_t static_link(const char *const *needs, int nn) {
    /* TODO: 累加各被用函数 size。HINT: return sum_size(needs, nn);  骨架占位 return 0; */
    return sum_size(needs, nn);
}

/* E2：动态链接——只存『名字引用』+一张私有 GOT（每槽 PTR 字节、初值 UNRESOLVED），
 *     不拷库代码。把 got[0..nn) 置 UNRESOLVED，返回私有 GOT 字节数。 */
static uint64_t dyn_link(int nn, uint64_t got[MAXSYM]) {
    /* TODO: got[i]=UNRESOLVED; return (uint64_t)nn*PTR;  骨架占位 return 0;（got 仍清零） */
    for (int i = 0; i < nn; i++) got[i] = UNRESOLVED;
    return (uint64_t)nn * PTR;
}

/* E3：ld.so 符号解析——按 name 在公共库 LIB 查表(key 查找)，把真实地址回填私有 GOT。
 *     全部命中返回 1；任一缺失返回 0。 */
static int resolve(const char *const *needs, int nn, uint64_t got[MAXSYM]) {
    /* TODO: k=lib_lookup(needs[i]); if(k<0) return 0; got[i]=LIB[k].addr;  骨架占位 return 0; */
    for (int i = 0; i < nn; i++) {
        int k = lib_lookup(needs[i]);
        if (k < 0) return 0;
        got[i] = LIB[k].addr;
    }
    return 1;
}

/* E4：空间对比。静态：每程序各塞一份 → nprog*Σsize。 */
static uint64_t static_footprint(int nprog, const char *const *needs, int nn) {
    /* TODO: return (uint64_t)nprog * sum_size(needs, nn);  骨架占位 return 0; */
    return (uint64_t)nprog * sum_size(needs, nn);
}
/* E4：动态：库代码一份共享(Σsize) + 每程序一张私有 GOT(nn*PTR)。 */
static uint64_t dyn_footprint(int nprog, const char *const *needs, int nn) {
    /* TODO: return sum_size(needs,nn) + (uint64_t)nprog*nn*PTR;  骨架占位 return 0; */
    return (uint64_t)sum_size(needs, nn) + (uint64_t)nprog * nn * PTR;
}

/* E5：lazy 调用——首次调用某 GOT 槽才解析+回填+计一次 ld.so 解析；二次直达不再解析。
 *     返回该槽目标地址；按需 (*resolver_calls)++。 */
static uint64_t call_lazy(uint64_t got[MAXSYM], int idx, const char *const *needs,
                          int *resolver_calls) {
    /* TODO: if(got[idx]==UNRESOLVED){got[idx]=LIB[lib_lookup(needs[idx])].addr; (*resolver_calls)++;}
       return got[idx];  骨架占位 return got[idx];（不解析/不计数） */
    if (got[idx] == UNRESOLVED) {
        int k = lib_lookup(needs[idx]);
        got[idx] = LIB[k].addr;
        (*resolver_calls)++;
    }
    return got[idx];
}

/* E6：dlopen——插件需要的每个 extern 符号都能在 LIB 解析才成功(RTLD_NOW)；任一缺失返回 0。 */
static int dlopen_sim(const char *const *needs, int nn) {
    /* TODO: for i: if(lib_lookup(needs[i])<0) return 0;  return 1;  骨架占位 return 0; */
    for (int i = 0; i < nn; i++) if (lib_lookup(needs[i]) < 0) return 0;
    return 1;
}
/* E6：dlsym——在插件导出表按 name 找；命中回填 *out 返回 1，找不到返回 0(NULL)。 */
static int dlsym_sim(const char *const *exp_names, const uint64_t *exp_val, int ne,
                     const char *name, uint64_t *out) {
    /* TODO: 线性查 exp_names[i]==name；命中 *out=exp_val[i] return 1; 否则 return 0;  骨架占位 return 0; */
    for (int i = 0; i < ne; i++) if (strcmp(exp_names[i], name) == 0) { *out = exp_val[i]; return 1; }
    return 0;
}

/* ════════════════ 测试 harness（勿改）════════════════ */

static int check_static(void) {
    const char *needs[] = { "puts", "printf" };
    ull bytes = static_link(needs, 2), want = 200 + 360;
    int ok = 1;
    if (bytes != want) { printf("STATIC_FAIL 静态镜像库代码=%llu 应=%llu（每程序各塞一份拷贝）\n", bytes, want); ok = 0; }
    if (ok) printf("STATIC_PASS\n");
    return ok;
}
static int check_dynref(void) {
    uint64_t got[MAXSYM];
    for (int i = 0; i < MAXSYM; i++) got[i] = 0xDEAD;
    ull gb = dyn_link(2, got);
    int ok = 1;
    if (gb != 2 * PTR) { printf("DYNREF_FAIL 私有 GOT 字节=%llu 应=%d（只存名字引用+GOT，不拷代码）\n", gb, 2 * PTR); ok = 0; }
    for (int i = 0; i < 2; i++) if (got[i] != UNRESOLVED) { printf("DYNREF_FAIL got[%d]=%#llx 应=0(UNRESOLVED)\n", i, (ull)got[i]); ok = 0; }
    if (ok) printf("DYNREF_PASS\n");
    return ok;
}
static int check_resolve(void) {
    const char *needs[] = { "printf", "puts" };
    uint64_t got[MAXSYM] = {0};
    int r = resolve(needs, 2, got), ok = 1;
    if (!r) { printf("RESOLVE_FAIL needs 都在公共库里，解析应成功\n"); ok = 0; }
    if (got[0] != 0x2000 || got[1] != 0x1000) { printf("RESOLVE_FAIL GOT 回填错 got=[%#llx,%#llx] 应=[0x2000,0x1000]\n", (ull)got[0], (ull)got[1]); ok = 0; }
    const char *bad[] = { "no_such" };
    uint64_t g2[MAXSYM] = {0};
    if (resolve(bad, 1, g2)) { printf("RESOLVE_FAIL 缺符号竟解析成功（ld.so 应报未定义）\n"); ok = 0; }
    if (ok) printf("RESOLVE_PASS\n");
    return ok;
}
static int check_share(void) {
    const char *needs[] = { "puts", "printf", "malloc" };
    int nprog = 64;
    ull st = static_footprint(nprog, needs, 3), dy = dyn_footprint(nprog, needs, 3);
    ull sz = 200 + 360 + 280; /* 840 */
    int ok = 1;
    if (st != (ull)nprog * sz) { printf("SHARE_FAIL 静态总占=%llu 应=%llu\n", st, (ull)nprog * sz); ok = 0; }
    if (dy != sz + (ull)nprog * 3 * PTR) { printf("SHARE_FAIL 动态总占=%llu 应=%llu（一份共享+各自GOT）\n", dy, sz + (ull)nprog * 3 * PTR); ok = 0; }
    if (!(st > 10 * dy)) { printf("SHARE_FAIL 动态未显著省空间 static=%llu dyn=%llu\n", st, dy); ok = 0; }
    if (ok) { printf("SHARE static=%llu dyn=%llu (%.1fx)\n", st, dy, (double)st / (double)dy); printf("SHARE_PASS\n"); }
    return ok;
}
static int check_lazy(void) {
    const char *needs[] = { "puts", "printf", "malloc" };
    int ok = 1;
    { /* now：装载时全解析，调用阶段不再触发 resolver */
        uint64_t got[MAXSYM] = {0}; int rc = 0;
        resolve(needs, 3, got); rc = 3;
        int before = rc;
        for (int i = 0; i < 3; i++) { call_lazy(got, i, needs, &rc); call_lazy(got, i, needs, &rc); }
        if (rc != before) { printf("LAZY_FAIL now 模式调用阶段不该再解析 rc=%d 应=%d\n", rc, before); ok = 0; }
    }
    { /* lazy：首调用才解析、缓存命中不再、未用到的不解析 */
        uint64_t got[MAXSYM] = {0}; int rc = 0;
        dyn_link(3, got);
        call_lazy(got, 0, needs, &rc);   /* 解析 puts → 1 */
        call_lazy(got, 0, needs, &rc);   /* 缓存命中 → 1 */
        call_lazy(got, 1, needs, &rc);   /* 解析 printf → 2 */
        if (rc != 2) { printf("LAZY_FAIL lazy 解析次数=%d 应=2（每符号首调一次、缓存后不再、未用到的不解析）\n", rc); ok = 0; }
        if (got[2] != UNRESOLVED) { printf("LAZY_FAIL 未调用的 malloc 不应被解析\n"); ok = 0; }
    }
    if (ok) printf("LAZY_PASS\n");
    return ok;
}
static int check_dlopen(void) {
    int ok = 1;
    const char *gneed[] = { "puts" };
    const char *gexp[]  = { "answer" };
    uint64_t gval[]     = { 42 };
    if (!dlopen_sim(gneed, 1)) { printf("DLOPEN_FAIL 好插件应能 dlopen\n"); ok = 0; }
    uint64_t v = 0;
    if (!dlsym_sim(gexp, gval, 1, "answer", &v) || v != 42) { printf("DLOPEN_FAIL dlsym(answer) 应=42 实=%llu\n", (ull)v); ok = 0; }
    else printf("answer=%llu\n", (ull)v);
    uint64_t v2 = 0xDEAD;
    if (dlsym_sim(gexp, gval, 1, "nonexistent", &v2)) { printf("DLOPEN_FAIL dlsym 找不到符号应返回 NULL\n"); ok = 0; }
    else printf("dlsym(nonexistent)=NULL\n");
    const char *bneed[] = { "missing_extern" };
    if (dlopen_sim(bneed, 1)) { printf("DLOPEN_FAIL 坏插件应 dlopen 失败\n"); ok = 0; }
    else printf("dlopen rejected bad_plugin: undefined symbol missing_extern\n");
    if (ok) printf("DLOPEN_PASS\n");
    return ok;
}

int main(void) {
    int all = 1;
    all &= check_static();
    all &= check_dynref();
    all &= check_resolve();
    all &= check_share();
    all &= check_lazy();
    all &= check_dlopen();
    if (all) { printf("ALL_PASS\n"); return 0; }
    return 1;
}
```

**实测**（参考解）：`SHARE static=53760 dyn=2376 (22.6x)`，全部 `*_PASS` + `ALL_PASS`。

## A.4 参考解 `ans/improper/03b-dynlink-sim/sw/rust/src/main.rs`（全量）

`Cargo.toml`：

```toml
[package]
name = "dynlink-sw-rust"
version = "0.1.0"
edition = "2021"

[[bin]]
name = "dynlink"
path = "src/main.rs"
```

`src/main.rs`：

```rust
//! 动态链接模拟（软件建模）—— Rust 参考解。
//! 母题：静态=每程序各塞一份库代码拷贝（费空间）；动态=只存名字引用+私有 GOT，
//!   公共库全局仅一份(HashMap 按 key 查)，ld.so 解析回填各程序私有 GOT（省空间）。
//! 逐题：E1 静态拷贝 → E2 名字引用+私有GOT → E3 ld.so按key解析回填 →
//!   E4 一份共享+空间对比 → E5 lazy vs now → E6 dlopen/dlsym+缺符号失败。
//! 学生只填带 // TODO 的函数体；下方测试 harness 勿改。
#![allow(unused_variables, dead_code)]
use std::collections::HashMap;

const PTR: u64 = 8;            // 私有 GOT 每槽宽
const UNRESOLVED: u64 = 0;     // GOT 槽未解析初值

#[derive(Clone, Copy)]
struct LibSym { addr: u64, size: u32 }

/// 公共库：HashMap 按 name(key) 查，全局仅一份（给定，勿改）。
fn shared_lib() -> HashMap<&'static str, LibSym> {
    HashMap::from([
        ("puts",   LibSym { addr: 0x1000, size: 200 }),
        ("printf", LibSym { addr: 0x2000, size: 360 }),
        ("malloc", LibSym { addr: 0x3000, size: 280 }),
        ("answer", LibSym { addr: 0x4000, size: 16  }),
    ])
}
fn sum_size(lib: &HashMap<&str, LibSym>, needs: &[&str]) -> u64 {
    needs.iter().filter_map(|n| lib.get(n)).map(|s| s.size as u64).sum()
}

// ═════════════════════ 学生填空区（六段核心逻辑）═════════════════════

/// E1：静态——每程序各塞一份库代码拷贝；返回本镜像库代码字节数(=Σsize)。
fn static_link(lib: &HashMap<&str, LibSym>, needs: &[&str]) -> u64 {
    // TODO: sum_size(lib, needs)。骨架占位 0。
    sum_size(lib, needs)
}

/// E2：动态——只存名字引用+私有 GOT(每槽 PTR、初值 UNRESOLVED)；返回 GOT 字节数。
fn dyn_link(n: usize, got: &mut Vec<u64>) -> u64 {
    // TODO: *got = vec![UNRESOLVED; n]; n as u64 * PTR。骨架占位：*got=vec![UNRESOLVED;n]; 0。
    *got = vec![UNRESOLVED; n];
    n as u64 * PTR
}

/// E3：ld.so 解析——按 key 在公共库查表，回填私有 GOT；全中 Some(())，缺符号 None。
fn resolve(lib: &HashMap<&str, LibSym>, needs: &[&str], got: &mut [u64]) -> Option<()> {
    // TODO: for i: got[i] = lib.get(needs[i])?.addr; Some(())。骨架占位 None。
    for (i, n) in needs.iter().enumerate() {
        got[i] = lib.get(n)?.addr;
    }
    Some(())
}

/// E4：静态总占 = nprog * Σsize。
fn static_footprint(lib: &HashMap<&str, LibSym>, nprog: u64, needs: &[&str]) -> u64 {
    // TODO: nprog * sum_size(lib, needs)。骨架占位 0。
    nprog * sum_size(lib, needs)
}
/// E4：动态总占 = 一份共享 Σsize + nprog * nn * PTR。
fn dyn_footprint(lib: &HashMap<&str, LibSym>, nprog: u64, needs: &[&str]) -> u64 {
    // TODO: sum_size(lib,needs) + nprog*needs.len() as u64*PTR。骨架占位 0。
    sum_size(lib, needs) + nprog * needs.len() as u64 * PTR
}

/// E5：lazy 调用——首调解析+回填+计一次解析；二调直达不再解析。返回目标地址。
fn call_lazy(lib: &HashMap<&str, LibSym>, got: &mut [u64], idx: usize, needs: &[&str],
             resolver_calls: &mut u32) -> u64 {
    // TODO: if got[idx]==UNRESOLVED { got[idx]=lib[needs[idx]].addr; *resolver_calls+=1; } got[idx]。
    //       骨架占位：got[idx]（不解析/不计数）。
    if got[idx] == UNRESOLVED {
        got[idx] = lib.get(needs[idx]).unwrap().addr;
        *resolver_calls += 1;
    }
    got[idx]
}

/// E6：dlopen——每个 extern 都能解析才 Some(())，缺符号 None(RTLD_NOW 失败)。
fn dlopen_sim(lib: &HashMap<&str, LibSym>, needs: &[&str]) -> Option<()> {
    // TODO: needs.iter().all(|n| lib.contains_key(n)).then_some(())。骨架占位 None。
    needs.iter().all(|n| lib.contains_key(n)).then_some(())
}
/// E6：dlsym——在导出表按 name 找；命中 Some(val)，找不到 None。
fn dlsym_sim(exports: &HashMap<&str, u64>, name: &str) -> Option<u64> {
    // TODO: exports.get(name).copied()。骨架占位 None。
    exports.get(name).copied()
}

// ═════════════════════ 测试 harness（勿改）═════════════════════

fn check_static() -> bool {
    let lib = shared_lib();
    let bytes = static_link(&lib, &["puts", "printf"]);
    let want = 200 + 360;
    if bytes != want { println!("STATIC_FAIL 静态镜像库代码={} 应={}（每程序各塞一份拷贝）", bytes, want); return false; }
    println!("STATIC_PASS"); true
}
fn check_dynref() -> bool {
    let mut got = Vec::new();
    let gb = dyn_link(2, &mut got);
    let mut ok = true;
    if gb != 2 * PTR { println!("DYNREF_FAIL 私有 GOT 字节={} 应={}（只存名字引用+GOT，不拷代码）", gb, 2 * PTR); ok = false; }
    if got.len() != 2 || got.iter().take(2).any(|&g| g != UNRESOLVED) { println!("DYNREF_FAIL GOT 槽应初始化为 UNRESOLVED(0)"); ok = false; }
    if ok { println!("DYNREF_PASS"); }
    ok
}
fn check_resolve() -> bool {
    let lib = shared_lib();
    let mut got = vec![0u64; 2];
    let mut ok = true;
    if resolve(&lib, &["printf", "puts"], &mut got).is_none() { println!("RESOLVE_FAIL needs 都在公共库里，解析应成功"); ok = false; }
    if got[0] != 0x2000 || got[1] != 0x1000 { println!("RESOLVE_FAIL GOT 回填错 got={:#x?} 应=[0x2000,0x1000]", got); ok = false; }
    let mut g2 = vec![0u64; 1];
    if resolve(&lib, &["no_such"], &mut g2).is_some() { println!("RESOLVE_FAIL 缺符号竟解析成功（ld.so 应报未定义）"); ok = false; }
    if ok { println!("RESOLVE_PASS"); }
    ok
}
fn check_share() -> bool {
    let lib = shared_lib();
    let needs = ["puts", "printf", "malloc"];
    let (nprog, sz) = (64u64, 200 + 360 + 280u64);
    let st = static_footprint(&lib, nprog, &needs);
    let dy = dyn_footprint(&lib, nprog, &needs);
    let mut ok = true;
    if st != nprog * sz { println!("SHARE_FAIL 静态总占={} 应={}", st, nprog * sz); ok = false; }
    if dy != sz + nprog * 3 * PTR { println!("SHARE_FAIL 动态总占={} 应={}（一份共享+各自GOT）", dy, sz + nprog * 3 * PTR); ok = false; }
    if st <= 10 * dy { println!("SHARE_FAIL 动态未显著省空间 static={} dyn={}", st, dy); ok = false; }
    if ok { println!("SHARE static={} dyn={} ({:.1}x)", st, dy, st as f64 / dy as f64); println!("SHARE_PASS"); }
    ok
}
fn check_lazy() -> bool {
    let lib = shared_lib();
    let needs = ["puts", "printf", "malloc"];
    let mut ok = true;
    { // now
        let mut got = vec![0u64; 3];
        let mut rc = 0u32;
        resolve(&lib, &needs, &mut got);
        rc = 3;
        let before = rc;
        for i in 0..3 { call_lazy(&lib, &mut got, i, &needs, &mut rc); call_lazy(&lib, &mut got, i, &needs, &mut rc); }
        if rc != before { println!("LAZY_FAIL now 模式调用阶段不该再解析 rc={} 应={}", rc, before); ok = false; }
    }
    { // lazy
        let mut got = Vec::new();
        let mut rc = 0u32;
        dyn_link(3, &mut got);
        call_lazy(&lib, &mut got, 0, &needs, &mut rc);
        call_lazy(&lib, &mut got, 0, &needs, &mut rc);
        call_lazy(&lib, &mut got, 1, &needs, &mut rc);
        if rc != 2 { println!("LAZY_FAIL lazy 解析次数={} 应=2（每符号首调一次、缓存后不再、未用到的不解析）", rc); ok = false; }
        if got[2] != UNRESOLVED { println!("LAZY_FAIL 未调用的 malloc 不应被解析"); ok = false; }
    }
    if ok { println!("LAZY_PASS"); }
    ok
}
fn check_dlopen() -> bool {
    let lib = shared_lib();
    let exports = HashMap::from([("answer", 42u64)]);
    let mut ok = true;
    if dlopen_sim(&lib, &["puts"]).is_none() { println!("DLOPEN_FAIL 好插件应能 dlopen"); ok = false; }
    match dlsym_sim(&exports, "answer") {
        Some(42) => println!("answer=42"),
        other => { println!("DLOPEN_FAIL dlsym(answer) 应=42 实={:?}", other); ok = false; }
    }
    if dlsym_sim(&exports, "nonexistent").is_some() { println!("DLOPEN_FAIL dlsym 找不到符号应返回 NULL"); ok = false; }
    else { println!("dlsym(nonexistent)=NULL"); }
    if dlopen_sim(&lib, &["missing_extern"]).is_some() { println!("DLOPEN_FAIL 坏插件应 dlopen 失败"); ok = false; }
    else { println!("dlopen rejected bad_plugin: undefined symbol missing_extern"); }
    if ok { println!("DLOPEN_PASS"); }
    ok
}

fn main() {
    let mut all = true;
    all &= check_static();
    all &= check_dynref();
    all &= check_resolve();
    all &= check_share();
    all &= check_lazy();
    all &= check_dlopen();
    if all { println!("ALL_PASS"); } else { std::process::exit(1); }
}
```

## A.5 骨架策略（`exercises/improper/03b-dynlink-sim/`）

骨架 = 与 ans 同结构同 harness，仅把 6 个 `// TODO` 函数体换成**会判 FAIL 的占位**（不引入 panic/越界）：

| 函数 | C 占位 | Rust 占位 | 触发 |
| :-- | :-- | :-- | :-- |
| `static_link` | `return 0;` | `0` | STATIC_FAIL |
| `dyn_link` | `for(...)got[i]=UNRESOLVED; return 0;` | `*got=vec![UNRESOLVED;n]; 0` | DYNREF_FAIL（got 仍合法，不 panic） |
| `resolve` | `return 0;` | `None` | RESOLVE_FAIL |
| `static_footprint`/`dyn_footprint` | `return 0;` | `0` | SHARE_FAIL |
| `call_lazy` | `return got[idx];` | `got[idx]` | LAZY_FAIL（不解析/不计数） |
| `dlopen_sim`/`dlsym_sim` | `return 0;` | `None` | DLOPEN_FAIL |

`Cargo.lock` 复制一份占位即可（与 03-compile-link 同）。`target/` 不入库（`.gitignore` 已忽略）。

## A.6 `essay/THINKING.md`（exercises 含哨兵；ans 已作答删哨兵）

要点：判据 = 答案非空 + 不含 `LABCTL_ESSAY_TODO`（关键字仅作引导，不强判）。三问：

1. **为什么动态链接省空间？** 提示关键字：一份共享 `.so` 物理页多进程共享 / 静态把库代码拷进每个可执行 / 名字引用+NEEDED+重定位 / 磁盘与内存冗余 / 可独立升级库。
2. **私有 GOT 为什么每程序一张、而库代码只一份？** 提示：代码只读可共享、地址因 ASLR/装载基址每进程不同必须各自回填 / GOT 是「每进程的地址回填表」/ PIC。
3. **lazy vs now（RTLD_LAZY/RTLD_NOW）权衡？dlopen 缺符号为何 now 立即失败？** 提示：lazy 首调用才解析、启动快、首调抖动 / now 装载时全解析、配 RELRO 把 GOT 改只读防劫持 / 缺符号 now 立刻 `undefined symbol` 失败、lazy 拖到首次调用才崩。

`ans` 版每问写 2-4 句命中关键字并删除 `<!-- LABCTL_ESSAY_TODO ... -->` 行。

## A.7 `README.md` 要点

- **母题一句话**：编译器/链接器对外部符号有两条路——静态把库代码**拷进每个程序**（费空间），动态只存**名字引用 + 私有 GOT**、公共库**一份共享**靠 ld.so **按 key 解析回填**（省空间）。本课用纯软件 `{name→(addr,size)}` 表 + 私有 GOT 数组把 ld.so 的活建模出来，**不真调 ld**。
- **子实验表**（E1..E6 × 函数 × 判据）：

  | 子实验 | C / Rust 函数 | 要求 | 判据 |
  | :-- | :-- | :-- | :-- |
  | E1 静态拷贝 | `static_link` | 每程序各塞一份 Σsize | `STATIC_PASS` |
  | E2 名字引用+私有GOT | `dyn_link` | 只存引用、GOT 每槽 8B 初值 0 | `DYNREF_PASS` |
  | E3 ld.so 解析 | `resolve` | 按 key 查表回填、缺符号失败 | `RESOLVE_PASS` |
  | E4 共享+对比 | `static_footprint`/`dyn_footprint` | 动态 < 静态/10（实测 22.6x） | `SHARE_PASS` |
  | E5 lazy vs now | `call_lazy` | 首调解析、二调缓存、未用不解析 | `LAZY_PASS` |
  | E6 dlopen | `dlopen_sim`/`dlsym_sim` | dlsym 命中/NULL、缺符号失败 | `DLOPEN_PASS` |

- **DoD**：六枚 `*_PASS` + `ALL_PASS`；C/Rust 任一全过（必修），另一条计辅助分；essay 过 `ESSAY_PASS`。
- **引申**（体现可扩展性，≥4 条）：① 把 `addr` 升级成真函数指针、`call_lazy` 真跳转（PLT trampoline 模型）；② 多命名空间 / 符号插入顺序（`LD_PRELOAD` 注入、global vs local）；③ 版本符号 `puts@GLIBC_2.x`；④ Full RELRO：解析后把 GOT 标记只读、再写触发保护错；⑤ 引用计数 `dlclose` 与 `RTLD_GLOBAL`；⑥ 与真课对照——做完软件模型直接去 `proper/S09b-linking` 用真 gcc/readelf 验证同一套概念。
- **思考题**：见 A.6 三问。
- 用法块：`labctl run improper/03b-dynlink-sim` / `labctl watch` / `labctl hint improper/03b-dynlink-sim`。

---

# B) proper/S09b-linking

## B.1 build 类型决策与 labctl 最小改法

现有 7 种 build 都不合适：`qemu-virt` 编 `kernel.elf` 并 boot QEMU（S 态内核里跑不了 gcc/ar/readelf/dlopen）；`gcc-host`/`gcc-rv64` 是「单源文件编译+运行」的固定流程，无法多命令编排。本课需要按顺序跑十几条真工具命令并对其输出做 grep 断言——**新增 `build="make-host"`**：在变体目录 `make -s test`，Makefile 把每个子实验断言结果 echo 成 `*_PASS`，沿用 labctl 既有 grep 判题模型。

**labctl 改动（共 ~18 行，3 处）**：

`labctl/src/toolchain.rs` —— 在 `build_available` 的 `_ => false` 之前加一臂：

```rust
        "make-host" => {
            which("make") && which("gcc") && which("ar") && which("readelf")
                && which("nm") && which("ldd") && which("objdump") && which("file")
                && which("riscv64-linux-gnu-gcc") && which("riscv64-linux-gnu-objdump")
                && which("qemu-riscv64")
        }
```

`labctl/src/variant/mod.rs` —— 在 `run_variant` 的 `match v.build.as_str()`（L70-79）里加分派：

```rust
        "make-host" => run_make_host(&vdir, timeout_s),
```

`labctl/src/variant/mod.rs` —— 新增函数（仿 `run_qemu_virt`，置于其后）：

```rust
// ── make-host（host 多命令工具链：Makefile 跑真 gcc/ar/readelf/objdump/ld.so/dlopen，echo *_PASS）──
fn run_make_host(dir: &Path, timeout_s: u64) -> Result<RunOutput, String> {
    // make -s test：每个子目标内部 grep 工具输出，只 echo *_PASS / *_FAIL token。
    let run = exec_run("make", &[OsString::from("-s"), OsString::from("test")], dir, timeout_s)?;
    let s = combine(&run);
    let warnings = s.matches("warning:").count();
    Ok(RunOutput { output: s.clone(), warnings, log: s })
}
```

`exec_run` 已自带 GNU `timeout` 包裹防卡死；`make -s` 抑制命令回显，避免工具输出里的杂字（含 `FAIL`/`error` 等）误触 `forbid`——子目标只 echo token。

## B.2 track / env 归属

- **track = `proper`**：S09b 是「真·工具链」，与 03b「软件模型」一一对照，归正经赛道的「真」哲学（真 ELF/真重定位/真 ld.so/真 dlopen）。
- **env = `host`（有意例外）**：proper 惯例 `env=qemu-virt`，但 `env` 字段是 `dead_code`（`manifest.rs` L40-43，注释「qemu&TUI 里程碑消费」），当前**不参与 build 选择**，真正驱动执行的是 `build="make-host"`。故「proper 能否 env=host」——**功能上能**（env 不 gate 任何逻辑），语义上诚实（本课在 host 跑，E4 的 RV 部分走 **qemu-user** 而非 qemu-virt 内核）。在 meta.toml 注释里写明这是例外。
- **替代方案**（若未来里程碑让 env 变成强约束、禁止 proper+host）：(a) 让 `build="make-host"` 的存在即蕴含 host，env 只作展示；或 (b) 改挂 `improper/03c-real-toolchain`（与 03b 同赛道）。**首选仍是 proper/S09b + env=host**。

## B.3 `meta.toml`（exercises 与 ans 同一份）

```toml
id      = "S09b-linking"
title   = "正经·S09b · 真工具链：gcc -c→ET_REL / ar·-static vs -shared / readelf / RV-PLT / dlopen"
track   = "proper"
require = 1
# env：proper 惯例 qemu-virt，但本课跑真 host 工具链 + qemu-user（E4 RV），
#      env 字段当前 dead_code 不参与 build 选择，故置 host（有意例外，详见 README）。
env     = "host"
weight  = 1

[[variant]]
id    = "toolchain"
axis  = "software"
lang  = "c"
dir   = "toolchain"
build = "make-host"          # 新增 build 类型：变体目录跑 `make -s test`

[[variant]]
id    = "essay"
axis  = "essay"
lang  = "text"
dir   = "essay"
build = "essay"

[judge]
expect    = ["ETREL_PASS", "STATIC_PASS", "READELF_PASS", "RVPLT_PASS", "DLOPEN_PASS", "ALL_PASS"]
forbid    = ["FAIL", "panic", "UNEXPECTED"]
timeout_s = 60               # 真 -static glibc 链接 + RV 交叉 + qemu，留足时间

[[hint]]
text = "E1 gcc -c hello.c 产 hello.o = ET_REL（readelf -h 看 Type: REL）：只有节没有程序头，chmod +x 直接跑报 Exec format error（exit 126）。nm 因 printf(\"hello\\n\") 被优化成 puts，故是 'T main'+'U puts'。"
[[hint]]
text = "E2 gcc -static vs 默认动态体积差 ~49x；ldd 静态 'not a dynamic executable'、动态 'libc.so.6'。ar rcs libX.a 打静态库(ar t 列成员)；gcc -shared -fPIC 产 .so(file 显 shared object)。.a 不是 ELF，别 readelf 它。"
[[hint]]
text = "E3 readelf -d 看 NEEDED/BIND_NOW；-l 看 INTERP 与 LOAD（binutils 2.42 实测 4 个 LOAD，判 >=2 不能 ==2）；-r 看 RELATIVE/GLOB_DAT/JUMP_SLO（x86 名被截断成 JUMP_SLO，grep 子串）。Ubuntu 默认就是 BIND_NOW，看经典 lazy 须 -Wl,-z,lazy -no-pie。"
[[hint]]
text = "E4 riscv64-linux-gnu-gcc -no-pie -Wl,-z,lazy；puts@plt 三条 auipc t3 / ld t3,off(t3) / jalr t1,t3（算 GOT 地址→装真实地址→间接跳）；readelf -r 见 R_RISCV_JUMP_SLOT。反汇编 RV 必须用 riscv64-linux-gnu-objdump。qemu 动态版必须 -L /usr/riscv64-linux-gnu。E5 dlopen RTLD_NOW 缺符号立即 'undefined symbol'。"
```

## B.4 `view.toml`（真工具链流水线 + 静/动对比）

```toml
# S09b-linking 可视化：真工具链流水线——.c → ET_REL → {静态各塞一份(大) / 动态名字引用(小)}
#  → readelf 解剖 → 运行期 PLT/GOT 由 ld.so 回填 → dlopen 插件。

[[node]]
id = "src"
label = ".c 源码（printf(\"hello\\n\") 被 gcc 优化成 U puts）"
[[node]]
id = "obj"
label = "gcc -c → hello.o（ET_REL 可重定位；chmod +x 也跑不了：Exec format error）"
[[node]]
id = "static"
label = "ar .a / gcc -static → 各塞一份 libc 拷贝（~785KB，大）"
[[node]]
id = "shared"
label = "-shared -fPIC .so / 默认 PIE → 只存 NEEDED+重定位项（~16KB，小，~49x 差）"
[[node]]
id = "readelf"
label = "readelf -h/-d/-l/-r · nm · ldd · size：解剖 ELF（INTERP/LOAD/JUMP_SLO）"
[[node]]
id = "plt"
label = "运行期 PLT/GOT：RV64 auipc/ld/jalr，ld.so 把 JUMP_SLOT 回填进 GOT"
[[node]]
id = "dlopen"
label = "dlopen/dlsym/dlclose 插件；缺符号 RTLD_NOW 立即 undefined symbol"

[[edge]]
from = "src"
to   = "obj"
[[edge]]
from = "obj"
to   = "static"
[[edge]]
from = "obj"
to   = "shared"
[[edge]]
from = "shared"
to   = "readelf"
[[edge]]
from = "readelf"
to   = "plt"
[[edge]]
from = "plt"
to   = "dlopen"

[[flow]]
name = "et-rel"
path = ["src", "obj"]
note = "E1：.o 是 ET_REL，没有程序头/INTERP，不能直接 execve → ETREL_PASS"
[[flow]]
name = "static-vs-shared"
path = ["obj", "static", "shared"]
note = "E2：静态各塞一份(785K) vs 动态名字引用(16K)，ar/.a vs -shared/.so → STATIC_PASS"
[[flow]]
name = "dissect"
path = ["shared", "readelf"]
note = "E3：readelf -d/-l/-r 看 NEEDED/INTERP/LOAD/重定位，lazy vs BIND_NOW → READELF_PASS"
[[flow]]
name = "rv-plt"
path = ["readelf", "plt"]
note = "E4：RV puts@plt auipc/ld/jalr + R_RISCV_JUMP_SLOT + qemu 实跑 → RVPLT_PASS"
[[flow]]
name = "dlopen"
path = ["plt", "dlopen"]
note = "E5：dlopen/dlsym 正常路径 + 坏插件缺符号失败 → DLOPEN_PASS"

[[iface]]
name = "ET_REL"
bits = "Type: REL"
note = "gcc -c 产物：只有节无程序头；execve 拒绝（exit 126 Exec format error）"
[[iface]]
name = "PIE"
bits = "Type: DYN"
note = "默认动态可执行=PIE(DYN)；要 Type=EXEC 须 -no-pie（判 LOAD>=2 不能==2）"
[[iface]]
name = "JUMP_SLO"
bits = "R_*_JUMP_SLO[T]"
note = "x86 readelf 把 JUMP_SLOT 截断成 JUMP_SLO；grep 用子串。RV 不截断"
[[iface]]
name = "RV_PLT"
bits = "auipc/ld/jalr"
note = "puts@plt：算 GOT 项地址→装入真实地址(ld.so 回填)→间接跳；故须经 GOT"
[[iface]]
name = "INTERP"
bits = "ld-linux-*.so"
note = "动态 ELF 的解释器；RV 动态版 qemu 须 -L /usr/riscv64-linux-gnu 才找得到"
```

## B.5 `toolchain/` 源文件（给定，勿改；exercises 与 ans 同）

`hello.c`（**必须**保持 `printf("hello\n")`，否则 nm 不会出 `U puts`，见 pitfall #4）：

```c
#include <stdio.h>
int main(void) { printf("hello\n"); return 0; }
```

`mathlib.c`：

```c
int square(int x) { return x * x; }
```

`plugin.c`：

```c
int answer(void) { return 42; }
```

`badplugin.c`（引用未定义符号，`-shared` 默认允许、留到装载期失败）：

```c
extern int missing_extern_symbol(void);
int bad(void) { return missing_extern_symbol(); }
```

`host.c`（dlopen/dlsym/dlclose，插件路径走 argv，cwd 无关）：

```c
#include <stdio.h>
#include <dlfcn.h>
int main(int argc, char **argv) {
    void *h = dlopen(argv[1], RTLD_NOW);
    if (!h) { printf("HOST_FAIL: %s\n", dlerror()); return 1; }
    int (*f)(void) = (int (*)(void))dlsym(h, "answer");
    printf("answer=%d\n", f());
    void *n = dlsym(h, "nonexistent");
    printf("nonexistent=%s\n", n ? "PTR" : "NULL");
    void *b = dlopen(argv[2], RTLD_NOW);          /* 坏插件：RTLD_NOW 立即解析 */
    if (!b) printf("dlopen badplugin: %s\n", dlerror());
    else    printf("UNEXPECTED badplugin loaded\n");
    dlclose(h);
    return 0;
}
```

## B.6 参考解 `toolchain/Makefile`（ans 全量）

```make
# 正经·S09b · 真工具链：把 link 全过程用真 gcc/ar/ld/readelf/objdump/dlopen 跑一遍，
# 每个子实验把断言结果 echo 成 *_PASS / *_FAIL，由 labctl(make-host) grep 判题。
# test/clean 骨架与命令行勿改；学生在各 e? 目标填命令与 grep 断言（见 exercises 骨架）。
RV      := riscv64-linux-gnu-gcc
RVOD    := riscv64-linux-gnu-objdump
SYSROOT := /usr/riscv64-linux-gnu
B       := build

.PHONY: test clean e1 e2 e3 e4 e5
test: e1 e2 e3 e4 e5
	@echo ALL_PASS

clean:
	@rm -rf $(B)

$(B):
	@mkdir -p $(B)

# ── E1：.o 是 ET_REL，不能直接跑 ─────────────────────────────────
e1: | $(B)
	@gcc -c hello.c -o $(B)/hello.o
	@if readelf -h $(B)/hello.o | grep -q 'Type:.*REL' \
	 && nm $(B)/hello.o | grep -q ' T main' && nm $(B)/hello.o | grep -q ' U puts' \
	 && { cp $(B)/hello.o $(B)/notrun; chmod +x $(B)/notrun; ! ./$(B)/notrun 2>$(B)/e1.err; grep -q 'Exec format error' $(B)/e1.err; }; \
	 then echo ETREL_PASS; else echo ETREL_FAIL; fi

# ── E2：静态 vs 动态：体积/file/ldd/ar.a vs -shared.so ───────────
e2: | $(B)
	@gcc -c hello.c -o $(B)/hello.o
	@gcc $(B)/hello.o -o $(B)/hello_dyn
	@gcc -static $(B)/hello.o -o $(B)/hello_static
	@gcc -c -fPIC mathlib.c -o $(B)/mathlib.o
	@ar rcs $(B)/libmath.a $(B)/mathlib.o
	@gcc -shared -fPIC mathlib.c -o $(B)/libmath.so
	@if [ $$(stat -c%s $(B)/hello_static) -gt $$(( $$(stat -c%s $(B)/hello_dyn) * 10 )) ] \
	 && ldd $(B)/hello_static | grep -q 'not a dynamic executable' && ldd $(B)/hello_dyn | grep -q 'libc.so.6' \
	 && file $(B)/hello_static | grep -q 'statically linked' && file $(B)/hello_dyn | grep -q 'dynamically linked' \
	 && ar t $(B)/libmath.a | grep -q 'mathlib.o' && file $(B)/libmath.so | grep -q 'shared object'; \
	 then echo STATIC_PASS; else echo STATIC_FAIL; fi

# ── E3：readelf 解剖动态 ELF：-d/-l/-r，lazy vs BIND_NOW ────────
e3: | $(B)
	@gcc -c hello.c -o $(B)/hello.o
	@gcc $(B)/hello.o -o $(B)/hello_dyn
	@gcc $(B)/hello.o -o $(B)/hello_lazy -Wl,-z,lazy -no-pie
	@if readelf -d $(B)/hello_dyn | grep -q 'NEEDED.*libc.so.6' \
	 && readelf -l $(B)/hello_dyn | grep -q 'ld-linux-x86-64.so.2' && [ $$(readelf -l $(B)/hello_dyn | grep -c 'LOAD') -ge 2 ] \
	 && readelf -r $(B)/hello_dyn | grep -q 'JUMP_SLO' && readelf -r $(B)/hello_dyn | grep -q 'GLOB_DAT' && readelf -r $(B)/hello_dyn | grep -q 'RELATIVE' \
	 && readelf -d $(B)/hello_dyn | grep -q 'BIND_NOW' && ! readelf -d $(B)/hello_lazy | grep -q 'BIND_NOW'; \
	 then echo READELF_PASS; else echo READELF_FAIL; fi

# ── E4：RISC-V 交叉：puts@plt auipc/ld/jalr + R_RISCV_JUMP_SLOT + qemu 实跑 ──
e4: | $(B)
	@$(RV) -no-pie -fno-pie -Wl,-z,lazy hello.c -o $(B)/hello_rv64
	@if file $(B)/hello_rv64 | grep -q 'RISC-V' \
	 && readelf -r $(B)/hello_rv64 | grep -q 'R_RISCV_JUMP_SLOT' \
	 && $(RVOD) -d -j .plt $(B)/hello_rv64 | grep -A3 'puts@plt' | grep -q auipc \
	 && $(RVOD) -d -j .plt $(B)/hello_rv64 | grep -A3 'puts@plt' | grep -qP '\tld\t' \
	 && $(RVOD) -d -j .plt $(B)/hello_rv64 | grep -A3 'puts@plt' | grep -q jalr \
	 && qemu-riscv64 -L $(SYSROOT) $(B)/hello_rv64 | grep -q '^hello$$'; \
	 then echo RVPLT_PASS; else echo RVPLT_FAIL; fi

# ── E5：dlopen/dlsym/dlclose + 缺符号失败 ────────────────────────
e5: | $(B)
	@gcc -shared -fPIC plugin.c -o $(B)/plugin.so
	@gcc -shared -fPIC badplugin.c -o $(B)/badplugin.so
	@gcc host.c -o $(B)/host -ldl
	@./$(B)/host $(B)/plugin.so $(B)/badplugin.so > $(B)/host.out 2>&1; \
	 if grep -q 'answer=42' $(B)/host.out && grep -q 'nonexistent=NULL' $(B)/host.out \
	 && grep -q 'undefined symbol: missing_extern_symbol' $(B)/host.out; \
	 then echo DLOPEN_PASS; else echo DLOPEN_FAIL; fi
```

设计要点 / 已编进判据的 pitfall：每个 `e?` 目标都把工具输出喂给 grep、**只 echo 一枚 token 且永远 exit 0**（`if…then…else…fi`），故 `make` 不中断、token 完整。`JUMP_SLO`（x86 截断）/ `LOAD>=2`（实测 4 个）/ `printf("hello\n")→U puts` / 看经典 lazy 用 `-Wl,-z,lazy -no-pie` / RV 动态 qemu `-L $(SYSROOT)` / `-static` 体积判 `>10x`（留余量，实测 49x）/ `.a` 用 `ar t` 不用 readelf / RV 反汇编用 `riscv64-linux-gnu-objdump`。

## B.7 骨架 `Makefile`（`exercises/proper/S09b-linking/toolchain/Makefile`）

骨架保留头部变量、`test`/`clean`/`$(B)` 目标与每个 `e?` 目标的**给定编译命令**，把**断言 `@if…fi`** 换成占位并留 TODO，使其 echo `*_FAIL`：

```make
e1: | $(B)
	@gcc -c hello.c -o $(B)/hello.o
	# TODO(E1): 用 readelf -h / nm / 直接执行 断言 ET_REL、'T main'+'U puts'、Exec format error；全中 echo ETREL_PASS
	@echo ETREL_FAIL
# e2..e5 同理：保留 gcc/ar/objdump 命令，断言处 echo <TOKEN>_FAIL 并留 TODO
```

骨架每题 echo `*_FAIL` → `forbid` 命中 + 对应 `*_PASS` 缺失 → 判 FAIL（按预期）。学生把 TODO 换成 B.6 的真断言即过。

## B.8 `essay/THINKING.md` 要点（ans 删哨兵，exercises 留哨兵）

三问（关键字判）：① `.o` chmod +x 仍跑不了的根因（ET_REL/无程序头/无 PT_LOAD/无 INTERP/符号 U 未重定位/需 ld 链成 EXEC 或 PIE）；② `-static` 比动态大 ~49x 的来龙去脉与静/动权衡（拷贝 vs 共享、可移植 vs 省空间/可升级/LD_PRELOAD）；③ RV `puts@plt` 三条指令各做什么、为何必须经 GOT 间接而非一条 `jal`（链接期地址未知 + 超 `jal ±1MB` PC 相对范围 + PIC/ASLR）。

## B.9 `README.md` 要点

- 母题：链接不是一个动作而是一条**真实工具链流水线**——`gcc -c` 出 ET_REL（不能跑）→ `ld` 链成可执行（静态拷贝 / 动态名字引用）→ 运行期 `ld.so` 经 PLT/GOT 解析。本课**真跑** gcc/ar/ld/readelf/nm/ldd/objdump/qemu/dlopen，把 03b 的软件模型逐一对到真产物。
- 子实验表 E1..E5 × 命令 × 判据 token（ETREL/STATIC/READELF/RVPLT/DLOPEN）。
- DoD：五枚 `*_PASS` + `ALL_PASS`；essay 过。
- **明确写出 env=host 的例外说明**（proper 惯例 qemu-virt，本课 host+qemu-user，env 当前 dead_code）。
- 引申：① 自写 `linker.ld` 控段地址再 readelf 验证；② `-z relro,now` vs `-z lazy` 对 GOT 可写性的影响（Full RELRO）；③ 版本符号与 `--version-script`；④ `LD_LIBRARY_PATH`/`-rpath` 修复 `cannot open shared object file`；⑤ 静态库成员选择与 `--gc-sections`；⑥ 对照 03b 软件模型逐题验真。
- 用法块：`labctl run proper/S09b-linking` / `make -C toolchain test`（手动）。

---

# C) 小结：四件套对照

| 文件 | improper/03b | proper/S09b |
| :-- | :-- | :-- |
| meta.toml | A.1（gcc-host/cargo/essay，6 token） | B.3（make-host/essay，5 token，env=host 例外） |
| view.toml | A.2（静态大 vs 动态小 + ld.so 回填 GOT） | B.4（工具链流水线 + 静/动 + RV-PLT，node/edge/flow/iface 用满，wave 省略） |
| essay/THINKING | A.6（省空间/私有GOT/lazy-now 三问） | B.8（ET_REL/49x/RV-PLT 三问） |
| README | A.7 | B.9 |
| 源 | sw/c/dynlink.c + sw/rust（A.3/A.4） | toolchain/{Makefile,hello.c,mathlib.c,plugin.c,badplugin.c,host.c}（B.5/B.6） |

---

# D) `info.toml` 注册

在根 `info.toml` 的 `order` 数组里两处插入（03b 紧跟 03 后；S09b 紧跟 S09 后）：

```diff
   "improper/03-compile-link",
+  "improper/03b-dynlink-sim",
   "improper/04-thread",
```

```diff
   "proper/S09-libc",
+  "proper/S09b-linking",
   "proper/S10-userland",
```

命名遵循现有子版本规约（`09b-vfs`、`S05b-kheap`、`S06b-hal` 等）：`improper/03b-<name>`、`proper/S09b-<name>`。

---

# E) 实现顺序与验证方法

**顺序**

1. **改 labctl**（B.1 三处）→ `cargo build --manifest-path labctl/Cargo.toml`，确认编译过、`labctl list` 正常。
2. **03b**：先建 `ans/improper/03b-dynlink-sim/`（meta/view/README/essay + sw/c/dynlink.c + sw/rust 全套，A.3/A.4 全量 + `Cargo.lock`），跑通后再**派生 exercises 骨架**（A.5：仅替换 6 个 TODO 体为占位；essay 留哨兵）。
3. **S09b**：先建 `ans/proper/S09b-linking/`（meta/view/README/essay + `toolchain/` 全部源 + B.6 Makefile），跑通后派生 exercises 骨架（B.7：断言改 `echo *_FAIL`；essay 留哨兵）。
4. **注册** info.toml（D）。
5. `.gitignore` 已忽略 `target/`、`build/`；ans 里 03b 的 `sw/rust/target/` 与 S09b 的 `toolchain/build/` 不入库。

**验证命令与期望**

```bash
# 参考解：应全过（绿）
labctl run improper/03b-dynlink-sim --solutions   # sw-c ✓ sw-rust ✓ essay ✓
labctl run proper/S09b-linking      --solutions   # toolchain ✓ essay ✓
# 骨架：应判 FAIL（红，证明留白有效）
labctl run improper/03b-dynlink-sim              # *_FAIL，缺 *_PASS
labctl run proper/S09b-linking                    # *_FAIL，缺 *_PASS
# 全量记分板：含新两课全绿
labctl verify --solutions
```

**逐项核对要点**

- 03b 参考解输出须含 `STATIC_PASS DYNREF_PASS RESOLVE_PASS SHARE_PASS LAZY_PASS DLOPEN_PASS ALL_PASS`，且 `SHARE static=53760 dyn=2376 (22.6x)`；无 `FAIL/panic/ERROR`。骨架须出现至少一枚 `*_FAIL`。
- S09b 参考解：`make -s test` 须打印 `ETREL_PASS STATIC_PASS READELF_PASS RVPLT_PASS DLOPEN_PASS ALL_PASS`；可先手动 `make -C ans/proper/S09b-linking/toolchain test` 验证（耗时主要在 `-static` glibc 链接与 RV qemu，60s 足够）。骨架 `make` 须打印 `*_FAIL`。
- essay：`ans` 版 THINKING 无 `LABCTL_ESSAY_TODO` 且非空 → `essay` 变体 ✓；`exercises` 版含哨兵 → ✗。
- 工具链门：本机已验 `gcc/ar/readelf/nm/ldd/objdump/size/file/make/qemu-riscv64/riscv64-linux-gnu-gcc/riscv64-linux-gnu-objdump/cargo` 全部就位、sysroot `/usr/riscv64-linux-gnu/...ld-linux-riscv64-lp64d.so.1` 存在；任一缺失则 `make-host` 变体降级 `⊘ Unavailable`（不算挂）。
```
