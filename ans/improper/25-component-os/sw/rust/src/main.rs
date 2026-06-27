//! 不正经赛道 · 25 组件化 OS —— Rust 参考解（host 软件直觉 demo，不是真内核）。
//!
//! 母题（arceos 的精髓）：内核不必从头写。把可复用的「组件」(分配器/调度器/控制台)
//! 当积木，用「特性开关」(cargo features 的心智模型) **按需组装**——同一套组件，
//! 不同拼法就拼出不同形态的 OS：
//!   ① 组件：每个有统一接口(函数指针 vtable) + 1~2 个可替换实现
//!      Allocator：bump / freelist；Scheduler：fifo / rr；Console：plain。
//!   ② 组装：KernelConfig 用特性开关选「装哪些组件 / 用哪个实现 / 要不要 syscall 边界」，
//!      wire 起来跑一个小工作负载。
//!   ③ 两种形态：UNIKERNEL(最小组件 + app 直链、无 syscall 边界、零陷入)
//!              vs MONOLITHIC(更多组件 + syscall 边界、有陷入)——同源组件不同拼法。
//!   ④ 热替换：把分配器/调度器换一个实现，OS 仍正常工作。
//!
//! 学生只填两处：build_kernel(按特性组装/wiring) + freelist_alloc(替换一个组件实现)。
//! 其余 harness（向量 + 不变量 + PASS 打印）勿改。
#![allow(dead_code)]

// ════════════════════════════════════════════════════════════════
// 组件接口：统一用「函数指针 vtable」表示一个可替换组件
// （Rust 也能用 trait，这里用 fn 指针好和 C 版逐字对应）
// ════════════════════════════════════════════════════════════════

const SLOT: usize = 64; // freelist 的固定槽大小
const CAP: u32 = 16; // freelist 的槽数上限

/// 分配器组件共享的那点状态（bump 用 top，freelist 用 slots）。
struct AllocState {
    top: usize,   // bump 游标
    slots: u32,   // freelist 已用槽数
}
impl AllocState {
    fn new() -> Self {
        AllocState { top: 0, slots: 0 }
    }
}

type AllocFn = fn(&mut AllocState, usize) -> i64;
/// Allocator 组件：名字 + 分配函数（返回偏移，OOM 返回 -1）。
struct Allocator {
    name: &'static str,
    alloc: AllocFn,
}

type ConsFn = fn(&mut usize, usize) -> i64;
/// Console 组件：名字 + 写函数（console_len += n，返回 n）。
struct Console {
    name: &'static str,
    write: ConsFn,
}

#[derive(Clone, Copy)]
struct Task {
    id: u32,
    burst: u32,
}
type SchedFn = fn(&[Task], &mut [u32]) -> usize;
/// Scheduler 组件：名字 + 跑函数（把执行序列写进 out，返回长度）。
struct Scheduler {
    name: &'static str,
    run: SchedFn,
}

// ── 组件实现：分配器两种 ─────────────────────────────────────────

/// bump 分配：紧凑前移，off = top，top += n（连续区间紧贴、不重叠）。
fn bump_alloc(s: &mut AllocState, n: usize) -> i64 {
    let off = s.top as i64;
    s.top += n;
    off
}

// ── 组件实现：调度器两种 ─────────────────────────────────────────

/// FIFO（运行到完成）：一个任务跑光它的 burst 步，再轮下一个。
fn fifo_run(tasks: &[Task], out: &mut [u32]) -> usize {
    let mut idx = 0;
    for t in tasks {
        for _ in 0..t.burst {
            out[idx] = t.id;
            idx += 1;
        }
    }
    idx
}

/// Round-Robin（时间片=1）：每轮每个未完成任务走一步，轮转直到全部排空。
fn rr_run(tasks: &[Task], out: &mut [u32]) -> usize {
    let mut rem = [0u32; 16];
    for (i, t) in tasks.iter().enumerate() {
        rem[i] = t.burst;
    }
    let mut idx = 0;
    let mut left = tasks.iter().map(|t| t.burst).sum::<u32>();
    while left > 0 {
        for (i, t) in tasks.iter().enumerate() {
            if rem[i] > 0 {
                out[idx] = t.id;
                idx += 1;
                rem[i] -= 1;
                left -= 1;
            }
        }
    }
    idx
}

// ── 组件实现：控制台一种 ─────────────────────────────────────────

fn console_plain(len: &mut usize, n: usize) -> i64 {
    *len += n;
    n as i64
}

// ── 组件「注册表 / 工厂」：按特性枚举取对应实现（给定）──────────────

