//! I/O 多路复用：select / epoll / 边沿触发 / O(ready) 伸缩性 —— Rust。
//!
//! 心智模型（没吃过猪肉但见过猪跑）：N 个「fd」各有一个就绪态(可读/不可读)。
//! 谁想知道「哪些 fd 现在能读」？两条路：
//!   · select —— 把全部 fd 从头扫到尾，凡可读就收集。O(n)，fd 越多越亏。
//!   · epoll  —— 先 epoll_ctl 注册「兴趣集」，内核在 fd 变就绪的「那一刻」把它
//!               挂上「就绪链表」；epoll_wait 只遍历就绪链表，O(ready)。
//!
//! 四段递进：
//!   1. SELECT  —— 扫描全部 fd 找就绪集（对照基线，已给好）。
//!   2. EPOLL   —— 注册兴趣集；wait 只返回「已注册 且 就绪」的 fd，未注册的不返回。
//!   3. EDGE    —— 边沿触发 ET：不可读→可读只通知一次；对照水平触发 LT 持续通知。
//!   4. SCALE   —— 1 个就绪 / 1000 个 fd，epoll 只检视 O(ready)、select 检视 O(n)。
//!
//! 你只需填 2 处（标 TODO）：epoll_wait 的「只收集就绪兴趣 fd」与 ET 的「边沿判定」。
//! 下方测试 harness（向量 + 不变量 + PASS 打印）勿改。
#![allow(unused_variables, dead_code, unused_mut)]

// ── fd 的就绪态（这里只建模「可读」一种事件，够 get 到大意即可）──────
#[derive(Clone, Copy)]
struct Fd {
    readable: bool,
}

// ── 一个极简 epoll 实例 ──────────────────────────────────────────
//   interest[fd] = Some(et)  表示已注册兴趣；et=true 边沿触发(ET)，false 水平触发(LT)
//   ready        = 就绪链表（内核在「变就绪那一刻」把 fd 挂进来）
//   on_list      = 去重位图（一个 fd 不重复入链）
//   scan         = epoll_wait 检视过的 fd 数 —— 用来证明 O(ready)
struct Epoll {
    interest: Vec<Option<bool>>,
    ready: Vec<usize>,
    on_list: Vec<bool>,
    scan: u64,
}

impl Epoll {
    fn new(n: usize) -> Self {
        Epoll {
            interest: vec![None; n],
            ready: Vec::new(),
            on_list: vec![false; n],
            scan: 0,
        }
    }

    /// epoll_ctl(ADD)：把 fd 加入兴趣集；et=true 为边沿触发、false 为水平触发。
    fn ctl_add(&mut self, fd: usize, et: bool) {
        self.interest[fd] = Some(et);
    }

    /// epoll_ctl(DEL)：移出兴趣集。
    fn ctl_del(&mut self, fd: usize) {
        self.interest[fd] = None;
    }
}

// ════════════════════════════════════════════════════════════════
// 学生填空区：2 处
// ════════════════════════════════════════════════════════════════

/// 【填空 1 / ET 边沿判定】仅当「上一刻不可读、此刻可读」才算一次新事件(上升沿)。
/// 这正是 ET「不可读→可读只通知一次」的内核侧依据：只有沿才把 fd 挂上就绪链表。
fn edge_ready(prev: bool, cur: bool) -> bool {
    // TODO: 返回「上升沿」——上一刻不可读、此刻可读。
    //   HINT: !prev && cur
    //   注意：true→true 不算沿(ET 静默)，false→true 才算沿(ET 通知一次)。
    false // ← 占位
}

/// 【填空 2 / epoll_wait 收集】只遍历就绪链表，收集「已注册兴趣 且 当前确实可读」的 fd。
/// 未注册的 fd 即便可读也不收（兴趣集之外内核不关心）；已注册但此刻不可读的也不收。
fn epoll_wait(ep: &mut Epoll, fds: &[Fd]) -> Vec<usize> {
    let mut out = Vec::new();
    let mut keep = Vec::new();
    let pending = std::mem::take(&mut ep.ready);
    for fd in pending {
        ep.scan += 1; // 只数「就绪链表」长度 → O(ready)，这是 epoll 的命根
        ep.on_list[fd] = false;
        let registered = ep.interest[fd].is_some();
        let readable = fds[fd].readable;

        // TODO: 只把「registered && readable」的 fd 收进 out（out.push(fd)）。
        //   未注册的、或此刻不可读的，都不收。
        let _ = (registered, readable);

        // LT 重新武装：水平触发下只要「仍可读」就挂回链表，下次 wait 继续通知。
        // ET 不武装：报告完即摘除，要等下一次上升沿(edge_ready)才再上链。（已给好）
        if let Some(et) = ep.interest[fd] {
            if !et && readable {
                keep.push(fd);
                ep.on_list[fd] = true;
            }
        }
    }
    ep.ready = keep;
    out
}

