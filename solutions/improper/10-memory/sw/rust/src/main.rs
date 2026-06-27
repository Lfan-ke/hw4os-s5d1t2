//! 内存管理：分层、Swap 与统一地址空间 —— Rust 参考解。
//!
//! 两块"设备"= 两个软件数组：
//!   FAST —— 小、快（每次访问代价 +1）、断电即失（reboot 清零）。
//!   SLOW —— 大、慢（每次访问代价 +10）、断电不丢（reboot 保留）。
//!
//! 同样两块设备，按三种"想要"拼出三种内存系统：
//!   场景一（要速度）：工作集放 FAST 上算，结果持久回 SLOW。
//!   场景二（要容量+持久）：小 FAST 当帧、SLOW 当 swap，缺页换入/换出+脏页回写。
//!   场景三（只要够大）：两块焊成一条平坦大内存，线性地址译码到 (dev, off)。
//!
//! 你只需填 5 处「核心逻辑」函数体；下方测试 harness（含校验与 *_PASS 打印）勿改。
#![allow(dead_code)]
use std::collections::VecDeque;

const FAST_COST: u64 = 1; // 快设备：每次块访问代价 +1
const SLOW_COST: u64 = 10; // 慢设备：每次块访问代价 +10（分层成立）

/// 一块 RAM 块设备：`data` 是块数组（1 块 = 1 个 u64），`cost` 是累计访问代价计数器。
struct Dev {
    data: Vec<u64>,
    per_access: u64,
    cost: u64,
}

impl Dev {
    fn new(blocks: usize, per_access: u64) -> Dev {
        Dev { data: vec![0; blocks], per_access, cost: 0 }
    }
    fn blocks(&self) -> usize {
        self.data.len()
    }
    /// 断电：仅 FAST 调用（易失），SLOW 不调用（持久）。
    fn power_cycle(&mut self) {
        for x in self.data.iter_mut() {
            *x = 0;
        }
    }
}

// ════════════════════════════════════════════════════════════════════
//  填空区 ①：设备抽象（10.1）——统一块读写接口
// ════════════════════════════════════════════════════════════════════

/// 读一块：累加访问代价，返回该块内容。
fn blk_read(dev: &mut Dev, blk: usize) -> u64 {
    dev.cost += dev.per_access;
    dev.data[blk]
}

/// 写一块：累加访问代价，写入该块。
fn blk_write(dev: &mut Dev, blk: usize, val: u64) {
    dev.cost += dev.per_access;
    dev.data[blk] = val;
}

// ════════════════════════════════════════════════════════════════════
//  填空区 ②：场景一（10.2）——小内存 + 大存储（要速度）
// ════════════════════════════════════════════════════════════════════

/// 搬入：把 SLOW 上 [0,n) 的数据集拷到 FAST 的 [0,n)。
fn stage_in(fast: &mut Dev, slow: &mut Dev, n: usize) {
    for i in 0..n {
        let v = blk_read(slow, i);
        blk_write(fast, i, v);
    }
}

/// 搬出：把 FAST 上 [0,n) 的结果写回 SLOW 的 [0,n)（持久化）。
fn stage_out(fast: &mut Dev, slow: &mut Dev, n: usize) {
    for i in 0..n {
        let v = blk_read(fast, i);
        blk_write(slow, i, v);
    }
}

// ════════════════════════════════════════════════════════════════════
//  填空区 ③④：场景二（10.3 + 10.4）——建 Swap
// ════════════════════════════════════════════════════════════════════

#[derive(Clone, Copy)]
struct Pte {
    present: bool,
    frame: usize,
    dirty: bool,
}

/// 单级页表 + 软件 MMU 垫片：FAST 当物理帧，SLOW 当 swap（槽号 = vpn）。
struct Pager {
    ptes: Vec<Pte>,                 // 按 vpn 索引
    owner: Vec<Option<usize>>,      // frame -> 占用它的 vpn
    free: Vec<usize>,               // 空闲帧栈
    fifo: VecDeque<usize>,          // 已占用帧的换出顺序（FIFO）
    fast: Dev,                      // 物理帧：frame i = fast.data[i]
    slow: Dev,                      // swap：slot vpn = slow.data[vpn]
}

