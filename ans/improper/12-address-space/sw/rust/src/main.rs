//! 12 地址空间：软件 MMU 与「偷梁换柱」的稀疏映射 —— Rust 参考解。
//!
//! 一句话母题：用户程序眼里内存「无限大」，物理机却只有几帧。
//! 你写一层「软件 MMU」当中间人——只把真正用到的 vpn 偷偷接到几块真 ppn 上。
//!
//! 五段逐题递进（学生只填带「学生填」标注的纯函数；下方 harness 勿改）：
//!   E1 稀疏映射 · E2 直接映射区「拍卖行」· E3 多槽 SMP · E4 两级映射 · E5 SV39 草图
#![allow(dead_code, unused_variables, unused_mut)]

// ───────────────────────── 公共常量 ─────────────────────────
const PAGE_WORDS: usize = 512; // 一帧 = 512 个 u64 = 4096 字节
const NFRAMES: usize = 8; // 物理内存只有 8 帧（远小于虚拟空间）

const DIRECT_BASE: u64 = 0x8000_0000; // E2 直接映射窗口基址
const DIRECT_SIZE: u64 = 0x1000; // 窗口大小 = 1 页
const AUCTION_VA: u64 = DIRECT_BASE; // 拍卖行槽位（identity）
const PRIV_VA: u64 = 0x10_0000; // 私有虚拟地址（走翻译）

const SMP_BASE: u64 = 0x9000_0000; // E3 SMP 槽位基址
const SLOT: u64 = 8; // 每核一个槽（一个 u64）
const NHARTS: usize = 4;

// SV39 PTE 标志位
const PTE_V: u64 = 1 << 0;
const PTE_R: u64 = 1 << 1;
const PTE_W: u64 = 1 << 2;
const PTE_X: u64 = 1 << 3;
const PTE_U: u64 = 1 << 4;

// ═════════════════════ E1 · 软件 MMU 稀疏映射 ═════════════════════
struct Pt1 {
    ents: Vec<(u64, u64)>, // 扁平单级页表：(vpn, ppn)
}
impl Pt1 {
    fn new() -> Self {
        Pt1 { ents: Vec::new() }
    }
}

// ── 学生填（E1）──────────────────────────────────────────────
/// 记一条 vpn→ppn 映射。
fn map1(pt: &mut Pt1, vpn: u64, ppn: u64) {
    pt.ents.push((vpn, ppn));
}
/// 翻译：va = vpn(高位)|off(低 12 位)；查到映射则 pa=ppn<<12|off，否则 None。
fn translate(pt: &Pt1, va: u64) -> Option<u64> {
    let vpn = va >> 12;
    let off = va & 0xfff;
    for &(v, p) in &pt.ents {
        if v == vpn {
            return Some((p << 12) | off);
        }
    }
    None
}

// ── E1 harness（勿改）────────────────────────────────────────
struct World1 {
    mem: Vec<u64>,
    next: u64,
    pt: Pt1,
}
impl World1 {
    fn new() -> Self {
        World1 { mem: vec![0u64; NFRAMES * PAGE_WORDS], next: 0, pt: Pt1::new() }
    }
    fn alloc(&mut self) -> Option<u64> {
        if (self.next as usize) < NFRAMES {
            let p = self.next;
            self.next += 1;
            Some(p)
        } else {
            None
        }
    }
    fn pw(&mut self, pa: u64, val: u64) {
        let i = ((pa >> 12) as usize) * PAGE_WORDS + ((pa & 0xfff) >> 3) as usize;
        if i < self.mem.len() {
            self.mem[i] = val;
        }
    }
    fn pr(&self, pa: u64) -> u64 {
        let i = ((pa >> 12) as usize) * PAGE_WORDS + ((pa & 0xfff) >> 3) as usize;
        if i < self.mem.len() {
            self.mem[i]
        } else {
            0
        }
    }
}
/// 写 va：缺页即按需分配一帧（demand paging 雏形）。
fn store1(w: &mut World1, va: u64, val: u64) -> bool {
    let pa = match translate(&w.pt, va) {
        Some(pa) => pa,
        None => {
            let vpn = va >> 12;
            match w.alloc() {
                Some(ppn) => {
                    map1(&mut w.pt, vpn, ppn);
                    match translate(&w.pt, va) {
                        Some(pa) => pa,
                        None => return false,
                    }
                }
                None => return false,
            }
        }
    };
    w.pw(pa, val);
    true
}
fn load1(w: &World1, va: u64) -> Option<u64> {
    translate(&w.pt, va).map(|pa| w.pr(pa))
}

