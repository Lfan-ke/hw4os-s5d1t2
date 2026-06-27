//! 形态 F4 · 库OS / Unikernel —— Rust（host 软件直觉 demo，不是真内核）。
//!
//! 本质权衡，用最朴素的软件模型演出来：
//!   1. OS 例程就是被 app 直接链接的库函数 —— uni_write / uni_alloc / uni_clock。
//!   2. app→OS 是直接函数调用，替代「陷入」(trap)：传统模型每次系统服务 traps+1，
//!      unikernel 模型直接 call，陷入计数恒为 0。
//!   3. 单应用 ⇒ 编译期特化裁剪：app 不用的模块(net/blk/fs)整段裁掉，镜像变小。
//!   4. capstone：一镜像 = app + OS，同地址空间、零陷入、只含用到的模块。
//!
//! 你只需填 6 个函数体（标 TODO 处）；下方测试 harness 勿改。
#![allow(unused_variables, dead_code)]

// ── 镜像模块表：每个子系统是一个可单独选/不选的「微库」(仿 unikraft 88 lib) ──
struct Module {
    name: &'static str,
    bit: u32,
    size: u32, // 模块的「符号数 / 代码量」——链入它镜像就变大这么多
}

const MOD_CONSOLE: u32 = 1 << 0;
const MOD_ALLOC: u32 = 1 << 1;
const MOD_CLOCK: u32 = 1 << 2;
const MOD_NET: u32 = 1 << 3;
const MOD_BLK: u32 = 1 << 4;
const MOD_FS: u32 = 1 << 5;

const MODULES: [Module; 6] = [
    Module { name: "console", bit: MOD_CONSOLE, size: 10 },
    Module { name: "alloc", bit: MOD_ALLOC, size: 14 },
    Module { name: "clock", bit: MOD_CLOCK, size: 6 },
    Module { name: "net", bit: MOD_NET, size: 40 },
    Module { name: "blk", bit: MOD_BLK, size: 32 },
    Module { name: "fs", bit: MOD_FS, size: 50 },
];

/// 全量「通用内核」：链入所有模块（像传统 OS 什么都带着）。
const ALL_MODS: u32 = MOD_CONSOLE | MOD_ALLOC | MOD_CLOCK | MOD_NET | MOD_BLK | MOD_FS;
/// 本 app 实际用到的：只有 console / alloc / clock —— 特化镜像就只链这三个。
const APP_USES: u32 = MOD_CONSOLE | MOD_ALLOC | MOD_CLOCK;

// ── 系统服务号（传统模型里是 syscall 号，unikernel 里只是 match 分支）──
const SVC_WRITE: u32 = 0;
const SVC_ALLOC: u32 = 1;
const SVC_CLOCK: u32 = 2;

/// 「被链接进 app 的那点 OS 状态」——和 app 同处一个地址空间。
struct UniKernel {
    console_len: usize, // 写进控制台的字节数
    heap_top: usize,    // bump 分配器游标
    ticks: u64,         // 单调时钟
}

impl UniKernel {
    fn new() -> Self {
        UniKernel { console_len: 0, heap_top: 0, ticks: 0 }
    }
}

// ════════════════════════════════════════════════════════════════
// 学生填空区：3 个 OS 例程 + 1 个直接绑定 + 2 个特化开关
// ════════════════════════════════════════════════════════════════

// ── OS 例程：普通库函数，app 直接 call（同地址空间，无陷入）──

/// console 写：把 n 字节追加进控制台，返回写入字节数。
fn uni_write(k: &mut UniKernel, n: usize) -> usize {
    // TODO: k.console_len += n; 返回 n。
    0 // ← 占位
}

/// bump 分配：返回分配前的 heap_top，再把游标前移 n（连续分配区间不重叠）。
fn uni_alloc(k: &mut UniKernel, n: usize) -> usize {
    // TODO: let off = k.heap_top; k.heap_top += n; 返回 off。
    0 // ← 占位
}

/// 单调时钟：返回当前 tick，再自增。
fn uni_clock(k: &mut UniKernel) -> u64 {
    // TODO: let t = k.ticks; k.ticks += 1; 返回 t。
    0 // ← 占位
}

// ── 直接绑定：app→OS 用直接函数调用替代 syscall 陷入 ──