#[derive(Clone, Copy, PartialEq)]
enum AllocKind {
    Bump,
    FreeList,
}
#[derive(Clone, Copy, PartialEq)]
enum SchedKind {
    NoSched,
    Fifo,
    Rr,
}

fn make_allocator(kind: AllocKind) -> Allocator {
    match kind {
        AllocKind::Bump => Allocator { name: "bump", alloc: bump_alloc },
        AllocKind::FreeList => Allocator { name: "freelist", alloc: freelist_alloc },
    }
}
fn make_scheduler(kind: SchedKind) -> Scheduler {
    match kind {
        // NoSched 也给个缺省值占位，但 has_sched=false 时不会被用到。
        SchedKind::NoSched | SchedKind::Fifo => Scheduler { name: "fifo", run: fifo_run },
        SchedKind::Rr => Scheduler { name: "rr", run: rr_run },
    }
}
fn make_console() -> Console {
    Console { name: "plain", write: console_plain }
}

// ── 组装出来的内核：一组被选中的组件 + 它们的状态 ──────────────────

/// 「特性开关」配置：选哪个分配器/调度器、装不装调度器、要不要 syscall 边界。
struct KernelConfig {
    alloc_kind: AllocKind,
    sched_kind: SchedKind,
    syscall: bool,
}

struct Kernel {
    console: Console,
    console_len: usize,
    alloc: Allocator,
    astate: AllocState,
    sched: Scheduler,
    has_sched: bool,       // unikernel 单应用：根本不链调度器
    syscall_boundary: bool, // mono：app→OS 过 syscall（陷入）；uni：直链（无陷入）
    traps: u64,
}

// 系统服务号（mono 里是 syscall 号，uni 里只是 match 分支）。
const SVC_WRITE: u32 = 0;
const SVC_ALLOC: u32 = 1;

// ════════════════════════════════════════════════════════════════
// 学生填空区 ①：组装 / wiring —— 按 cfg 的特性开关选组件实现并接线
// ════════════════════════════════════════════════════════════════

/// 按配置组装一个内核：选分配器实现、选调度器实现（或不装）、设 syscall 边界。
/// 这就是「同一套组件、按特性拼出不同形态」的那道组装工序。
fn build_kernel(cfg: &KernelConfig) -> Kernel {
    let has_sched = cfg.sched_kind != SchedKind::NoSched;
    Kernel {
        console: make_console(),
        console_len: 0,
        alloc: make_allocator(cfg.alloc_kind),
        astate: AllocState::new(),
        sched: make_scheduler(cfg.sched_kind),
        has_sched,
        syscall_boundary: cfg.syscall,
        traps: 0,
    }
}

// ════════════════════════════════════════════════════════════════
// 学生填空区 ②：替换一个组件实现 —— freelist 分配器
// ════════════════════════════════════════════════════════════════

/// freelist 分配：固定槽分配——off = slots*SLOT，slots += 1；过大或满返回 -1。
/// 和 bump 策略不同，但同样满足「连续分配区间不重叠」这条 OS 级契约（故可热替换）。
fn freelist_alloc(s: &mut AllocState, n: usize) -> i64 {
    if n > SLOT || s.slots >= CAP {
        return -1;
    }
    let off = (s.slots as usize * SLOT) as i64;
    s.slots += 1;
    off
}

// ════════════════════════════════════════════════════════════════
// 测试 harness（给定，勿改）
// ════════════════════════════════════════════════════════════════

/// app→OS 的统一入口：mono 过 syscall 边界(traps+1)，uni 直链(不碰陷入)。
/// 不管哪种形态，最终都路由到**同一批**组件函数——差别只在那道陷入墙。
fn kcall(k: &mut Kernel, svc: u32, arg: u32) -> i64 {
    if k.syscall_boundary {
        k.traps += 1; // ← 陷入开销：unikernel 形态没有这一行
    }
    match svc {
        SVC_WRITE => (k.console.write)(&mut k.console_len, arg as usize),
        SVC_ALLOC => (k.alloc.alloc)(&mut k.astate, arg as usize),
        _ => -1,
    }
}

/// 校验一串 (off,n) 分配区间：都 >=0 且两两不重叠（OS 级不变量，与用哪个分配器无关）。
fn allocs_disjoint(regions: &[(i64, usize)]) -> bool {
    for &(off, _) in regions {
        if off < 0 {
            return false;
        }
    }
    for i in 0..regions.len() {
        for j in (i + 1)..regions.len() {
            let (a0, an) = regions[i];
            let (b0, bn) = regions[j];
            let a1 = a0 + an as i64;
            let b1 = b0 + bn as i64;
            if a0 < b1 && b0 < a1 {
                return false; // 重叠
            }
        }
    }
    true
}

