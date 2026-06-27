//! 形态 F6 · 混合内核（hybrid）—— Rust 参考解。
//!
//! 母题：宏内核(F1)把所有服务塞内核态，syscall→fn call，几纳秒，最快但
//!       一个 driver bug 全系统崩；微内核(F2)把服务全赶到用户态，跨服务都走
//!       IPC，最隔离但每次都要陷入+切换+拷贝+回复，慢。
//!
//! 混合内核(Windows NT / macOS XNU)想「两全其美」：把性能关键服务留在内核态
//! 直调(快)，把需要隔离的服务放用户态走消息(隔离)。本 demo 用最朴素的软件模型
//! 把这个折中演示出来——同一份工作负载，分别按「全直调 / 混合 / 全消息」三种
//! 路由跑，统计开销与隔离度，亲眼看到混合「两头不靠」：比纯宏慢、比纯微不隔离。
//!
//! 学生只需填 1 个函数 `route`（按服务类型选直调或消息）；下方 harness 勿改。
#![allow(dead_code)]

// ── 放置策略：服务跑在内核态(直调) 还是 用户态(消息) ─────────────
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Placement {
    Kernel, // 内核态直调：一次函数调用，快，但与内核同生共死(无隔离)
    User,   // 用户态服务：请求/回复消息往返，慢，但崩了不拖垮内核(有隔离)
}

// ── 服务类型：性能关键 vs 需要隔离 ───────────────────────────────
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Kind {
    Perf,    // 性能关键(调度/时钟/内存)——热路径，受不了 IPC 开销
    Isolate, // 需要隔离(文件/网络/驱动/音频)——第三方代码多，崩了别连累内核
}

// ── 抽象开销模型（以「时钟拍」计）─────────────────────────────────
const COST_DIRECT: u64 = 1; // 内核态直调：一次 fn call
const COST_IPC: u64 = 10; // 用户态消息往返：陷入+切地址空间+拷贝+回复
const MSGS_PER_IPC: u64 = 2; // 每次用户态调用 = 请求 + 回复 两条消息

// ── 一个服务：名字 + 类型 + 它干的「活」(纯计算，放哪都算同样结果) ──
struct Service {
    name: &'static str,
    kind: Kind,
    work: fn(u64) -> u64,
}

fn w_double(x: u64) -> u64 {
    x * 2
}
fn w_inc(x: u64) -> u64 {
    x + 1
}
fn w_sq(x: u64) -> u64 {
    (x * x) & 0xFFFF
}

// 7 个服务：3 个性能关键 + 4 个需要隔离。
const SERVICES: &[Service] = &[
    Service { name: "sched", kind: Kind::Perf, work: w_double },
    Service { name: "timer", kind: Kind::Perf, work: w_inc },
    Service { name: "mm", kind: Kind::Perf, work: w_sq },
    Service { name: "fs", kind: Kind::Isolate, work: w_double },
    Service { name: "net", kind: Kind::Isolate, work: w_inc },
    Service { name: "driver", kind: Kind::Isolate, work: w_sq },
    Service { name: "audio", kind: Kind::Isolate, work: w_double },
];

// ════════════════════════════════════════════════════════════════
// 学生填空区：唯一要填的服务路由
// ════════════════════════════════════════════════════════════════

/// 按服务类型选放置策略：
/// 性能关键(Perf) → 内核态直调(Kernel，快)；需要隔离(Isolate) → 用户态消息(User，隔离)。
/// 这正是混合内核 Cutler(NT)/XNU 的设计直觉：热路径留 ring0，可替换/危险代码推用户态。
fn route(kind: Kind) -> Placement {
    match kind {
        Kind::Perf => Placement::Kernel,
        Kind::Isolate => Placement::User,
    }
}

// ════════════════════════════════════════════════════════════════
// 测试 harness（给定，勿改）
// ════════════════════════════════════════════════════════════════

#[derive(Default)]
struct Metrics {
    kcalls: u64, // 内核态直调发生次数
    umsgs: u64,  // 用户态消息条数(每次调用 2 条)
    cost: u64,   // 累计开销(时钟拍)
}