// ════════════════════════════════════════════════════════════════
// 内核侧 / 对照实现（给定，勿改）
// ════════════════════════════════════════════════════════════════

/// 内核侧：把 fd 的可读态设为 val；若构成上升沿(edge_ready)且已注册兴趣，则把 fd
/// 挂上就绪链表（这一步就是「在变就绪那一刻」记账，省得 wait 时全表扫描）。
fn set_readable(fds: &mut [Fd], ep: &mut Epoll, fd: usize, val: bool) {
    let prev = fds[fd].readable;
    fds[fd].readable = val;
    if ep.interest[fd].is_some() && edge_ready(prev, val) && !ep.on_list[fd] {
        ep.ready.push(fd);
        ep.on_list[fd] = true;
    }
}

/// select：扫描全部 fd，凡可读即收集。复杂度 O(n)——fd 再多也得逐个看。
fn select_scan(fds: &[Fd], counter: &mut u64) -> Vec<usize> {
    let mut out = Vec::new();
    for (i, f) in fds.iter().enumerate() {
        *counter += 1; // 每个 fd 都看一遍
        if f.readable {
            out.push(i);
        }
    }
    out
}

// ════════════════════════════════════════════════════════════════
// 测试 harness（给定，勿改）
// ════════════════════════════════════════════════════════════════

fn check_select() -> bool {
    let mut ok = true;

    let mut fds = vec![Fd { readable: false }; 8];
    fds[1].readable = true;
    fds[4].readable = true;
    fds[7].readable = true;
    let mut cnt = 0u64;
    let r = select_scan(&fds, &mut cnt);
    if r != vec![1, 4, 7] {
        println!("SELECT_MISS 就绪集={:?} 应=[1,4,7]", r);
        ok = false;
    }
    if cnt != 8 {
        println!("SELECT_BAD 扫描计数={} 应=8(全表都得看)", cnt);
        ok = false;
    }

    // 空集：没有可读 fd
    let empty = vec![Fd { readable: false }; 3];
    let mut c2 = 0u64;
    let r2 = select_scan(&empty, &mut c2);
    if !r2.is_empty() {
        println!("SELECT_MISS 空集却返回 {:?}", r2);
        ok = false;
    }

    if ok {
        println!("SELECT_PASS");
    }
    ok
}

fn check_epoll() -> bool {
    let mut ok = true;
    let n = 8;
    let mut fds = vec![Fd { readable: false }; n];
    let mut ep = Epoll::new(n);

    // 注册兴趣集：fd 2、3、5（水平触发）
    ep.ctl_add(2, false);
    ep.ctl_add(3, false);
    ep.ctl_add(5, false);

    // 让 3、5 可读；外加一个「未注册」的 fd 6 也可读。
    set_readable(&mut fds, &mut ep, 3, true);
    set_readable(&mut fds, &mut ep, 5, true);
    set_readable(&mut fds, &mut ep, 6, true); // 未注册：epoll 不该返回它

    let mut r = epoll_wait(&mut ep, &fds);
    r.sort();
    if r != vec![3, 5] {
        println!("EPOLL_MISS 返回 {:?} 应=[3,5]（只返回已注册且就绪）", r);
        ok = false;
    }
    if r.contains(&6) {
        println!("EPOLL_BAD 未注册的 fd6 被错误返回");
        ok = false;
    }
    if r.contains(&2) {
        println!("EPOLL_BAD 已注册但不可读的 fd2 被错误返回");
        ok = false;
    }

    if ok {
        println!("EPOLL_PASS");
    }
    ok
}