/// 直接派发：按 svc 直接调对应的 uni_* 例程——**绝不碰陷入计数器**。
/// 对比 harness 的 dispatch_trap：内核代码相同，差别只在那一次 *traps += 1。
fn dispatch_direct(k: &mut UniKernel, svc: u32, arg: u32) -> u64 {
    // TODO: match svc { SVC_WRITE => uni_write(k, arg as usize) as u64,
    //                   SVC_ALLOC => uni_alloc(k, arg as usize) as u64,
    //                   SVC_CLOCK => uni_clock(k), _ => 0 }
    0 // ← 占位
}

// ── 特化开关：单应用 ⇒ 编译期把没用到的模块裁掉 ──

/// 某模块是否链入镜像：app 用到了才链入（仿 Kconfig/feature 的编译期开关）。
fn is_linked(used: u32, module_bit: u32) -> bool {
    // TODO: (used & module_bit) != 0
    false // ← 占位
}

/// 镜像符号数：把所有「被链入」模块的 size 累加——没链入的不计入，镜像就变小。
fn image_symbols(used: u32) -> u32 {
    // TODO: 遍历 MODULES，is_linked(used, m.bit) 才把 m.size 加进 total。
    0 // ← 占位
}

// ════════════════════════════════════════════════════════════════
// 测试 harness（给定，勿改）
// ════════════════════════════════════════════════════════════════

/// 传统模型的派发：每次系统服务都要 user→kernel 陷入(模式切换 / ecall)，故 traps += 1。
/// 它调用的是**同一批** uni_* 例程——唯一差别就是这道陷入墙。
fn dispatch_trap(k: &mut UniKernel, svc: u32, arg: u32, traps: &mut u64) -> u64 {
    *traps += 1; // ← 陷入开销：unikernel 的 dispatch_direct 没有这一行
    match svc {
        SVC_WRITE => uni_write(k, arg as usize) as u64,
        SVC_ALLOC => uni_alloc(k, arg as usize) as u64,
        SVC_CLOCK => uni_clock(k),
        _ => 0,
    }
}

fn check_uni() -> bool {
    let mut ok = true;
    let mut k = UniKernel::new();

    // (a) console 写：直接 call，返回写入字节数。
    let w1 = uni_write(&mut k, 5);
    let w2 = uni_write(&mut k, 3);
    if w1 != 5 || w2 != 3 || k.console_len != 8 {
        println!("UNI_FAIL 直接写控制台 w1={} w2={} len={} 应=(5,3,8)", w1, w2, k.console_len);
        ok = false;
    }
    // (b) bump 分配：偏移 0、16，游标 24。
    let a1 = uni_alloc(&mut k, 16);
    let a2 = uni_alloc(&mut k, 8);
    if a1 != 0 || a2 != 16 || k.heap_top != 24 {
        println!("UNI_FAIL bump 分配 a1={} a2={} top={} 应=(0,16,24)", a1, a2, k.heap_top);
        ok = false;
    }
    // (c) 单调时钟：0,1,2。
    let c0 = uni_clock(&mut k);
    let c1 = uni_clock(&mut k);
    let c2 = uni_clock(&mut k);
    if c0 != 0 || c1 != 1 || c2 != 2 {
        println!("UNI_FAIL 单调时钟 = {},{},{} 应=0,1,2", c0, c1, c2);
        ok = false;
    }

    if ok {
        println!("UNI_PASS");
    }
    ok
}