fn stage_e1() -> bool {
    let mut w = World1::new();
    let addrs = [0x0u64, 0x10_0000, 0x1000_0000_0000];
    let magic = [0xA1u64, 0xB2, 0xC3];
    let mut ok = true;
    for i in 0..3 {
        if !store1(&mut w, addrs[i], magic[i]) {
            println!("SPARSE_FAIL store va={:#x} 失败", addrs[i]);
            ok = false;
        }
    }
    for i in 0..3 {
        match load1(&w, addrs[i]) {
            Some(v) if v == magic[i] => {}
            other => {
                println!("SPARSE_FAIL va={:#x} got={:?} want={:#x}", addrs[i], other, magic[i]);
                ok = false;
            }
        }
    }
    if w.next > 3 {
        println!("SPARSE_FAIL 实占帧数={} 超过 3（巨址被物理铺开了？）", w.next);
        ok = false;
    }
    if ok {
        println!("SPARSE_PASS");
    }
    ok
}

// ═══════════════ E2 · 直接映射区「拍卖行」交换 ═══════════════
struct World2 {
    mem: Vec<u64>,
    next: u64,
    direct: [u64; PAGE_WORDS], // 直接映射窗口背后的「同一帧」
}
impl World2 {
    fn new() -> Self {
        World2 { mem: vec![0u64; NFRAMES * PAGE_WORDS], next: 0, direct: [0u64; PAGE_WORDS] }
    }
    fn alloc(&mut self) -> Option<u64> {
        if (self.next as usize) < NFRAMES {
            let p = self.next;
            self.next += 1;
            Some(p)
        } else {
            None
        }
    }
}
fn in_direct(va: u64) -> bool {
    va >= DIRECT_BASE && va < DIRECT_BASE + DIRECT_SIZE
}

// ── 学生填（E2）──────────────────────────────────────────────
/// 路由一个虚拟地址到物理地址。
/// 落在直接映射窗口 → identity（pa==va，跨空间/跨核共识的「拍卖行」）；
/// 否则走各空间私有页表翻译，缺页按需分配。
fn route(w: &mut World2, pt: &mut Pt1, va: u64) -> u64 {
    // TODO[a]: 直接映射窗口走 identity
    if in_direct(va) {
        return va;
    }
    // ELSE[b]: 虚拟段走翻译（缺页 demand alloc）
    match translate(pt, va) {
        Some(pa) => pa,
        None => {
            let vpn = va >> 12;
            if let Some(ppn) = w.alloc() {
                map1(pt, vpn, ppn);
                translate(pt, va).unwrap_or(u64::MAX)
            } else {
                u64::MAX
            }
        }
    }
}

// ── E2 harness（勿改）────────────────────────────────────────
fn phys_w2(w: &mut World2, pa: u64, val: u64) {
    if in_direct(pa) {
        let i = ((pa - DIRECT_BASE) >> 3) as usize;
        if i < PAGE_WORDS {
            w.direct[i] = val;
        }
    } else {
        let i = ((pa >> 12) as usize) * PAGE_WORDS + ((pa & 0xfff) >> 3) as usize;
        if i < w.mem.len() {
            w.mem[i] = val;
        }
    }
}
fn phys_r2(w: &World2, pa: u64) -> u64 {
    if in_direct(pa) {
        let i = ((pa - DIRECT_BASE) >> 3) as usize;
        if i < PAGE_WORDS {
            w.direct[i]
        } else {
            0
        }
    } else {
        let i = ((pa >> 12) as usize) * PAGE_WORDS + ((pa & 0xfff) >> 3) as usize;
        if i < w.mem.len() {
            w.mem[i]
        } else {
            0
        }
    }
}

