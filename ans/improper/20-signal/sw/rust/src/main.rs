//! 异步事件 · 信号：软件建模一个「进程」的信号机制 —— Rust 参考解。
//!
//! 把一个「进程」抽象成三样东西 + 一个动作：
//!   - handler 表 handlers[NSIG]   : 每个信号号注册的处理函数（None = 默认忽略）。
//!   - pending  位集 (u32)          : 暂时投递不出去、挂起等待的信号（「位」不是「计数」）。
//!   - mask     位集 (u32)          : 被屏蔽的信号；屏蔽期间来的信号只能进 pending。
//!   动作 raise(sig)：被屏蔽 → 入 pending；否则 → 立刻跑 handler（投递）。
//!   动作 unmask(sig)：解屏蔽，并把这期间攒下的 pending「补投递」出去。
//!
//! 四段逐题递进：
//!   1. DELIVER   —— 注册 handler，raise → handler 真的跑了、改了标志。
//!   2. MASK      —— 先 mask 再 raise → handler 不跑、信号进 pending。
//!   3. PENDING   —— unmask → 把 pending 补投递出来。
//!   4. REENTRANT —— handler 执行中再来同号信号 → 合并/不丢、且绝不递归重入崩。
//!
//! 学生只需填 raise / unmask 两个函数体；下方 run_handler 与测试 harness 勿改。
#![allow(dead_code)]

const NSIG: usize = 8;

/// 信号处理函数：拿到「进程」与信号号，可读写进程状态（甚至再 raise）。
type Handler = fn(&mut Proc, usize);

/// 被建模的「进程」：信号机制的全部状态都在这里。
struct Proc {
    handlers: [Option<Handler>; NSIG], // 信号表：每号一个 handler（None=默认忽略）
    pending: u32,                      // 挂起位集：投递不出去、攒着的信号
    mask: u32,                         // 屏蔽位集：被挡住的信号
    in_handler: u32,                   // 正在处理中的信号（可重入守卫用）
    run_count: [u32; NSIG],            // 每号 handler 实际跑了几次（观测用）
    depth: u32,                        // 当前 handler 嵌套深度
    max_depth: u32,                    // 历史最大嵌套深度（应恒为 1）
    flag: [u32; NSIG],                 // handler 可改的标志（证明它真的跑过）
    reent_left: u32,                   // 可重入测试：首次进入时再触发几次同号信号
}

impl Proc {
    fn new() -> Self {
        Proc {
            handlers: [None; NSIG],
            pending: 0,
            mask: 0,
            in_handler: 0,
            run_count: [0; NSIG],
            depth: 0,
            max_depth: 0,
            flag: [0; NSIG],
            reent_left: 0,
        }
    }
}

fn bit(sig: usize) -> u32 {
    1u32 << sig
}
fn is_masked(p: &Proc, sig: usize) -> bool {
    p.mask & bit(sig) != 0
}
fn is_pending(p: &Proc, sig: usize) -> bool {
    p.pending & bit(sig) != 0
}
fn is_in_handler(p: &Proc, sig: usize) -> bool {
    p.in_handler & bit(sig) != 0
}

/// 注册 handler（given）。
fn install(p: &mut Proc, sig: usize, h: Handler) {
    p.handlers[sig] = Some(h);
}

/// 屏蔽一个信号（given）：只置 mask 位。
fn mask_sig(p: &mut Proc, sig: usize) {
    p.mask |= bit(sig);
}

/// 真正把 handler 跑起来（given，勿改）。两个职责：
///   1. 可重入守卫：若同号 handler 正在运行，绝不递归进入——把这次合并进 pending 后返回。
///   2. 返回时补投递：handler 跑完，若运行期间攒了同号 pending（且没被屏蔽），补投递一次。
/// 这正是「标准信号」语义：运行期间的多次同号信号被合并成一个，返回后只补送一次
/// （实时信号才会逐个排队，本模型不展开）。
fn run_handler(p: &mut Proc, sig: usize) {
    // —— 可重入守卫 ——
    if is_in_handler(p, sig) {
        p.pending |= bit(sig); // 合并：handler 运行中再来同号，挂起、不递归
        return;
    }
    p.in_handler |= bit(sig);
    p.depth += 1;
    if p.depth > p.max_depth {
        p.max_depth = p.depth;
    }
    if let Some(h) = p.handlers[sig] {
        h(p, sig); // handler 里若再 raise 同号 → 经 raise→run_handler 被上面的守卫拦下
        p.run_count[sig] += 1;
    }
    p.depth -= 1;
    p.in_handler &= !bit(sig);
    // —— 返回补投递（合并后的那一个）——
    if is_pending(p, sig) && !is_masked(p, sig) {
        p.pending &= !bit(sig);
        run_handler(p, sig); // 此时 in_handler 已清，不会被守卫拦
    }
}

// ════════════════════════════════════════════════════════════════
// 学生填空区：raise / unmask 两个函数
// ════════════════════════════════════════════════════════════════

/// raise(sig)：投递一个信号 —— 「立刻投递」还是「先入 pending」？
fn raise_sig(p: &mut Proc, sig: usize) {
    // 被屏蔽 → 入 pending（攒着，等 unmask 补投递）；否则 → 立刻投递。
    if is_masked(p, sig) {
        p.pending |= bit(sig);
    } else {
        run_handler(p, sig);
    }
}