fn check_edge() -> bool {
    let mut ok = true;
    let n = 2;
    let mut fds = vec![Fd { readable: false }; n];
    let mut ep = Epoll::new(n);
    ep.ctl_add(0, true); // fd0：边沿触发 ET
    ep.ctl_add(1, false); // fd1：水平触发 LT

    // (1) 首个上升沿：两者都不可读→可读，ET、LT 都应通知一次。
    set_readable(&mut fds, &mut ep, 0, true);
    set_readable(&mut fds, &mut ep, 1, true);
    let w1 = epoll_wait(&mut ep, &fds);
    if !(w1.contains(&0) && w1.contains(&1)) {
        println!("EDGE_MISS 首个上升沿 ET/LT 都应通知，得 {:?}", w1);
        ok = false;
    }

    // (2) 无新沿、仍可读：ET 静默；LT 继续通知（数据没读完就一直催）。
    let w2 = epoll_wait(&mut ep, &fds);
    if w2.contains(&0) {
        println!("EDGE_BAD ET 在无新沿时重复通知 {:?}", w2);
        ok = false;
    }
    if !w2.contains(&1) {
        println!("EDGE_MISS LT 在仍可读时应持续通知，得 {:?}", w2);
        ok = false;
    }

    // (3) 再写一次「可读」(true→true，不构成沿)：ET 仍静默；LT 仍通知。
    set_readable(&mut fds, &mut ep, 0, true);
    set_readable(&mut fds, &mut ep, 1, true);
    let w3 = epoll_wait(&mut ep, &fds);
    if w3.contains(&0) {
        println!("EDGE_BAD ET 把 true→true 误判为沿 {:?}", w3);
        ok = false;
    }
    if !w3.contains(&1) {
        println!("EDGE_MISS LT 持续通知失效，得 {:?}", w3);
        ok = false;
    }

    // (4) 先落沿(可读→不可读)再起沿(不可读→可读)：ET 又得到一次新沿，应再通知一次。
    set_readable(&mut fds, &mut ep, 0, false);
    set_readable(&mut fds, &mut ep, 1, false);
    set_readable(&mut fds, &mut ep, 0, true);
    set_readable(&mut fds, &mut ep, 1, true);
    let w4 = epoll_wait(&mut ep, &fds);
    if !w4.contains(&0) {
        println!("EDGE_MISS 新上升沿后 ET 应再通知一次，得 {:?}", w4);
        ok = false;
    }
    if !w4.contains(&1) {
        println!("EDGE_MISS LT 应通知，得 {:?}", w4);
        ok = false;
    }

    if ok {
        println!("EDGE_PASS");
    }
    ok
}

fn check_scale() -> bool {
    let mut ok = true;
    let n = 1000;
    let mut fds = vec![Fd { readable: false }; n];
    let mut ep = Epoll::new(n);
    for fd in 0..n {
        ep.ctl_add(fd, true); // 1000 个 fd 全部注册
    }

    // 只有 1 个 fd 就绪。
    let target = 617;
    set_readable(&mut fds, &mut ep, target, true);

    // epoll_wait：只遍历就绪链表（长度=1）。
    let before = ep.scan;
    let r = epoll_wait(&mut ep, &fds);
    let epoll_steps = ep.scan - before;
    if r != vec![target] {
        println!("SCALE_MISS epoll 返回 {:?} 应=[{}]", r, target);
        ok = false;
    }
    if epoll_steps != 1 {
        println!("SCALE_BAD epoll 检视了 {} 个 fd 应=1(只看就绪链表)", epoll_steps);
        ok = false;
    }

    // select：扫描全部 n 个。
    let mut sc = 0u64;
    let rs = select_scan(&fds, &mut sc);
    if rs != vec![target] {
        println!("SCALE_MISS select 返回 {:?} 应=[{}]", rs, target);
        ok = false;
    }
    if sc != n as u64 {
        println!("SCALE_BAD select 扫描了 {} 应={}", sc, n);
        ok = false;
    }

    // 关键对照：1 个就绪时 epoll 步数远小于 select。
    if epoll_steps >= sc {
        println!("SCALE_BAD epoll({}) 未优于 select({})", epoll_steps, sc);
        ok = false;
    }
    println!(
        "SCALE_INFO epoll检视={} vs select扫描={}（O(ready) 完胜 O(n)）",
        epoll_steps, sc
    );

    if ok {
        println!("SCALE_PASS");
    }
    ok
}

fn main() {
    let mut all = true;
    all &= check_select();
    all &= check_epoll();
    all &= check_edge();
    all &= check_scale();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
