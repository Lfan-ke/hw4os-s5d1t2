//! 外核（Exokernel）形态认知 demo —— Rust 参考解。
//!
//! 本质权衡：内核只做「安全多路复用裸资源 + 保护检查」，**不强加抽象**。
//!   - exo 内核只发资源句柄：exo_alloc 把一段不重叠的块发给某个 owner；
//!     越界 / 重叠的请求一律拒绝。内核当「裁判」（裁定谁能用哪块），
//!     不当「独裁者」（不规定这些块该被当成「文件 / 堆 / 页表」用）。
//!   - 抽象交给应用自带的 libOS：libA 在自己拿到的块上建「顺序布局」文件系统，
//!     libB 在它自己的块上建「分块（scatter）布局」文件系统——
//!     同一块裸磁盘、同一套裸资源，长出两种完全不同的文件抽象，且都能正确读写。
//!
//! 对应真实系统：MIT 6.828 jos / Aegis(Engler'95) / Xok。jos 的 sys_page_alloc
//! 只把物理页 map 给你，不提供 malloc / mmap；fs 抽象由用户态 LibOS 实现。
//!
//! 学生只需填两处：① exo_alloc 的边界 / 重叠校验；② libB 的分块布局策略。
//! 下方测试 harness（向量 + 不变量 + PASS 打印）勿改。
#![allow(dead_code)]

// ── 裸资源模型：一块「磁盘」= NBLK 个块，每块 BS 字节 ────────────
const NBLK: usize = 16;
const BS: usize = 4;
const FREE: i32 = -1; // owner 表中 -1 表示该块空闲

// 整个系统共享一块物理磁盘；exo 内核把它的「块」分给不同 libOS。
static mut DISK: [u8; NBLK * BS] = [0; NBLK * BS];

// ════════════════════════════════════════════════════════════════
// 学生填空区 ①：exo 内核——安全多路复用 + 保护检查（不含任何抽象）
// ════════════════════════════════════════════════════════════════

/// exo 内核的唯一职责：把块区间 [start, start+len) 发给 `who`。
///
/// 裁判规则（仅校验，不解释用途）：
///   - 越界：start+len 必须 <= NBLK（且 len>0），否则拒绝。
///   - 重叠：区间内每个块都必须当前空闲（owner==FREE），否则拒绝。
/// 通过则把这些块标记为 `who` 拥有并返回 true；任一不满足返回 false 且**不改动**。
fn exo_alloc(owner: &mut [i32; NBLK], who: i32, start: usize, len: usize) -> bool {
    // 越界校验（用减法比较避免 start+len 溢出）。
    if len == 0 || start >= NBLK || len > NBLK - start {
        return false;
    }
    // 重叠校验：必须整段空闲。
    for b in start..start + len {
        if owner[b] != FREE {
            return false;
        }
    }
    // 安全：发放资源句柄（标记归属）。
    for b in start..start + len {
        owner[b] = who;
    }
    true
}

// ════════════════════════════════════════════════════════════════
// 学生填空区 ②：libB 的「分块（scatter）布局」策略
// ════════════════════════════════════════════════════════════════

/// libB 的块放置策略：在自己的领地 [base, base+len) 里挑 n 个空闲块给一个文件。
///
/// libB 选择「分块 / 倒序」布局：**从领地尾部往前**挑空闲块（高块号优先），
/// 返回挑中的块号列表（即该文件的 inode 块表，逻辑顺序 = 采集顺序）。
/// 这与 libA 的「顺序升序」布局形成鲜明对比——同样的裸块，不同的抽象。
/// 块不够则返回 None（并保持 used 不变）。
fn libb_place(base: usize, len: usize, used: &mut [bool], n: usize) -> Option<Vec<usize>> {
    // 先看是否有足够空闲块，避免挑到一半失败留下脏标记。
    let avail = (base..base + len).filter(|&b| !used[b]).count();
    if n == 0 || avail < n {
        return None;
    }
    let mut blocks = Vec::with_capacity(n);
    // 从尾向前（高块号优先）采集：这就是 libB 与 libA 不同的布局选择。
    let mut b = base + len;
    while b > base && blocks.len() < n {
        b -= 1;
        if !used[b] {
            used[b] = true;
            blocks.push(b);
        }
    }
    Some(blocks)
}