impl Pager {
    fn new(num_frames: usize, num_pages: usize) -> Pager {
        Pager {
            ptes: vec![Pte { present: false, frame: 0, dirty: false }; num_pages],
            owner: vec![None; num_frames],
            free: (0..num_frames).rev().collect(),
            fifo: VecDeque::new(),
            fast: Dev::new(num_frames, FAST_COST),
            slow: Dev::new(num_pages, SLOW_COST),
        }
    }

    /// 缺页路径核心：返回 vpn 当前驻留的帧号。
    ///   命中（present）：直接返回帧号。
    ///   缺页：若有空闲帧直接用；否则按 FIFO 选 victim，victim 脏则回写其 swap 槽，
    ///         腾出帧；再从 swap 槽 vpn 把该页读入帧，置 present/frame，清 dirty。
    fn translate(&mut self, vpn: usize) -> usize {
        // 命中
        if self.ptes[vpn].present {
            return self.ptes[vpn].frame;
        }
        // 取一个可用帧：优先空闲，否则换出 victim
        let frame = if let Some(f) = self.free.pop() {
            f
        } else {
            // FIFO 选 victim 帧
            // TODO[a] FIFO：从队首取最早进入的帧作为 victim。
            // ELSE[b] Clock：可改用 ref 位 + 指针的二次机会算法（此处用 FIFO）。
            let victim = self.fifo.pop_front().unwrap();
            let vvpn = self.owner[victim].unwrap();
            if self.ptes[vvpn].dirty {
                // 脏页回写 swap（page-out）
                let val = blk_read(&mut self.fast, victim);
                blk_write(&mut self.slow, vvpn, val);
            }
            self.ptes[vvpn].present = false;
            self.owner[victim] = None;
            victim
        };
        // 换入目标页（page-in）
        let val = blk_read(&mut self.slow, vpn);
        blk_write(&mut self.fast, frame, val);
        self.ptes[vpn] = Pte { present: true, frame, dirty: false };
        self.owner[frame] = Some(vpn);
        self.fifo.push_back(frame);
        frame
    }

    /// 同步：退出前把所有"驻留且脏"的页刷回 SLOW，保证持久。
    fn sync_all(&mut self) {
        for vpn in 0..self.ptes.len() {
            if self.ptes[vpn].present && self.ptes[vpn].dirty {
                let f = self.ptes[vpn].frame;
                let val = blk_read(&mut self.fast, f);
                blk_write(&mut self.slow, vpn, val);
                self.ptes[vpn].dirty = false;
            }
        }
    }
}

// ════════════════════════════════════════════════════════════════════
//  填空区 ⑤：场景三（10.5）——两块都当内存：平坦大内存
// ════════════════════════════════════════════════════════════════════

/// 线性地址译码：la < fast_size 落 FAST（dev=0），其余落 SLOW（dev=1）。
/// 返回 (dev, off)：dev 0=FAST 1=SLOW；off 是设备内块偏移。
fn addr_route(la: usize, fast_size: usize) -> (u32, usize) {
    if la < fast_size {
        (0, la)
    } else {
        (1, la - fast_size)
    }
}

// ═══════════════════════ 以下为测试 harness（勿改）═══════════════════════

// 读/写包装：经页表把 vpn 访问落到帧。read 不改脏位，write 置脏位。
fn pg_read(p: &mut Pager, vpn: usize) -> u64 {
    let f = p.translate(vpn);
    blk_read(&mut p.fast, f)
}
fn pg_write(p: &mut Pager, vpn: usize, val: u64) {
    let f = p.translate(vpn);
    blk_write(&mut p.fast, f, val);
    p.ptes[vpn].dirty = true;
}