/// unmask(sig)：解除屏蔽，并把屏蔽期间攒下的 pending「补投递」出去。
fn unmask_sig(p: &mut Proc, sig: usize) {
    p.mask &= !bit(sig); // 先放开屏蔽
    if is_pending(p, sig) {
        // 还挂着同号 pending → 清掉 pending 位，补投递一次。
        p.pending &= !bit(sig);
        run_handler(p, sig);
    }
}

// ════════════════════════════════════════════════════════════════
// 测试用 handler（given）
// ════════════════════════════════════════════════════════════════

/// 普通 handler：改一个标志，证明自己真的被调用过。
fn h_deliver(p: &mut Proc, sig: usize) {
    p.flag[sig] = 0xA5;
}

/// 可重入 handler：首次进入时连发若干次同号信号。
/// 它们都在「自己正在运行」期间到来 → 被守卫合并成 1 个 pending → 返回后只补投递一次。
fn h_reentrant(p: &mut Proc, sig: usize) {
    while p.reent_left > 0 {
        p.reent_left -= 1;
        raise_sig(p, sig); // 期间到来：合并、不递归
    }
}

// ════════════════════════════════════════════════════════════════
// 测试 harness（给定，勿改）
// ════════════════════════════════════════════════════════════════

fn check_deliver() -> bool {
    let mut ok = true;
    let mut p = Proc::new();
    let sig = 3;
    install(&mut p, sig, h_deliver);

    // 没注册 handler 的信号被 raise：默认忽略，不得跑、不得崩。
    raise_sig(&mut p, 5);
    if p.run_count[5] != 0 {
        println!("DELIVER_BAD 无 handler 的信号竟被执行");
        ok = false;
    }

    // 注册后 raise：handler 必须跑一次、并改了标志。
    raise_sig(&mut p, sig);
    if p.run_count[sig] != 1 || p.flag[sig] == 0 {
        println!(
            "DELIVER_MISS raise 后 handler 没跑 run_count={} flag=0x{:02x}",
            p.run_count[sig], p.flag[sig]
        );
        ok = false;
    }
    if is_pending(&p, sig) {
        println!("DELIVER_BAD 未屏蔽却进了 pending");
        ok = false;
    }

    if ok {
        println!("DELIVER_PASS");
    }
    ok
}

fn check_mask() -> bool {
    let mut ok = true;
    let mut p = Proc::new();
    let sig = 3;
    install(&mut p, sig, h_deliver);

    mask_sig(&mut p, sig);
    raise_sig(&mut p, sig);
    if p.run_count[sig] != 0 || p.flag[sig] != 0 {
        println!("MASK_MISS 屏蔽期间 handler 竟然跑了 run_count={}", p.run_count[sig]);
        ok = false;
    }
    if !is_pending(&p, sig) {
        println!("MASK_BAD 屏蔽期间 raise 没有进 pending");
        ok = false;
    }

    if ok {
        println!("MASK_PASS");
    }
    ok
}

fn check_pending() -> bool {
    let mut ok = true;
    let mut p = Proc::new();
    let sig = 3;
    install(&mut p, sig, h_deliver);

    mask_sig(&mut p, sig);
    raise_sig(&mut p, sig); // 进 pending
    raise_sig(&mut p, sig); // 再来一次：合并，pending 仍只 1 个
    if p.run_count[sig] != 0 {
        println!("PENDING_BAD 解屏蔽前不该执行 run_count={}", p.run_count[sig]);
        ok = false;
    }

    unmask_sig(&mut p, sig); // 解屏蔽 → 补投递
    if p.run_count[sig] != 1 || p.flag[sig] == 0 {
        println!("PENDING_MISS 解屏蔽后没有补投递 run_count={}", p.run_count[sig]);
        ok = false;
    }
    if is_pending(&p, sig) || is_masked(&p, sig) {
        println!("PENDING_BAD 补投递后 pending/mask 没清干净");
        ok = false;
    }

    if ok {
        println!("PENDING_PASS");
    }
    ok
}

fn check_reentrant() -> bool {
    let mut ok = true;
    let mut p = Proc::new();
    let sig = 2;
    install(&mut p, sig, h_reentrant);
    p.reent_left = 3; // handler 首次进入时连发 3 次同号信号

    raise_sig(&mut p, sig);

    // 期望：初投递 1 次 + 合并后的补投递 1 次 = 共 2 次；从不递归(max_depth==1)；pending 清空。
    if p.max_depth != 1 {
        println!("REENTRANT_BAD handler 被递归重入 max_depth={}", p.max_depth);
        ok = false;
    }
    if p.run_count[sig] != 2 {
        println!(
            "REENTRANT_MISS 期望执行 2 次(初投递+合并补投递)，实际 {}",
            p.run_count[sig]
        );
        ok = false;
    }
    if is_pending(&p, sig) {
        println!("REENTRANT_BAD 收尾仍有 pending 未清");
        ok = false;
    }

    if ok {
        println!("REENTRANT_PASS");
    }
    ok
}

fn main() {
    let mut all = true;
    all &= check_deliver();
    all &= check_mask();
    all &= check_pending();
    all &= check_reentrant();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