// ════════════════════════════════════════════════════════════════
// 以下为参考实现 + 测试 harness —— 勿改
// ════════════════════════════════════════════════════════════════

/// libA 的「顺序（sequential）布局」策略：从领地头部找第一段连续 n 个空闲块。
/// 这是给定的参照 libOS（与学生要填的 libB 形成对比）。
fn liba_place(base: usize, len: usize, used: &mut [bool], n: usize) -> Option<Vec<usize>> {
    if n == 0 || n > len {
        return None;
    }
    let mut i = base;
    while i + n <= base + len {
        if (i..i + n).all(|b| !used[b]) {
            for b in i..i + n {
                used[b] = true;
            }
            return Some((i..i + n).collect());
        }
        i += 1;
    }
    None
}

/// 一个最朴素的 libOS 文件系统：在自己的块领地上，用某种 place 策略建文件抽象。
struct LibFs {
    base: usize,
    len: usize,
    used: [bool; NBLK],
    // 目录：文件名 -> 块表（inode）。块表里块号的顺序就是逻辑字节顺序。
    inodes: Vec<(&'static str, Vec<usize>, usize)>, // (name, blocks, byte_len)
    chunked: bool, // false=libA 顺序, true=libB 分块
}

impl LibFs {
    fn new(base: usize, len: usize, chunked: bool) -> Self {
        LibFs {
            base,
            len,
            used: [false; NBLK],
            inodes: Vec::new(),
            chunked,
        }
    }

    fn place(&mut self, n: usize) -> Option<Vec<usize>> {
        if self.chunked {
            libb_place(self.base, self.len, &mut self.used, n)
        } else {
            liba_place(self.base, self.len, &mut self.used, n)
        }
    }

    /// 写文件：按本 libOS 的布局策略分块，再把字节铺进物理磁盘。
    fn write(&mut self, name: &'static str, data: &[u8]) -> bool {
        let nblk = data.len().div_ceil(BS);
        let blocks = match self.place(nblk) {
            Some(b) => b,
            None => return false,
        };
        for (i, &byte) in data.iter().enumerate() {
            let gblk = blocks[i / BS];
            unsafe {
                DISK[gblk * BS + i % BS] = byte;
            }
        }
        self.inodes.push((name, blocks, data.len()));
        true
    }

    /// 读文件：按 inode 块表从物理磁盘把字节拼回来。
    fn read(&self, name: &str) -> Option<Vec<u8>> {
        let (_, blocks, blen) = self.inodes.iter().find(|(n, _, _)| *n == name)?;
        let mut out = Vec::with_capacity(*blen);
        for i in 0..*blen {
            let gblk = blocks[i / BS];
            unsafe {
                out.push(DISK[gblk * BS + i % BS]);
            }
        }
        Some(out)
    }