/// 校验调度迹：每个任务恰好出现 burst 次、总步数对、无杂质 id（= 所有任务都跑完）。
fn sched_complete(tasks: &[Task], trace: &[u32]) -> bool {
    let total: u32 = tasks.iter().map(|t| t.burst).sum();
    if trace.len() as u32 != total {
        return false;
    }
    for t in tasks {
        let cnt = trace.iter().filter(|&&x| x == t.id).count() as u32;
        if cnt != t.burst {
            return false;
        }
    }
    for &x in trace {
        if !tasks.iter().any(|t| t.id == x) {
            return false;
        }
    }
    true
}

const DEMO_TASKS: [Task; 3] = [
    Task { id: 1, burst: 2 },
    Task { id: 2, burst: 3 },
    Task { id: 3, burst: 1 },
];

/// 判据 1：组件可独立使用、且可按特性选/换。
fn check_component() -> bool {
    let mut ok = true;

    // (a) 注册表按特性选实现：选谁就拿到谁（这就是「可替换」的接口面）。
    if make_allocator(AllocKind::Bump).name != "bump"
        || make_allocator(AllocKind::FreeList).name != "freelist"
        || make_scheduler(SchedKind::Fifo).name != "fifo"
        || make_scheduler(SchedKind::Rr).name != "rr"
        || make_console().name != "plain"
    {
        println!("COMPONENT_FAIL 注册表按特性选错了实现");
        ok = false;
    }

    // (b) 分配器组件独立可用：bump 紧凑分配 0,32，游标 48。
    let mut s = AllocState::new();
    let a0 = bump_alloc(&mut s, 32);
    let a1 = bump_alloc(&mut s, 16);
    if a0 != 0 || a1 != 32 || s.top != 48 {
        println!("COMPONENT_FAIL bump 分配器 a0={} a1={} top={} 应=(0,32,48)", a0, a1, s.top);
        ok = false;
    }

    // (c) 控制台组件独立可用：写 5 再写 3，len=8。
    let mut len = 0usize;
    let w = console_plain(&mut len, 5);
    console_plain(&mut len, 3);
    if w != 5 || len != 8 {
        println!("COMPONENT_FAIL console 组件 w={} len={} 应=(5,8)", w, len);
        ok = false;
    }

    // (d) 两个调度器组件都把所有任务跑完（仅顺序策略不同）。
    let mut tf = [0u32; 16];
    let nf = fifo_run(&DEMO_TASKS, &mut tf);
    let mut tr = [0u32; 16];
    let nr = rr_run(&DEMO_TASKS, &mut tr);
    if !sched_complete(&DEMO_TASKS, &tf[..nf]) || !sched_complete(&DEMO_TASKS, &tr[..nr]) {
        println!("COMPONENT_FAIL 调度器组件未把任务跑完");
        ok = false;
    }

    if ok {
        println!("COMPONENT_PASS");
    }
    ok
}

/// 判据 2：把组件组装成 UNIKERNEL 形态并跑通。
fn check_compose_uni() -> bool {
    let mut ok = true;
    let cfg = KernelConfig {
        alloc_kind: AllocKind::Bump,
        sched_kind: SchedKind::NoSched, // 单应用：不装调度器
        syscall: false,                 // app 直链 OS：无 syscall 边界
    };
    let mut k = build_kernel(&cfg);

    if k.has_sched {
        println!("COMPOSE_UNI_FAIL unikernel 形态不该装调度器");
        ok = false;
    }
    if k.syscall_boundary {
        println!("COMPOSE_UNI_FAIL unikernel 形态不该有 syscall 边界");
        ok = false;
    }

    // app 直接调 OS：写 6 字节 banner + 两次分配。
    let banner = kcall(&mut k, SVC_WRITE, 6);
    let p0 = kcall(&mut k, SVC_ALLOC, 32);
    let p1 = kcall(&mut k, SVC_ALLOC, 16);

    if banner != 6 || k.console_len != 6 {
        println!("COMPOSE_UNI_FAIL banner ret={} len={} 应=(6,6)", banner, k.console_len);
        ok = false;
    }
    if !allocs_disjoint(&[(p0, 32), (p1, 16)]) {
        println!("COMPOSE_UNI_FAIL 分配区重叠 p0={} p1={}", p0, p1);
        ok = false;
    }
    if k.traps != 0 {
        println!("TRAP_LEAK_FAIL unikernel 直链却产生了 {} 次陷入", k.traps);
        ok = false;
    }

    println!("FORM_UNI 组件={{console:{},alloc:{}}} sched=无 陷入={}", k.console.name, k.alloc.name, k.traps);
    if ok {
        println!("COMPOSE_UNI_PASS");
    }
    ok
}