fn refv(vpn: usize) -> u64 {
    0xA000 + vpn as u64
}
fn newv(vpn: usize) -> u64 {
    0xB000 + vpn as u64
}

// ── 10.1 设备探测 ──
fn test_dev_probe() -> bool {
    let mut fast = Dev::new(8, FAST_COST);
    let mut slow = Dev::new(64, SLOW_COST);
    let mut ok = true;

    // 容量读数
    if fast.blocks() != 8 || slow.blocks() != 64 {
        println!("DEV_PROBE_FAIL 容量错: fast={} slow={}", fast.blocks(), slow.blocks());
        ok = false;
    }
    // 读写自检：两块设备各写一个值再读回
    blk_write(&mut fast, 3, 0x1234);
    blk_write(&mut slow, 40, 0x5678);
    if blk_read(&mut fast, 3) != 0x1234 || blk_read(&mut slow, 40) != 0x5678 {
        println!("DEV_PROBE_FAIL 块读写不一致");
        ok = false;
    }
    // 分层成立：等量访问下 FAST 代价应小于 SLOW
    let mut fa = Dev::new(8, FAST_COST);
    let mut sl = Dev::new(8, SLOW_COST);
    for i in 0..8 {
        blk_write(&mut fa, i, i as u64);
        blk_write(&mut sl, i, i as u64);
    }
    if !(fa.cost < sl.cost) {
        println!("DEV_PROBE_FAIL 分层不成立 cost(FAST)={} cost(SLOW)={}", fa.cost, sl.cost);
        ok = false;
    }
    if ok {
        println!("DEV_PROBE_PASS");
    }
    ok
}

// ── 10.2 场景一：小内存 + 大存储 ──
fn test_scenario_a() -> bool {
    const N: usize = 6;
    let mut fast = Dev::new(8, FAST_COST);
    let mut slow = Dev::new(64, SLOW_COST);
    // 数据集放在慢设备
    for i in 0..N {
        slow.data[i] = (i as u64) + 1;
    }
    let mut ok = true;

    // 工作集搬入快设备
    stage_in(&mut fast, &mut slow, N);
    // 计算只在快设备上进行（给定）：平方
    let slow_cost_before = slow.cost;
    for i in 0..N {
        let v = blk_read(&mut fast, i);
        blk_write(&mut fast, i, v * v);
    }
    // 证明"全程在快设备上算"：计算阶段不应触碰慢设备
    if slow.cost != slow_cost_before {
        println!("SCENARIO_A_FAIL 计算阶段触碰了慢设备");
        ok = false;
    }
    // 结果写回慢设备（持久化）
    stage_out(&mut fast, &mut slow, N);

    // "重启"：快设备断电清零；慢设备保留
    fast.power_cycle();

    // 二次运行：只从慢设备读回，校验持久化的平方结果
    for i in 0..N {
        let want = ((i as u64) + 1) * ((i as u64) + 1);
        let got = blk_read(&mut slow, i);
        if got != want {
            println!("SCENARIO_A_FAIL slot{} got={} want={}", i, got, want);
            ok = false;
        }
    }
    if ok {
        println!("SCENARIO_A_PASS");
    }
    ok
}

// ── 10.3 场景二（一）：换入，帧充足 ──
fn test_pagein() -> bool {
    // 4 帧、8 页；本测试只访问 ≤4 个不同页，不触发换出
    let mut p = Pager::new(4, 8);
    for vpn in 0..8 {
        p.slow.data[vpn] = refv(vpn);
    }
    let seq = [0usize, 1, 2, 3, 0, 2, 1, 3];
    let mut ok = true;
    for &vpn in seq.iter() {
        let got = pg_read(&mut p, vpn);
        if got != refv(vpn) {
            println!("PAGEIN_FAIL vpn={} got={:#x} want={:#x}", vpn, got, refv(vpn));
            ok = false;
        }
    }
    if ok {
        println!("PAGEIN_PASS");
    }
    ok
}

