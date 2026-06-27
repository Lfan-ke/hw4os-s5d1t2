//! 进程通信：原子操作、锁与「A 等 B 置位」的完成握手 —— Rust 参考解。
//!
//! 共享「控制字」(沿用 VLAN 的「包字」同构思路)，32-bit：
//!   [31]BUSY  [30]DONE  [29]LOCK  [28]START  [15:0]RESULT
//!
//! 四段逐题递进，全部建立在「共享状态 + 原子位」之上：
//!   1. done-bit 握手   —— B 干完置 DONE，A 死盯黑板，见 DONE 才取数。
//!   2. test_and_set 锁 —— 为什么「涂黑板」必须原子。
//!   3. 计数信号量      —— 把「一个位」推广到「N 个资源」。
//!   4. 编排 capstone   —— A 按门铃→等 DONE→做后续，B 见门铃→算→置位。
//!
//! 学生只需填 8 个函数体；下方测试 harness（向量 + 不变量 + PASS 打印）勿改。
#![allow(dead_code)]

use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::Arc;
use std::thread;

// ── 控制字位布局 ─────────────────────────────────────────────────
const BUSY: u32 = 1 << 31;
const DONE: u32 = 1 << 30;
const LOCK: u32 = 1 << 29;
const START: u32 = 1 << 28;
const RESULT_MASK: u32 = 0xFFFF;

// ════════════════════════════════════════════════════════════════
// 学生填空区：8 个纯函数（硬件路径是同一字段的几个 always/组合块）
// ════════════════════════════════════════════════════════════════

// ── 1. done-bit 握手 ─────────────────────────────────────────────

/// B 干完活：把 result 打进 [15:0]、置 DONE、清 BUSY。
fn b_finish(result: u32) -> u32 {
    DONE | (result & RESULT_MASK)
}

/// A 看黑板：仅当 DONE=1 才 ready，并取 [15:0] 作为 result。
fn a_poll(ctrl: u32) -> (bool, u32) {
    // 一次性解包：DONE 位决定 ready，低 16 位是 result。
    (ctrl & DONE != 0, ctrl & RESULT_MASK)
}

// ── 2. test_and_set 自旋锁 ───────────────────────────────────────

/// 原子 test-and-set：无条件写 1，返回(新值, 旧值是否为 0=抢到)。对应 amoswap。
fn tas(lock: u32) -> (u32, bool) {
    (1, lock == 0)
}

/// 释放锁：写 0。
fn unlock() -> u32 {
    0
}

/// 用 tas 拼出 try_lock：旧值写回新值（恒 1），got 表示是否抢到。
fn try_lock(lock: &mut u32) -> bool {
    let (newv, got) = tas(*lock);
    *lock = newv;
    got
}

// ── 3. 计数信号量 ────────────────────────────────────────────────

/// down(P)：count-1；ok = count' >= 0（>=0 说明拿到资源，<0 说明该「阻塞」）。
fn down(count: i32) -> (i32, bool) {
    // TODO[a] 阻塞式：恒减一，count' 变负即记录一个等待者，ok=false。
    let c = count - 1;
    (c, c >= 0)
    // ELSE[b] 自旋式：count>0 才减一并 ok=true，否则原样返回 ok=false 让调用方重试。
}

/// up(V)：count+1（唤醒一个等待者 / 归还一个资源）。
fn up(count: i32) -> i32 {
    count + 1
}

// ── 4. 编排 capstone：A 控制 B 全流程 ────────────────────────────

/// B 的一步：见 START → 清 START、(置 BUSY 算完即清)、置 DONE、RESULT=job。
fn b_step(ctrl: u32, job: u32) -> u32 {
    if ctrl & START != 0 {
        DONE | (job & RESULT_MASK)
    } else {
        ctrl
    }
}

/// A 的一步：phase=0 按门铃(置 START)→phase=1；
/// phase=1 见 DONE 才做后续(post=result*2)、清 DONE、回 phase=0；否则原地等。
/// 返回 (ctrl', phase', post)。
fn a_step(ctrl: u32, phase: u32) -> (u32, u32, u32) {
    if phase == 0 {
        (ctrl | START, 1, 0) // 按门铃
    } else if ctrl & DONE != 0 {
        // 先用后清 DONE：取 result 算 post，再清 DONE 进下一轮。
        let post = (ctrl & RESULT_MASK) * 2;
        (ctrl & !DONE, 0, post)
        // ELSE[b] 先清后用：let post=...; (ctrl & !DONE, 0, post) —— 同一断言下等价。
    } else {
        (ctrl, 1, 0) // B 没干完，A 死等
    }
}

