//! 共享内存（软件建模）—— Rust 参考解。
//! 母题：让两个映射指向同一块物理字节——一处写、另一处立刻看见。
//! 把这条心智模型从「进程↔进程」一路推到「设备↔OS（MMIO）」：本质相同，
//! 都是 **同一份物理字节 + 一套不踩脚的协调协议**。
//!
//! 五段逐题递进（学生只填标了「学生填」的函数体/两行；harness 勿改）：
//!   1) 页表别名      → ALIAS_PASS / ISOLATED_PASS
//!   2) mmap 共享/私有 → SHARED_PASS / PRIVATE_PASS
//!   3) 邮箱握手      → MAILBOX_PASS
//!   4) 共享环(结构体) → RING_PASS
//!   5) 共享环(MMIO 语义) → MMIO_SHM_PASS
//! 全过再打印 ALL_PASS。

// ───────────────────────── 1) 页表别名 ─────────────────────────
const PAGE_WORDS: usize = 4; // 一页 = 4 个字（够演示别名）
const MAGIC: u32 = 0xCAFE_F00D;

#[derive(Clone, Copy, Default)]
struct Pte {
    valid: bool,
    ppn: usize,
}

/// translate：已给（勿改）。va = vpn*PAGE_WORDS + off，查页表得物理字地址。
fn translate(pt: &[Pte], va: usize) -> Option<usize> {
    let vpn = va / PAGE_WORDS;
    let off = va % PAGE_WORDS;
    if vpn >= pt.len() || !pt[vpn].valid {
        return None;
    }
    Some(pt[vpn].ppn * PAGE_WORDS + off)
}

/// 学生填：把虚拟页 vpn 映射到物理页 ppn（写一个有效 PTE）。
fn map(pt: &mut [Pte], vpn: usize, ppn: usize) {
    pt[vpn] = Pte { valid: true, ppn };
}

// 物理内存读写助手（已给，经页表翻译）。
fn pwrite(phys: &mut [u32], pt: &[Pte], va: usize, val: u32) -> bool {
    match translate(pt, va) {
        Some(pa) => {
            phys[pa] = val;
            true
        }
        None => false,
    }
}
fn pread(phys: &[u32], pt: &[Pte], va: usize) -> Option<u32> {
    translate(pt, va).map(|pa| phys[pa])
}

fn sub_alias() -> bool {
    let mut phys = vec![0u32; 32 * PAGE_WORDS];
    let mut pt = vec![Pte::default(); 16];
    let ppn_shared = 7usize;
    let (vpn_a, vpn_b) = (2usize, 5usize);

    // 学生填：让 vpn_a、vpn_b 映射到同一物理页 ppn_shared（别名 = 两 PTE 同 PPN）。
    map(&mut pt, vpn_a, ppn_shared);
    map(&mut pt, vpn_b, ppn_shared);

    let va_a = vpn_a * PAGE_WORDS + 1; // 页内偏移相同，才落到同一物理字
    let va_b = vpn_b * PAGE_WORDS + 1;
    if !pwrite(&mut phys, &pt, va_a, MAGIC) {
        println!("ALIAS_FAIL 经 va_a 写入失败（map 没生效？）");
        return false;
    }
    match pread(&phys, &pt, va_b) {
        Some(v) if v == MAGIC => println!("ALIAS_PASS"),
        other => {
            println!("ALIAS_FAIL 经 va_b 读到 {:?}，期望 {:#x}", other, MAGIC);
            return false;
        }
    }

    // 对照组：不同 PPN → 互不可见（隔离）。
    let mut pt2 = vec![Pte::default(); 16];
    let mut phys2 = vec![0u32; 32 * PAGE_WORDS];
    map(&mut pt2, vpn_a, 1);
    map(&mut pt2, vpn_b, 9);
    pwrite(&mut phys2, &pt2, va_a, MAGIC);
    match pread(&phys2, &pt2, va_b) {
        Some(v) if v != MAGIC => {
            println!("ISOLATED_PASS");
            true
        }
        _ => {
            println!("ISOLATED_FAIL 不同物理页竟互相可见");
            false
        }
    }
}

// ─────────────────── 2) mmap：共享 vs 私有 ───────────────────
const SHARED: u32 = 0;
const PRIVATE: u32 = 1;