fn stage_e2() -> bool {
    let mut w = World2::new();
    let mut a = Pt1::new();
    let mut b = Pt1::new();
    let mut ok = true;

    // 1) 拍卖行：A 出价、B 读到同值（共享）
    let bid = 0xB1D5u64;
    let pa = route(&mut w, &mut a, AUCTION_VA);
    phys_w2(&mut w, pa, bid);
    let pa = route(&mut w, &mut b, AUCTION_VA);
    let got = phys_r2(&w, pa);
    if got != bid {
        println!("DIRECT_FAIL B 在拍卖行读到 {:#x} 应为 {:#x}", got, bid);
        ok = false;
    } else {
        println!("DIRECT_PASS");
    }

    // 2) 私有区：A 写秘密，B 同号 va 读不到（隔离）
    let secret = 0x5ECu64;
    let pa = route(&mut w, &mut a, PRIV_VA);
    phys_w2(&mut w, pa, secret);
    let pa = route(&mut w, &mut b, PRIV_VA);
    let leak = phys_r2(&w, pa);
    if leak == secret {
        println!("EXCHANGE_FAIL 私有区串扰：B 读到了 A 的秘密 {:#x}", leak);
        ok = false;
    } else {
        println!("EXCHANGE_PASS");
    }
    ok
}

// ═══════════════════ E3 · 多槽位 SMP 拍卖 ═══════════════════
// ── 学生填（E3）──────────────────────────────────────────────
/// 第 hart 号核的私有槽位地址 = 基址 + hart*SLOT。
fn slot_addr(hart: usize) -> u64 {
    SMP_BASE + (hart as u64) * SLOT
}
/// 屏障后由 0 号核归约：把各核私有槽位求和（无锁，因互不写同一槽）。
fn reduce_slots(slots: &[u64]) -> u64 {
    slots.iter().copied().sum()
}

// ── E3 harness（勿改）────────────────────────────────────────
fn stage_e3() -> bool {
    let mut slots = [0u64; NHARTS];
    let mut ok = true;
    let bid = |h: usize| ((h + 1) * 100) as u64;

    // 各核写各自槽位（无竞争）
    for h in 0..NHARTS {
        let va = slot_addr(h);
        let d = va.wrapping_sub(SMP_BASE);
        let idx = (d / SLOT) as usize;
        if d % SLOT == 0 && idx < NHARTS {
            slots[idx] = bid(h);
        } else {
            println!("SLOTS_FAIL hart{} 槽地址非法 va={:#x}", h, va);
            ok = false;
        }
    }
    for h in 0..NHARTS {
        if slots[h] != bid(h) {
            println!("SLOTS_FAIL slot{}={} 应={}", h, slots[h], bid(h));
            ok = false;
        }
    }
    if ok {
        println!("SLOTS_PASS");
    }
    let want: u64 = (1..=NHARTS as u64).map(|x| x * 100).sum();
    let got = reduce_slots(&slots);
    if got != want {
        println!("SMP_FAIL 归约和={} 应={}", got, want);
        ok = false;
    } else {
        println!("SMP_PASS");
    }
    ok
}