// ════════════════════════════════════════════════════════════════
// 测试 harness（给定，勿改）
// ════════════════════════════════════════════════════════════════

/// 给定的「非原子读-改-写」对照：示范丢更新（两人都读到 0，各 +1，都写 1）。
fn naive_lost_update() -> i32 {
    let shared = 0i32;
    let r0 = shared; // proc0 读到 0
    let r1 = shared; // proc1 也读到 0（在 proc0 写回之前）
    let _w0 = r0 + 1; // proc0 算出 1
    let w1 = r1 + 1; // proc1 算出 1，覆盖写——丢了一次自增
    w1
}

fn check_handshake() -> bool {
    let mut ok = true;

    // (a) B 运行中：DONE=0 + 垃圾 RESULT —— A 绝不能 ready。
    let running = BUSY | 0xDEAD;
    let (r0, _) = a_poll(running);
    if r0 {
        println!("EARLY_FAIL A 在 DONE=0 时就绪了(读到垃圾值)");
        ok = false;
    }
    // (b) B 完成：DONE=1, RESULT=0x1234 —— A 必须 ready 且取数正确。
    let done = b_finish(0x1234);
    let (r1, v1) = a_poll(done);
    if !r1 || v1 != 0x1234 {
        println!("HANDSHAKE_FAIL 完成态 ready={} result=0x{:04x} 应=(true,0x1234)", r1, v1);
        ok = false;
    }

    // (c) 两个真执行流：B 线程置位，A 线程死盯黑板（有界自旋，防卡死）。
    let ctrl = Arc::new(AtomicU32::new(BUSY)); // 初始 B 忙
    let b = ctrl.clone();
    let hb = thread::spawn(move || {
        for _ in 0..2000 {
            std::hint::spin_loop();
        }
        // 协议：先写 RESULT 再置 DONE —— Release 保证 A 用 Acquire 读到的是写全的字。
        b.store(b_finish(0xBEEF), Ordering::Release);
    });
    let mut got = None;
    for _ in 0..50_000_000u64 {
        let c = ctrl.load(Ordering::Acquire);
        let (ready, val) = a_poll(c);
        if ready {
            got = Some(val);
            break;
        }
    }
    hb.join().unwrap();
    if got != Some(0xBEEF) {
        println!("HANDSHAKE_FAIL 双执行流握手 got={:?} 应=Some(0xBEEF)", got);
        ok = false;
    }

    if ok {
        println!("HANDSHAKE_PASS");
    }
    ok
}

fn check_tas_mutex() -> bool {
    // (a) tas 契约：空锁→(1,抢到)；占用→(1,没抢到)；unlock→0。
    let mut tok = true;
    let (n0, g0) = tas(0);
    let (n1, g1) = tas(1);
    if n0 != 1 || !g0 || n1 != 1 || g1 {
        println!("TAS_FAIL tas(0)=({},{}) tas(1)=({},{}) 应=(1,true)/(1,false)", n0, g0, n1, g1);
        tok = false;
    }
    if unlock() != 0 {
        println!("TAS_FAIL unlock() 应=0");
        tok = false;
    }
    if tok {
        println!("TAS_PASS");
    }

    // (b) 给定交错调度跑两个 proc 抢锁/放锁，断言临界区内 <= 1。
    let mut mok = true;
    let mut lock = 0u32;
    let mut in_cs = 0i32;
    // proc0 抢锁→进临界区
    if try_lock(&mut lock) {
        in_cs += 1;
    } else {
        println!("MUTEX_FAIL proc0 抢空锁却失败");
        mok = false;
    }
    if in_cs > 1 {
        println!("DOUBLE_ENTER_FAIL 同时 {} 个在临界区", in_cs);
        mok = false;
    }
    // proc1 在 proc0 持锁期间抢锁→必须失败
    if try_lock(&mut lock) {
        in_cs += 1;
        if in_cs > 1 {
            println!("DOUBLE_ENTER_FAIL proc1 闯入临界区，同时 {} 个", in_cs);
            mok = false;
        }
    }
    // proc0 放锁
    lock = unlock();
    in_cs -= 1;
    // proc1 重试→这次抢到
    if try_lock(&mut lock) {
        in_cs += 1;
    } else {
        println!("MUTEX_FAIL proc1 在锁释放后仍抢不到");
        mok = false;
    }
    if in_cs > 1 {
        println!("DOUBLE_ENTER_FAIL 同时 {} 个在临界区", in_cs);
        mok = false;
    }
    lock = unlock();
    in_cs -= 1;
    if in_cs != 0 {
        println!("MUTEX_FAIL 收尾时临界区计数={} 应=0", in_cs);
        mok = false;
    }
    let _ = lock;

    // 对照：非原子读改写丢更新（信息行，非判据）。
    println!("NAIVE_RACE 非原子读改写丢更新: got={} expected=2", naive_lost_update());

    if mok {
        println!("MUTEX_PASS");
    }
    tok && mok
}