/// 一个「世界」：两个地址空间共用的物理帧池 + 命名段注册表。
struct World {
    phys: Vec<u32>,
    next_ppn: usize,
    reg: Vec<(u32, usize)>, // key -> ppn
}
impl World {
    fn new() -> Self {
        World {
            phys: vec![0u32; 64 * PAGE_WORDS],
            next_ppn: 0,
            reg: Vec::new(),
        }
    }
    /// 分配一块新（零）物理帧，返回其 ppn。
    fn frame_alloc(&mut self) -> usize {
        let p = self.next_ppn;
        self.next_ppn += 1;
        p
    }
}

/// 学生填：mmap 的两个分支。
fn do_mmap(w: &mut World, pt: &mut [Pte], vpn: usize, key: u32, flags: u32) {
    let ppn = if flags == SHARED {
        // TODO[a] MAP_SHARED：查命名段注册表，命中复用同一 ppn；未命中再 alloc 并登记。
        if let Some(&(_, p)) = w.reg.iter().find(|(k, _)| *k == key) {
            p
        } else {
            let p = w.frame_alloc();
            w.reg.push((key, p));
            p
        }
    } else {
        // ELSE[b] MAP_PRIVATE：永远分配一块新的匿名（零）帧。
        w.frame_alloc()
    };
    map(pt, vpn, ppn);
}

fn sub_mmap() -> bool {
    let mut w = World::new();
    let mut pt1 = vec![Pte::default(); 16]; // 地址空间 1（进程1）
    let mut pt2 = vec![Pte::default(); 16]; // 地址空间 2（进程2）
    let key = 0x55u32;
    let (vpn, va) = (3usize, 3 * PAGE_WORDS + 2);

    // AS1 以 SHARED 映射并写入。
    do_mmap(&mut w, &mut pt1, vpn, key, SHARED);
    if !pwrite(&mut w.phys, &pt1, va, MAGIC) {
        println!("SHARED_FAIL AS1 写入失败");
        return false;
    }
    // AS2 以同 key 的 SHARED 映射 → 应读到同一物理字。
    do_mmap(&mut w, &mut pt2, vpn, key, SHARED);
    match pread(&w.phys, &pt2, va) {
        Some(v) if v == MAGIC => println!("SHARED_PASS"),
        other => {
            println!("SHARED_FAIL AS2 未读到共享值，得 {:?}", other);
            return false;
        }
    }

    // AS2 改用 PRIVATE 映射另一页 → 只见自己的零页。
    let (vpn_p, va_p) = (4usize, 4 * PAGE_WORDS + 2);
    do_mmap(&mut w, &mut pt2, vpn_p, key, PRIVATE);
    match pread(&w.phys, &pt2, va_p) {
        Some(0) => {
            println!("PRIVATE_PASS");
            true
        }
        other => {
            println!("PRIVATE_FAIL 私有页应为零，得 {:?}", other);
            false
        }
    }
}

// ─────────────────── 3) 共享区上的握手（邮箱）───────────────────
const MB_N: usize = 3;

struct Mailbox {
    data: [u32; MB_N],
    ready: bool,
}
impl Mailbox {
    fn new() -> Self {
        Mailbox {
            data: [0; MB_N],
            ready: false,
        }
    }
}

/// 学生填：生产者一步——先写满 data，**最后**才置 ready。
fn producer_step(mb: &mut Mailbox, payload: &[u32; MB_N]) {
    for i in 0..MB_N {
        mb.data[i] = payload[i];
    }
    mb.ready = true; // 置位务必在写入之后
}

/// 学生填：消费者一步——仅当 ready 才拷贝；拷贝后清 ready。
fn consumer_step(mb: &mut Mailbox) -> Option<[u32; MB_N]> {
    if !mb.ready {
        return None;
    }
    let got = mb.data;
    mb.ready = false;
    Some(got)
}

fn sub_mailbox() -> bool {
    let mut mb = Mailbox::new();
    let rounds: [[u32; MB_N]; 3] = [[1, 2, 3], [10, 20, 30], [100, 200, 300]];
    let mut ok = true;
    for (i, pl) in rounds.iter().enumerate() {
        // 探针：未置位时消费者绝不能读到东西（无撕裂读）。
        if consumer_step(&mut mb).is_some() {
            println!("MAILBOX_FAIL 第{}轮：ready 未置位即被读取", i);
            ok = false;
        }
        producer_step(&mut mb, pl);
        match consumer_step(&mut mb) {
            Some(got) if &got == pl => {}
            other => {
                println!("MAILBOX_FAIL 第{}轮 收到 {:?} 期望 {:?}", i, other, pl);
                ok = false;
            }
        }
    }
    if ok {
        println!("MAILBOX_PASS");
    }
    ok
}