// ═══════════════════ E4 · 两级地址映射 ═══════════════════
// va = l1(10) | l2(10) | off(12)
struct World4 {
    pd: Vec<i64>,         // 一级目录：-1 = 空，否则 = 二级表下标
    tables: Vec<Vec<u64>>, // 二级表池：每张 1024 项，pte = ppn+1（0 = 无效）
    mem: Vec<u64>,
    next: u64,
}
impl World4 {
    fn new() -> Self {
        World4 { pd: vec![-1i64; 1024], tables: Vec::new(), mem: vec![0u64; NFRAMES * PAGE_WORDS], next: 0 }
    }
    fn alloc(&mut self) -> Option<u64> {
        if (self.next as usize) < NFRAMES {
            let p = self.next;
            self.next += 1;
            Some(p)
        } else {
            None
        }
    }
    fn pw(&mut self, pa: u64, val: u64) {
        let i = ((pa >> 12) as usize) * PAGE_WORDS + ((pa & 0xfff) >> 3) as usize;
        if i < self.mem.len() {
            self.mem[i] = val;
        }
    }
    fn pr(&self, pa: u64) -> u64 {
        let i = ((pa >> 12) as usize) * PAGE_WORDS + ((pa & 0xfff) >> 3) as usize;
        if i < self.mem.len() {
            self.mem[i]
        } else {
            0
        }
    }
}

// ── 学生填（E4）──────────────────────────────────────────────
/// 建立 va→pa 映射：按需新建二级表（稀疏：用到哪张建哪张）。
fn map2(w: &mut World4, va: u64, pa: u64) {
    let l1 = ((va >> 22) & 0x3ff) as usize;
    let l2 = ((va >> 12) & 0x3ff) as usize;
    if w.pd[l1] < 0 {
        w.pd[l1] = w.tables.len() as i64;
        w.tables.push(vec![0u64; 1024]);
    }
    let t = w.pd[l1] as usize;
    w.tables[t][l2] = (pa >> 12) + 1;
}
/// 两级 walk：pde=PD[l1] → pt=tables[pde] → pte=pt[l2] → pa=ppn<<12|off。
fn walk2(w: &World4, va: u64) -> Option<u64> {
    let l1 = ((va >> 22) & 0x3ff) as usize;
    let l2 = ((va >> 12) & 0x3ff) as usize;
    let off = va & 0xfff;
    if w.pd[l1] < 0 {
        return None;
    }
    let t = w.pd[l1] as usize;
    let e = w.tables[t][l2];
    if e == 0 {
        return None;
    }
    Some(((e - 1) << 12) | off)
}

// ── E4 harness（勿改）────────────────────────────────────────
fn stage_e4() -> bool {
    let mut w = World4::new();
    let mut ok = true;
    let vas = [0x0000_1000u64, 0x0040_1000, 0x0080_2000]; // 三个不同 l1 的稀疏 va
    let magic = [0x111u64, 0x222, 0x333];
    for i in 0..3 {
        if let Some(ppn) = w.alloc() {
            let pa = ppn << 12;
            map2(&mut w, vas[i], pa);
        } else {
            println!("WALK_FAIL 帧不足");
            ok = false;
        }
    }
    for i in 0..3 {
        match walk2(&w, vas[i]) {
            Some(pa) => w.pw(pa, magic[i]),
            None => {
                println!("WALK_FAIL va={:#x} 未命中", vas[i]);
                ok = false;
            }
        }
    }
    for i in 0..3 {
        match walk2(&w, vas[i]) {
            Some(pa) => {
                if w.pr(pa) != magic[i] {
                    println!("WALK_FAIL va={:#x} 读回错", vas[i]);
                    ok = false;
                }
            }
            None => {
                println!("WALK_FAIL va={:#x} 未命中", vas[i]);
                ok = false;
            }
        }
    }
    if ok {
        println!("WALK_PASS");
    }
    let nt = w.tables.len();
    if nt > 3 {
        println!("TWOLEVEL_FAIL 二级表数={} 超过 3（稀疏失效）", nt);
        ok = false;
    } else if ok {
        println!("TWOLEVEL_PASS");
    }
    ok
}