fn check_sem() -> bool {
    let mut ok = true;

    // (a) 契约：有货 down 成功并减一；空仓 down 返回 ok=false；up 加一。
    let (c2, ok2) = down(2);
    if !ok2 || c2 != 1 {
        println!("SEM_FAIL down(2)=({},{}) 应=(1,true)", c2, ok2);
        ok = false;
    }
    let (c1, ok1) = down(1);
    if !ok1 || c1 != 0 {
        println!("SEM_FAIL down(1)=({},{}) 应=(0,true)", c1, ok1);
        ok = false;
    }
    let (_c0, ok0) = down(0);
    if ok0 {
        println!("SEM_FAIL down(0) 空仓却返回 ok=true（应阻塞）");
        ok = false;
    }
    if up(0) != 1 || up(2) != 3 {
        println!("SEM_FAIL up(0)={} up(2)={} 应=1/3", up(0), up(2));
        ok = false;
    }

    // (b) 不变式：2 个资源恰好发放 2 次；第 3 次 down 阻塞；up 后队首可继续。
    //     规则：仅当 ok=true 才把 count 更新为 count'（阻塞/自旋两种实现皆等价）。
    let mut count = 2i32;
    let mut grants = 0;
    for _ in 0..3 {
        let (c, g) = down(count);
        if g {
            count = c;
            grants += 1;
        }
    }
    if grants != 2 {
        println!("SEM_FAIL 2 个资源却发放了 {} 次", grants);
        ok = false;
    }
    count = up(count); // 归还一个
    let (c, g) = down(count);
    if !g {
        println!("SEM_FAIL up 之后队首仍拿不到资源");
        ok = false;
    } else {
        count = c;
    }
    let _ = count;

    if ok {
        println!("SEM_PASS");
    }
    ok
}

fn check_orch() -> bool {
    let mut ok = true;
    let jobs = [7u32, 21, 100, 3];
    let mut ctrl = 0u32;
    let mut phase = 0u32;

    for (r, &job) in jobs.iter().enumerate() {
        // A 按门铃
        let (c1, p1, _) = a_step(ctrl, phase);
        ctrl = c1;
        phase = p1;
        if ctrl & START == 0 || phase != 1 {
            println!("ORCH_FAIL r{} A 没按门铃(START 未置/相位错)", r);
            ok = false;
        }
        // A 提前轮询：B 还没干完，A 不得推进（锁步约束）
        let (c2, p2, post2) = a_step(ctrl, phase);
        if c2 != ctrl || p2 != 1 || post2 != 0 {
            println!("ORCH_FAIL r{} A 在 B 完成前就推进了(乱序)", r);
            ok = false;
        }
        // B 干活、置位
        ctrl = b_step(ctrl, job);
        if ctrl & DONE == 0 || ctrl & START != 0 || (ctrl & RESULT_MASK) != job {
            println!("ORCH_FAIL r{} B 未正确置位 ctrl=0x{:08x}", r, ctrl);
            ok = false;
        }
        // A 检测到 DONE，做后续 post=result*2，清 DONE 进下一轮
        let (c3, p3, post3) = a_step(ctrl, phase);
        ctrl = c3;
        phase = p3;
        if post3 != job * 2 || ctrl & DONE != 0 || phase != 0 {
            println!("ORCH_FAIL r{} 后续值/收尾错 post={} 应={}", r, post3, job * 2);
            ok = false;
        }
    }

    if ok {
        println!("ORCH_PASS");
    }
    ok
}

fn main() {
    let mut all = true;
    all &= check_handshake();
    all &= check_tas_mutex();
    all &= check_sem();
    all &= check_orch();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