/// 派发一次服务调用：按路由决定走直调还是消息，累计开销，返回计算结果。
/// 关键：结果与放置无关——放哪都算对，放置只改变「开销」与「隔离」。
fn dispatch(svc: &Service, x: u64, m: &mut Metrics) -> u64 {
    match route(svc.kind) {
        Placement::Kernel => {
            m.kcalls += 1;
            m.cost += COST_DIRECT;
        }
        Placement::User => {
            m.umsgs += MSGS_PER_IPC;
            m.cost += COST_IPC;
        }
    }
    (svc.work)(x)
}

fn check_hybrid() -> bool {
    let mut ok = true;

    // (a) 放置分布：必须既有内核直调服务，又有用户消息服务，才算「混合」。
    let nk = SERVICES.iter().filter(|s| route(s.kind) == Placement::Kernel).count();
    let nu = SERVICES.iter().filter(|s| route(s.kind) == Placement::User).count();
    if nk == 0 {
        println!("HYBRID_MISS 没有内核态直调服务(退化成纯微内核 F2)");
        ok = false;
    }
    if nu == 0 {
        println!("HYBRID_MISS 没有用户态消息服务(退化成纯宏内核 F1)");
        ok = false;
    }

    // (b) 两类服务都要真跑通：对同一输入，结果与「直接调 work」一致(放置不改语义)。
    let mut m = Metrics::default();
    for s in SERVICES {
        let y = dispatch(s, 21, &mut m);
        let want = (s.work)(21);
        if y != want {
            println!("HYBRID_MISS 服务 {} 结果错 got={} want={}", s.name, y, want);
            ok = false;
        }
    }

    // (c) 两条路径都要真实触发过。
    if m.kcalls == 0 || m.umsgs == 0 {
        println!("HYBRID_MISS 两类调用路径未都触发 kcalls={} umsgs={}", m.kcalls, m.umsgs);
        ok = false;
    }

    println!("PLACE 内核直调服务={} 用户消息服务={}（共 {}）", nk, nu, SERVICES.len());
    if ok {
        println!("HYBRID_PASS");
    }
    ok
}

fn check_tradeoff() -> bool {
    let mut ok = true;

    // 同一工作负载：每个服务各调用一次，按当前路由统计。
    let mut m = Metrics::default();
    for s in SERVICES {
        dispatch(s, 10, &mut m);
    }

    let n = SERVICES.len() as u64;
    let mono = n * COST_DIRECT; // F1 全直调(纯宏)：最快
    let micro = n * COST_IPC; // F2 全消息(纯微)：最慢
    let hybrid = m.cost; // 混合：折中

    println!("COST mono(F1全直调)={} hybrid(混合)={} micro(F2全消息)={}", mono, hybrid, micro);
    println!("MSG  kcalls(内核直调计数)={} umsgs(用户消息计数)={}", m.kcalls, m.umsgs);

    // (a) 折中：混合开销严格落在 F1 与 F2 之间——比纯宏慢(为隔离付费)、比纯微快(牺牲隔离)。
    if !(mono < hybrid && hybrid < micro) {
        println!("TRADEOFF_MISS 混合开销 {} 未严格落在 F1({}) 与 F2({}) 之间", hybrid, mono, micro);
        ok = false;
    }

    // (b) 隔离度：混合的隔离(用户态)服务数严格介于 0(纯宏) 与 N(纯微) 之间——两头都不占满。
    let iso = SERVICES.iter().filter(|s| route(s.kind) == Placement::User).count();
    if !(0 < iso && iso < SERVICES.len()) {
        println!("TRADEOFF_MISS 隔离服务数 {} 未严格介于 0 与 {} 之间", iso, n);
        ok = false;
    }

    // (c) 开销事件：用户态消息计数 > 内核态直调计数——IPC 的「隔离税」清晰可见。
    if !(m.kcalls < m.umsgs) {
        println!("TRADEOFF_MISS 内核直调计数 {} 应 < 用户消息计数 {}", m.kcalls, m.umsgs);
        ok = false;
    }

    if ok {
        println!("TRADEOFF_PASS");
    }
    ok
}

fn main() {
    let mut all = true;
    all &= check_hybrid();
    all &= check_tradeoff();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