// ═══════════════════ E5 · SV39 三级 walk 草图 ═══════════════════
// va39 = vpn2(9) | vpn1(9) | vpn0(9) | off(12)
// 非叶 PTE：仅 V，ppn 字段存「下一张表下标」；叶 PTE：V|R|W|…，ppn 字段存物理页号。
struct Sv39 {
    tabs: Vec<Vec<u64>>, // 表池，tabs[0] = 根表
    mem: Vec<u64>,
}
fn sv39_build() -> (Sv39, u64, u64) {
    let mut s = Sv39 { tabs: Vec::new(), mem: vec![0u64; NFRAMES * PAGE_WORDS] };
    s.tabs.push(vec![0u64; 512]); // idx0：根
    let va: u64 = 0x1_2345_6000; // 落在 39 位空间内
    let vpn2 = ((va >> 30) & 0x1ff) as usize;
    let vpn1 = ((va >> 21) & 0x1ff) as usize;
    let vpn0 = ((va >> 12) & 0x1ff) as usize;
    let i1 = s.tabs.len();
    s.tabs.push(vec![0u64; 512]);
    s.tabs[0][vpn2] = ((i1 as u64) << 10) | PTE_V; // 非叶
    let i2 = s.tabs.len();
    s.tabs.push(vec![0u64; 512]);
    s.tabs[i1][vpn1] = ((i2 as u64) << 10) | PTE_V; // 非叶
    let ppn_leaf: u64 = 3;
    s.tabs[i2][vpn0] = (ppn_leaf << 10) | PTE_V | PTE_R | PTE_W | PTE_U; // 叶
    let magic = 0x5F39_ABCDu64;
    let i = (ppn_leaf as usize) * PAGE_WORDS;
    s.mem[i] = magic;
    (s, va, magic)
}

// ── 学生填（E5）──────────────────────────────────────────────
/// 三级 walk：从根表逐级取 PTE，检查 V；遇到带 R/W/X 的叶 PTE 即解析出 (pa, flags)。
fn sv39_walk(s: &Sv39, va: u64) -> Option<(u64, u64)> {
    let off = va & 0xfff;
    let vpn = [
        ((va >> 12) & 0x1ff) as usize,
        ((va >> 21) & 0x1ff) as usize,
        ((va >> 30) & 0x1ff) as usize,
    ];
    let mut t = 0usize; // 从根表开始
    for level in (0..3).rev() {
        let pte = s.tabs[t][vpn[level]];
        if pte & PTE_V == 0 {
            return None; // 无效项
        }
        let rwx = pte & (PTE_R | PTE_W | PTE_X);
        if rwx != 0 {
            let ppn = pte >> 10;
            return Some(((ppn << 12) | off, pte & 0x1f)); // 叶
        }
        t = (pte >> 10) as usize; // 非叶：下探
    }
    None
}

// ── E5 harness（勿改）────────────────────────────────────────
fn stage_e5() -> bool {
    let (s, va, magic) = sv39_build();
    let mut ok = true;
    match sv39_walk(&s, va) {
        Some((pa, flags)) => {
            let i = ((pa >> 12) as usize) * PAGE_WORDS + ((pa & 0xfff) >> 3) as usize;
            let val = if i < s.mem.len() { s.mem[i] } else { 0 };
            if val != magic {
                println!("SV39_FAIL 读回 {:#x} 应 {:#x}", val, magic);
                ok = false;
            }
            if flags & PTE_V == 0 || flags & PTE_R == 0 || flags & PTE_W == 0 {
                println!("SV39_FAIL 叶 PTE 标志位不全 flags={:#x}", flags);
                ok = false;
            }
        }
        None => {
            println!("SV39_FAIL walk 未命中");
            ok = false;
        }
    }
    if ok {
        println!("SV39_PASS");
    }
    ok
}

fn main() {
    let mut all = true;
    all &= stage_e1();
    all &= stage_e2();
    all &= stage_e3();
    all &= stage_e4();
    all &= stage_e5();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