// ── 10.4 场景二（二）：换出/回写 + 同步持久化，工作集 >> 帧数 ──
fn test_swapout_sync() -> bool {
    const FRAMES: usize = 4;
    const PAGES: usize = 8; // 工作集是帧数的 2 倍
    let mut ok = true;

    // —— 第一次运行 ——
    let mut p = Pager::new(FRAMES, PAGES);
    for vpn in 0..PAGES {
        p.slow.data[vpn] = refv(vpn);
    }
    // 写一遍：每页改成 newv（产生脏页，远多于帧数 → 必然换出+回写）
    for vpn in 0..PAGES {
        pg_write(&mut p, vpn, newv(vpn));
    }
    // 再读一遍：跨越多轮换入/换出，仍须读到 newv
    for vpn in 0..PAGES {
        let got = pg_read(&mut p, vpn);
        if got != newv(vpn) {
            println!("SWAPOUT_FAIL vpn={} got={:#x} want={:#x}", vpn, got, newv(vpn));
            ok = false;
        }
    }
    if ok {
        println!("SWAPOUT_PASS");
    }

    // 退出前同步所有脏页到慢设备
    p.sync_all();
    // 把 swap（慢设备）内容快照出来，模拟掉电后慢设备仍在、内存清空
    let swap_snapshot = p.slow.data.clone();

    // —— 二次运行（reboot）：只从慢设备恢复，校验一致 ——
    let mut ok2 = true;
    for vpn in 0..PAGES {
        if swap_snapshot[vpn] != newv(vpn) {
            println!("SYNC_FAIL vpn={} swap={:#x} want={:#x}", vpn, swap_snapshot[vpn], newv(vpn));
            ok2 = false;
        }
    }
    if ok2 {
        println!("SYNC_PASS");
    }
    ok && ok2
}

// ── 10.5 场景三：平坦大内存（跨两设备的统一地址空间）──
fn test_unified() -> bool {
    const FAST_SZ: usize = 8;
    const SLOW_SZ: usize = 16;
    let total = FAST_SZ + SLOW_SZ;
    let mut fast = Dev::new(FAST_SZ, FAST_COST);
    let mut slow = Dev::new(SLOW_SZ, SLOW_COST);
    let mut ok = true;

    let val = |la: usize| (la as u64) * 3 + 7;

    // 写满整条平坦大内存（跨越 FAST/SLOW 边界）
    for la in 0..total {
        let (dev, off) = addr_route(la, FAST_SZ);
        match dev {
            0 => blk_write(&mut fast, off, val(la)),
            1 => blk_write(&mut slow, off, val(la)),
            _ => {
                println!("UNIFIED_FAIL la={} 非法 dev={}", la, dev);
                ok = false;
            }
        }
    }
    // 读回校验
    for la in 0..total {
        let (dev, off) = addr_route(la, FAST_SZ);
        let got = match dev {
            0 => blk_read(&mut fast, off),
            _ => blk_read(&mut slow, off),
        };
        if got != val(la) {
            println!("UNIFIED_FAIL la={} dev={} off={} got={} want={}", la, dev, off, got, val(la));
            ok = false;
        }
    }
    // 边界两侧落点必须正确：la=7→FAST off7；la=8→SLOW off0
    let (d7, o7) = addr_route(7, FAST_SZ);
    let (d8, o8) = addr_route(8, FAST_SZ);
    if !(d7 == 0 && o7 == 7 && d8 == 1 && o8 == 0) {
        println!("UNIFIED_FAIL 边界译码错: la7->({},{}) la8->({},{})", d7, o7, d8, o8);
        ok = false;
    }
    if ok {
        println!("UNIFIED_PASS");
    }
    ok
}

fn main() {
    let mut all = true;
    all &= test_dev_probe();
    all &= test_scenario_a();
    all &= test_pagein();
    all &= test_swapout_sync();
    all &= test_unified();

    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