// ─────────────────── 4) 共享环（定长 ring mailbox）───────────────────
const CAP: usize = 4;

struct Ring {
    buf: [u32; CAP],
    head: usize,
    tail: usize,
    count: usize,
}
impl Ring {
    fn new() -> Self {
        Ring {
            buf: [0; CAP],
            head: 0,
            tail: 0,
            count: 0,
        }
    }
    fn avail(&self) -> bool {
        self.count > 0
    }
}

/// 学生填：入队（生产侧）。满则拒绝返回 false；否则写 tail、抬 tail、count+1。
fn ring_push(r: &mut Ring, x: u32) -> bool {
    if r.count == CAP {
        return false;
    }
    r.buf[r.tail] = x;
    r.tail = (r.tail + 1) % CAP;
    r.count += 1;
    true
}

/// 学生填：出队（消费侧）。空则 None；否则读 head、抬 head、count-1。
fn ring_pop(r: &mut Ring) -> Option<u32> {
    if r.count == 0 {
        return None;
    }
    let x = r.buf[r.head];
    r.head = (r.head + 1) % CAP;
    r.count -= 1;
    Some(x)
}

fn sub_ring() -> bool {
    let mut r = Ring::new();
    let mut ok = true;

    // 填满 CAP 个。
    for v in [11u32, 22, 33, 44] {
        if !ring_push(&mut r, v) {
            println!("RING_FAIL 入队 {} 失败", v);
            ok = false;
        }
    }
    // 满后再入队应被拒绝（不得覆盖）。
    if ring_push(&mut r, 55) {
        println!("RING_FAIL 满后竟接受入队（覆盖未排空数据）");
        ok = false;
    }
    // 按 FIFO 顺序排空。
    for exp in [11u32, 22, 33, 44] {
        match ring_pop(&mut r) {
            Some(v) if v == exp => {}
            other => {
                println!("RING_FAIL 出队 {:?} 期望 {}", other, exp);
                ok = false;
            }
        }
    }
    // 空队再出队应为 None。
    if ring_pop(&mut r).is_some() {
        println!("RING_FAIL 空队竟出队");
        ok = false;
    }
    // 环绕：head/tail 已推进，再来一轮验证 %CAP 回绕。
    for v in [61u32, 62, 63] {
        ring_push(&mut r, v);
    }
    for exp in [61u32, 62, 63] {
        match ring_pop(&mut r) {
            Some(v) if v == exp => {}
            other => {
                println!("RING_FAIL 环绕出队 {:?} 期望 {}", other, exp);
                ok = false;
            }
        }
    }
    if ok {
        println!("RING_PASS");
    }
    ok
}

// ─────────────────── 5) 同一环，换 MMIO 语义 ───────────────────
// 同一套 ring_push/ring_pop，换成「设备↔OS 的 MMIO 共享邮箱」框架：
// 设备侧 doorbell 写入 = push、抬 tail；OS 侧轮询 avail、MMIO 读 = pop、抬 head。
// 软硬同构：硬件 ring_mbox 喂的就是同一组向量。
fn sub_mmio() -> bool {
    let mut mbox = Ring::new();
    let stream = [0xD0u32, 0xD1, 0xD2, 0xD3, 0xD4]; // 5 项穿过深度=4 的环 → 必然环绕
    let mut produced = 0usize;
    let mut consumed: Vec<u32> = Vec::new();
    let mut guard = 0usize;
    while consumed.len() < stream.len() {
        guard += 1;
        if guard > 10_000 {
            println!("MMIO_SHM_FAIL 推进超时（push/pop 未实现？）");
            return false;
        }
        // 设备写一拍（满则等 OS 排空）。
        if produced < stream.len() && ring_push(&mut mbox, stream[produced]) {
            produced += 1;
        }
        // OS 轮询 avail，有就读一拍。
        if mbox.avail() {
            if let Some(v) = ring_pop(&mut mbox) {
                consumed.push(v);
            }
        }
    }
    if consumed != stream {
        println!("MMIO_SHM_FAIL 收到 {:?} 期望 {:?}", consumed, stream);
        return false;
    }
    println!("MMIO_SHM_PASS");
    true
}

fn main() {
    let mut all = true;
    all &= sub_alias();
    all &= sub_mmap();
    all &= sub_mailbox();
    all &= sub_ring();
    all &= sub_mmio();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