fn check_direct() -> bool {
    let mut ok = true;
    // 同一份工作负载：(服务, 参数)。
    let workload = [
        (SVC_WRITE, 4u32),
        (SVC_ALLOC, 16),
        (SVC_CLOCK, 0),
        (SVC_WRITE, 2),
        (SVC_CLOCK, 0),
    ];

    // 传统模型：每次系统服务一次陷入。
    let mut kt = UniKernel::new();
    let mut traps_trad = 0u64;
    let mut res_trad = Vec::new();
    for &(svc, arg) in workload.iter() {
        res_trad.push(dispatch_trap(&mut kt, svc, arg, &mut traps_trad));
    }

    // unikernel：直接函数调用，陷入计数从不自增。
    let mut ku = UniKernel::new();
    let traps_uni = 0u64; // dispatch_direct 不接触陷入计数器
    let mut res_uni = Vec::new();
    for &(svc, arg) in workload.iter() {
        res_uni.push(dispatch_direct(&mut ku, svc, arg));
    }

    // 业务结果必须逐项一致（差别只在「有没有陷入」，不在「干的活」）。
    if res_trad != res_uni {
        println!("DIRECT_FAIL 业务结果不一致 trad={:?} uni={:?}", res_trad, res_uni);
        ok = false;
    }
    if traps_trad != workload.len() as u64 {
        println!("DIRECT_FAIL 传统模型陷入数={} 应={}", traps_trad, workload.len());
        ok = false;
    }
    if traps_uni != 0 {
        println!("TRAP_LEAK_FAIL unikernel 直接调用却产生了 {} 次陷入", traps_uni);
        ok = false;
    }

    println!("TRAP_COST 传统模型陷入={} unikernel 陷入={}", traps_trad, traps_uni);
    if ok {
        println!("DIRECT_PASS");
    }
    ok
}

fn check_specialize() -> bool {
    let mut ok = true;

    // (a) 单应用 ⇒ 特化：只链 app 用到的模块，镜像应当比全量小。
    let full = image_symbols(ALL_MODS);
    let spec = image_symbols(APP_USES);
    if spec >= full {
        println!("SPECIALIZE_BLOAT_FAIL 特化镜像符号数={} 未小于全量={}", spec, full);
        ok = false;
    }

    // (b) 用到的模块必须在镜像里；没用到的必须被裁掉。
    for m in MODULES.iter() {
        let linked = is_linked(APP_USES, m.bit);
        let used = APP_USES & m.bit != 0;
        if linked != used {
            println!("SPECIALIZE_FAIL 模块 {} linked={} 应={}", m.name, linked, used);
            ok = false;
        }
    }

    // (c) 特化后仍能服务 app 真正用到的三类调用。
    let mut k = UniKernel::new();
    if dispatch_direct(&mut k, SVC_WRITE, 3) != 3 {
        println!("SPECIALIZE_FAIL 特化镜像无法提供 console 服务");
        ok = false;
    }

    println!("IMAGE_SIZE 全量符号={} 特化符号={}", full, spec);
    if ok {
        println!("SPECIALIZE_PASS");
    }
    ok
}

fn check_image() -> bool {
    let mut ok = true;

    // 单镜像 = app + OS 同地址空间：构造 APP_USES 特化镜像并「启动」app。
    let img = APP_USES;
    let mut k = UniKernel::new();
    let traps = 0u64; // 整个 app 生命周期内的陷入计数——全程直接调用，恒 0

    // app main：全程经 dispatch_direct 直接调 OS。
    let banner = dispatch_direct(&mut k, SVC_WRITE, 6); // 写 6 字节 banner
    let p0 = dispatch_direct(&mut k, SVC_ALLOC, 32);
    let p1 = dispatch_direct(&mut k, SVC_ALLOC, 16);
    let t0 = dispatch_direct(&mut k, SVC_CLOCK, 0);
    let t1 = dispatch_direct(&mut k, SVC_CLOCK, 0);

    if banner != 6 || k.console_len != 6 {
        println!("IMAGE_FAIL banner 写入异常 ret={} len={} 应=(6,6)", banner, k.console_len);
        ok = false;
    }
    if p0 != 0 || p1 != 32 {
        println!("IMAGE_FAIL 分配区重叠 p0={} p1={} 应=0,32", p0, p1);
        ok = false;
    }
    if t1 <= t0 {
        println!("IMAGE_FAIL 时钟非单调 t0={} t1={}", t0, t1);
        ok = false;
    }
    if traps != 0 {
        println!("TRAP_LEAK_FAIL 镜像运行期出现 {} 次陷入", traps);
        ok = false;
    }
    // 特化镜像里不该混入未用模块。
    if is_linked(img, MOD_NET) || is_linked(img, MOD_BLK) || is_linked(img, MOD_FS) {
        println!("IMAGE_FAIL 特化镜像混入了未用模块(net/blk/fs)");
        ok = false;
    }

    if ok {
        println!("IMAGE_PASS");
    }
    ok
}

fn main() {
    let mut all = true;
    all &= check_uni();
    all &= check_direct();
    all &= check_specialize();
    all &= check_image();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