    fn blocks_of(&self, name: &str) -> Option<&Vec<usize>> {
        self.inodes.iter().find(|(n, _, _)| n == &name).map(|(_, b, _)| b)
    }
}

// ── 子实验 1：exo 内核当裁判 —— 越界 / 重叠必拒，合法必准 ────────
fn check_exo() -> bool {
    let mut owner = [FREE; NBLK];
    let mut ok = true;

    // (who, start, len, 期望是否被准许)
    let reqs: &[(i32, usize, usize, bool)] = &[
        (0, 0, 6, true),   // libA 拿 [0,6)
        (1, 6, 6, true),   // libB 拿 [6,12)
        (2, 4, 3, false),  // 与块 4,5 重叠 → 拒
        (2, 14, 4, false), // 14+4=18 越界(NBLK=16) → 拒
        (2, 12, 4, true),  // [12,16) 合法
        (3, 0, 1, false),  // 与块 0 重叠 → 拒
        (3, 12, 1, false), // 与块 12 重叠 → 拒
        (3, 16, 1, false), // 起点就越界 → 拒
    ];

    for (i, &(who, s, l, want)) in reqs.iter().enumerate() {
        let got = exo_alloc(&mut owner, who, s, l);
        if got != want {
            println!(
                "EXO_BAD 请求#{i} (who={who} start={s} len={l}) 期望准许={want} 实得={got}",
            );
            ok = false;
        }
    }

    // 不变式：被准许的发放必须让 owner 表恰好是 0..6→0, 6..12→1, 12..16→2。
    let want_owner: Vec<i32> = (0..NBLK)
        .map(|b| if b < 6 { 0 } else if b < 12 { 1 } else { 2 })
        .collect();
    for b in 0..NBLK {
        if owner[b] != want_owner[b] {
            println!("EXO_BAD 块{b} 归属={} 期望={}", owner[b], want_owner[b]);
            ok = false;
        }
    }

    if ok {
        println!("EXO_PASS exo 内核安全多路复用：越界/重叠请求被拒，合法请求获句柄");
    }
    ok
}

// ── 子实验 2：两个 libOS 在各自资源上建不同抽象，都能跑 ─────────
fn check_libos() -> bool {
    let mut owner = [FREE; NBLK];
    let mut ok = true;

    // exo 内核把磁盘分给两个 libOS（互不重叠的块区间）。
    let ga = exo_alloc(&mut owner, 0, 0, 6); // libA: [0,6)
    let gb = exo_alloc(&mut owner, 1, 6, 6); // libB: [6,12)
    if !ga || !gb {
        println!("LIBOS_BAD libOS 没拿到资源句柄（exo_alloc 校验未实现?）");
        return false;
    }

    let f1: &[u8] = &[10, 11, 12, 13, 14]; // 5 字节 → 2 块
    let f2: &[u8] = &[20, 21, 22]; //          3 字节 → 1 块

    let mut liba = LibFs::new(0, 6, false); // 顺序布局
    let mut libb = LibFs::new(6, 6, true); // 分块布局

    for fs in [&mut liba, &mut libb] {
        if !fs.write("f1", f1) || !fs.write("f2", f2) {
            println!("LIBOS_BAD 某 libOS 写文件失败（布局策略未实现?）");
            return false;
        }
    }

    // (a) 两种布局都能正确 round-trip 同一份数据。
    for (tag, fs) in [("libA", &liba), ("libB", &libb)] {
        if fs.read("f1").as_deref() != Some(f1) || fs.read("f2").as_deref() != Some(f2) {
            println!("LIBOS_BAD {tag} 读回的数据与写入不一致");
            ok = false;
        }
    }

    // (b) 同一份数据，两种布局的物理块映射不同 → 证明「抽象不同」。
    let ba = liba.blocks_of("f1").cloned().unwrap_or_default();
    let bb = libb.blocks_of("f1").cloned().unwrap_or_default();
    if ba == bb {
        println!("LIBOS_BAD 两个 libOS 的块布局相同，没体现「不同抽象」: {ba:?}");
        ok = false;
    }
    // libA 应是升序连续，libB 应是从尾倒序（高块号在前）。
    let asc_contig = ba.windows(2).all(|w| w[1] == w[0] + 1) && ba.first() == Some(&0);
    let desc = bb.windows(2).all(|w| w[1] + 1 == w[0]);
    if !asc_contig {
        println!("LIBOS_BAD libA f1 布局非「顺序升序」: {ba:?}");
        ok = false;
    }
    if !desc {
        println!("LIBOS_BAD libB f1 布局非「分块倒序」: {bb:?}");
        ok = false;
    }

    // (c) 隔离：每个 libOS 只碰自己被发放的块（exo 的不重叠分配保证）。
    for (tag, fs, lo, hi) in [("libA", &liba, 0, 6), ("libB", &libb, 6, 12)] {
        let stray = fs
            .inodes
            .iter()
            .flat_map(|(_, b, _)| b.iter())
            .any(|&blk| blk < lo || blk >= hi);
        if stray {
            println!("LIBOS_BAD {tag} 越界访问了不属于自己的块");
            ok = false;
        }
    }

    if ok {
        println!(
            "LIBOS_PASS 同一裸磁盘、同一套块资源：libA={ba:?}(顺序) vs libB={bb:?}(分块)，都正确读写",
        );
    }
    ok
}

fn main() {
    let mut all = true;
    all &= check_exo();
    all &= check_libos();
    if all {
        println!("ALL_PASS");
    } else {
        // 未全过：缺少对应 *_PASS 串即判定未完成（无需打印禁用词）。
        std::process::exit(1);
    }
}
