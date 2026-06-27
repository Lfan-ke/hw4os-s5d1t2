//! 形态 F1 · 宏内核 / 单内核（monolithic）—— Rust。
//!
//! 本质：fs / 调度 / 驱动 / 内存 等所有服务都跑在**同一个地址空间**里，
//! 子系统之间靠**直接函数调用**协作——没有 IPC、没有特权切换。
//!   好处：快（一次系统调用 = 几次普通函数调用，零消息、零上下文切换）。
//!   代价：脆（没有隔离边界——一个驱动越界写就能直接踩坏调度器的内存）。
//! 真实例：xv6、Linux、FreeBSD——fs/sched/driver 全在 kernel/ 下共享 struct。
//!
//! 两段 demo：
//!   1. MONO    —— fs_read / sched_pick / driver_io 当「内核服务」直接串起来。
//!   2. FRAGILE —— 同一块 kmem 里驱动缓冲紧挨调度器队列；越界 DMA 写污染调度器。
//!
//! 你只需填 6 个函数体（标 TODO 处）；下方测试 harness 勿改。
#![allow(unused_variables, dead_code)]

// ── 单一内核地址空间布局 ─────────────────────────────────────────
// kmem[8]：[0..4) 驱动 TX 缓冲区，[4..8) 调度器就绪队列优先级。
// 这条边界**只存在于注释里**，硬件上它就是一根连续数组——宏内核「脆」的根源。
const KMEM: usize = 8;
const DRV_BASE: usize = 0;
const DRV_LEN: usize = 4;
const SCHED_BASE: usize = 4;
const SCHED_LEN: usize = 4;

const CMD_WRITE: u32 = 1; // 驱动命令：写
const CMD_READ: u32 = 0; // 驱动命令：读

// ════════════════════════════════════════════════════════════════
// 学生填空区：6 个函数
// ════════════════════════════════════════════════════════════════

// ── 1. 三个「内核服务」(在宏内核里就是普通函数) ──────────────────

/// fs 服务：读 inode 的内容字。约定 content = 100 + inode。
fn fs_read(inode: u32) -> u32 {
    // TODO: 返回 100 + inode。
    0 // ← 占位
}

/// 调度服务：在就绪队列(各任务优先级)里挑优先级最大者，返回其下标。
/// 并列时取第一个（最低下标）。
fn sched_pick(prios: &[u32]) -> usize {
    // TODO: 遍历 prios，返回最大值的下标（并列取最低下标）。
    // HINT: best=0; for i in 1..len { if prios[i]>prios[best] { best=i } }
    0 // ← 占位
}

/// 驱动服务：device IO。WRITE 返回设备回执 (arg + 0x10)；READ 返回寄存器 0xD0。
fn driver_io(cmd: u32, arg: u32) -> u32 {
    // TODO: cmd==CMD_WRITE → arg + 0x10；否则 → 0xD0。
    0 // ← 占位
}

// ── 2. 服务直接调用链：一次「系统调用」就是几次直接函数调用 ───────

/// 宏内核的 syscall 路径：直接调 fs_read → sched_pick → driver_io，
/// 无 IPC、无消息、无特权切换。返回 (result, hops)：
///   result = data + idx + ack（把三个服务的产物合起来）
///   hops   = 直接函数调用次数（这里恒为 3）
fn syscall_dispatch(inode: u32, prios: &[u32]) -> (u32, u32) {
    // TODO: 直接调用链——在同一地址空间里，服务就是普通函数，一个接一个调：
    //   let data = fs_read(inode);            // 调用 1
    //   let idx  = sched_pick(prios) as u32;  // 调用 2
    //   let ack  = driver_io(CMD_WRITE, data);// 调用 3
    //   (data + idx + ack, 3)
    (0, 0) // ← 占位
}

// ── 3. 无隔离边界：越界 DMA 写 + 破坏检测 ────────────────────────

/// 驱动 DMA 写：把 val 写进「内核地址空间」kmem 的 DRV_BASE+off 处。
/// 注意：这里**不做任何边界检查**——宏内核里驱动和内核共享地址空间，
/// 越界与否完全靠调用方自律。off 越过 DRV_LEN 就会踩进调度器的内存。
fn driver_dma_write(kmem: &mut [u32], off: usize, val: u32) {
    // TODO: kmem[DRV_BASE + off] = val;（不要加边界检查！）
}

/// 破坏检测：把调度器区当前快照 region 与「驱动跑之前」的基准 baseline 比较，
/// 任一字节不同即说明调度器数据被别的子系统踩坏了，返回 true。
fn detect_corruption(region: &[u32], baseline: &[u32]) -> bool {
    // TODO: region != baseline 即返回 true。
    false // ← 占位
}

// ════════════════════════════════════════════════════════════════
// 测试 harness（给定，勿改）
// ════════════════════════════════════════════════════════════════