/// 判据 3：把（更多）组件组装成 MONOLITHIC 形态并跑通。
fn check_compose_mono() -> bool {
    let mut ok = true;
    let cfg = KernelConfig {
        alloc_kind: AllocKind::Bump,
        sched_kind: SchedKind::Fifo, // 宏内核：装调度器，多进程
        syscall: true,               // app→OS 过 syscall 边界（有陷入）
    };
    let mut k = build_kernel(&cfg);

    if !k.has_sched {
        println!("COMPOSE_MONO_FAIL 宏内核形态应当装调度器");
        ok = false;
    }
    if !k.syscall_boundary {
        println!("COMPOSE_MONO_FAIL 宏内核形态应当有 syscall 边界");
        ok = false;
    }

    // 多个进程经 syscall 请求服务：4 次调用 → 4 次陷入。
    let svc = [(SVC_WRITE, 4u32), (SVC_ALLOC, 16), (SVC_WRITE, 2), (SVC_ALLOC, 8)];
    let mut regions = Vec::new();
    for &(s, a) in svc.iter() {
        let r = kcall(&mut k, s, a);
        if s == SVC_ALLOC {
            regions.push((r, a as usize));
        }
    }
    if k.traps != svc.len() as u64 {
        println!("COMPOSE_MONO_FAIL 陷入数={} 应={}", k.traps, svc.len());
        ok = false;
    }
    if !allocs_disjoint(&regions) {
        println!("COMPOSE_MONO_FAIL 分配区重叠 {:?}", regions);
        ok = false;
    }

    // 调度器把进程都跑完。
    let mut tr = [0u32; 16];
    let n = (k.sched.run)(&DEMO_TASKS, &mut tr);
    if !sched_complete(&DEMO_TASKS, &tr[..n]) {
        println!("COMPOSE_MONO_FAIL 调度器({})未把进程跑完", k.sched.name);
        ok = false;
    }

    println!("FORM_MONO 组件={{console:{},alloc:{},sched:{}}} 陷入={}", k.console.name, k.alloc.name, k.sched.name, k.traps);
    if ok {
        println!("COMPOSE_MONO_PASS");
    }
    ok
}

/// 判据 4：热替换一个组件实现，OS 仍工作（同源组件、换个拼法）。
fn check_swap() -> bool {
    let mut ok = true;
    // 在宏内核形态上，把分配器 bump→freelist、调度器 fifo→rr 一起换掉。
    let cfg = KernelConfig {
        alloc_kind: AllocKind::FreeList,
        sched_kind: SchedKind::Rr,
        syscall: true,
    };
    let mut k = build_kernel(&cfg);

    if k.alloc.name != "freelist" || k.sched.name != "rr" {
        println!("SWAP_FAIL 没换上替换实现 alloc={} sched={}", k.alloc.name, k.sched.name);
        ok = false;
    }

    // 同一份工作负载，换了组件后 OS 级不变量仍成立。
    let svc = [(SVC_WRITE, 4u32), (SVC_ALLOC, 16), (SVC_WRITE, 2), (SVC_ALLOC, 8)];
    let mut regions = Vec::new();
    for &(s, a) in svc.iter() {
        let r = kcall(&mut k, s, a);
        if s == SVC_ALLOC {
            if r < 0 {
                println!("SWAP_FAIL freelist 分配失败返回 {}", r);
                ok = false;
            }
            regions.push((r, a as usize));
        }
    }
    if !allocs_disjoint(&regions) {
        println!("SWAP_FAIL 换 freelist 后分配区重叠 {:?}", regions);
        ok = false;
    }
    if k.console_len != 6 {
        println!("SWAP_FAIL 换组件后控制台异常 len={} 应=6", k.console_len);
        ok = false;
    }

    let mut tr = [0u32; 16];
    let n = (k.sched.run)(&DEMO_TASKS, &mut tr);
    if !sched_complete(&DEMO_TASKS, &tr[..n]) {
        println!("SWAP_FAIL 换 rr 后未把进程跑完");
        ok = false;
    }

    println!("HOTSWAP alloc bump→{} sched fifo→{} OS 仍工作", k.alloc.name, k.sched.name);
    if ok {
        println!("SWAP_PASS");
    }
    ok
}

fn main() {
    let mut all = true;
    all &= check_component();
    all &= check_compose_uni();
    all &= check_compose_mono();
    all &= check_swap();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