fn sched_region(kmem: &[u32]) -> &[u32] {
    &kmem[SCHED_BASE..SCHED_BASE + SCHED_LEN]
}

fn check_mono() -> bool {
    let mut ok = true;

    // (a) 三个服务各自的契约。
    if fs_read(3) != 103 {
        println!("MONO_FAIL fs_read(3)={} 应=103", fs_read(3));
        ok = false;
    }
    let prios = [10u32, 30, 20, 5];
    if sched_pick(&prios) != 1 {
        println!("MONO_FAIL sched_pick([10,30,20,5])={} 应=1", sched_pick(&prios));
        ok = false;
    }
    if driver_io(CMD_WRITE, 50) != 66 || driver_io(CMD_READ, 0) != 0xD0 {
        println!(
            "MONO_FAIL driver_io write={} read=0x{:x} 应=66/0xD0",
            driver_io(CMD_WRITE, 50),
            driver_io(CMD_READ, 0)
        );
        ok = false;
    }

    // (b) 直接调用链：结果 = 三服务产物之和，hops=3，IPC 全程为 0。
    let inode = 3u32;
    let expect = fs_read(inode) + sched_pick(&prios) as u32 + driver_io(CMD_WRITE, fs_read(inode));
    let (result, hops) = syscall_dispatch(inode, &prios);
    if result != expect {
        println!("MONO_FAIL syscall_dispatch 结果={} 应={}", result, expect);
        ok = false;
    }
    if hops != 3 {
        println!("MONO_FAIL 直接调用 hops={} 应=3（fs→sched→driver 三次直调）", hops);
        ok = false;
    }
    // 宏内核的卖点：一次 syscall = 3 次普通函数调用，0 条 IPC 消息、0 次上下文切换。
    let ipc_msgs = 0;
    println!("MONO_DISPATCH ipc_msgs={} hops={} result={}（直调，无消息传递）", ipc_msgs, hops, result);

    if ok {
        println!("MONO_PASS");
    }
    ok
}

fn check_fragile() -> bool {
    let mut ok = true;

    // 初始内核地址空间：驱动缓冲清零，调度器就绪队列优先级 [10,30,20,5]。
    let base_kmem: [u32; KMEM] = [0, 0, 0, 0, 10, 30, 20, 5];
    // 基准快照：驱动跑之前的调度器区，用于事后比对。
    let baseline: Vec<u32> = sched_region(&base_kmem).to_vec();
    let pick_before = sched_pick(sched_region(&base_kmem));
    if pick_before != 1 {
        println!("FRAGILE_FAIL 初始调度应选任务1(优先级30)，却选了 {}", pick_before);
        ok = false;
    }

    // (a) 守规矩的 DMA：off=2 在驱动缓冲区内 → 只动自己的地盘。
    let mut kmem = base_kmem;
    driver_dma_write(&mut kmem, 2, 0xAB);
    if kmem[2] != 0xAB {
        println!("FRAGILE_FAIL 合法 DMA 未写入驱动缓冲区 kmem[2]={}", kmem[2]);
        ok = false;
    }
    if detect_corruption(sched_region(&kmem), &baseline) {
        println!("FRAGILE_FAIL 合法 DMA 不该污染调度器，却报告了破坏");
        ok = false;
    }
    if sched_pick(sched_region(&kmem)) != pick_before {
        println!("FRAGILE_FAIL 合法 DMA 后调度决策不该改变");
        ok = false;
    }

    // (b) 越界 DMA：off=5 越过 DRV_LEN=4 → DRV_BASE+5 = kmem[5] = 调度器任务1。
    //     宏内核没有隔离边界，这个写直接落进了调度器的内存。
    let mut kmem = base_kmem;
    driver_dma_write(&mut kmem, 5, 0); // 把任务1的优先级 30 踩成 0
    // 检测：调度器区相对基准被改坏了——跨子系统的破坏被抓到，说明无隔离。
    if !detect_corruption(sched_region(&kmem), &baseline) {
        println!("FRAGILE_FAIL 越界 DMA 已污染调度器，检测却没发现（漏检）");
        ok = false;
    }
    // 后果：整个「内核」行为出错——调度器现在挑了错的任务。
    let pick_after = sched_pick(sched_region(&kmem));
    if pick_after == pick_before {
        println!("FRAGILE_FAIL 调度器被污染后决策应改变，却仍选 {}", pick_after);
        ok = false;
    }
    println!(
        "FRAGILE_OBSERVE 驱动越界 off=5 踩进 kmem[5](调度器任务1)：决策 {}→{}，无隔离边界",
        pick_before, pick_after
    );

    if ok {
        println!("FRAGILE_PASS");
    }
    ok
}

fn main() {
    let mut all = true;
    all &= check_mono();
    all &= check_fragile();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
