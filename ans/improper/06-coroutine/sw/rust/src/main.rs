//! 无栈协程：被 poll 出来的「状态机」绿色线程 —— Rust 参考解。
//!
//! 主线：顺序代码 → 状态机 → 谁来生成这台状态机。
//!   06.1 手写「暂停—恢复」状态机（poll 的本质）  → YIELD_PASS / STATEMACHINE_PASS
//!   06.2 极简协作执行器（合作式调度 / 退化批处理）→ EXEC_PASS / BATCH_PASS
//!   06.3 就绪与唤醒（别空转 busy-poll）           → WAKER_PASS
//!   06.4 让编译器替你写状态机（async/await + join）→ ASYNC_PASS / JOIN_PASS（辅助分）
//!
//! 与「有栈协程（05-fiber）」严格对照：那边换的是栈指针，这边换的是状态号。
//! 「无栈」的肉身体验：凡是跨让出点还要活着的局部（i / acc），都必须放进协程的结构体，
//! 而不是函数栈上——因为根本没有「每任务一根独立栈」。
//!
//! 学生只填带 TODO 的函数体；下方测试 harness 勿改。
#![allow(dead_code)]

use std::cell::RefCell;
use std::collections::VecDeque;
use std::future::Future;
use std::pin::Pin;
use std::rc::Rc;
use std::task::{Context, Poll, RawWaker, RawWakerVTable, Waker};

// ════════════════════════ 公共数据模型 ════════════════════════

/// poll 的返回：要么让出一个值（Pending），要么结束并给出终值（Ready）。
/// 简化成单一 u32 让出值，不引入泛型 / Pin / 生命周期纠缠。
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Step {
    Pending(u32), // 让出一个值，下次还要再 poll
    Ready(u32),   // 结束，给出终值
}

/// 一台无栈协程：从 start 起、步长 step、共让出 count 次，终值 = seed + 让出值之和。
/// 「状态号」就是 i：poll 一次推进一步，把进度记在自己身上。
#[derive(Clone, Copy)]
struct Stepper {
    seed: u32,
    start: u32,
    step: u32,
    count: u32,
    // ↓↓↓ 跨让出点存活的局部，全部塞进结构体（这就是「无栈」）↓↓↓
    i: u32,
    acc: u32,
    done: bool,
}

impl Stepper {
    fn new(seed: u32, start: u32, step: u32, count: u32) -> Self {
        Stepper { seed, start, step, count, i: 0, acc: 0, done: false }
    }
    fn is_done(&self) -> bool {
        self.done
    }
}

// ════════════════════════ 06.1 手写状态机（学生填）════════════════════════

impl Stepper {
    /// poll 一次：把状态机往前推一步。
    /// 状态号 = self.i：i < count 时让出 start + i*step 并累加；i == count 时收尾。
    fn poll(&mut self) -> Step {
        // TODO[a] 显式状态字（i 即状态）+ 分支：
        if self.i < self.count {
            let v = self.start + self.i * self.step; // v 是「本步」局部
            self.acc += v; // acc 必须活到收尾——所以它在 struct 里
            self.i += 1;
            Step::Pending(v)
        } else {
            self.done = true;
            Step::Ready(self.seed + self.acc)
        }
        // ELSE[b] 也可用 protothread 宏（Duff's device，用 __LINE__ 自动生成 case）——见 C 版注释。
    }
}

// ════════════════════════ 06.2 极简协作执行器（学生填）════════════════════════

/// 执行器一轮跑下来的「账本」：让出值的交错序列 + 任务完成顺序的终值。
#[derive(Default)]
struct RunLog {
    trace: Vec<u32>,  // 各任务让出值，按 poll 顺序交错
    finals: Vec<u32>, // 各任务终值，按完成顺序
}

/// round-robin 依次 poll 每个未完成任务：Pending 留在队里、记下让出值；
/// Ready 出队、记下终值。直到全部完成。
fn exec_run(tasks: &mut [Stepper]) -> RunLog {
    let mut log = RunLog::default();
    // TODO[a] 简单数组轮询：反复扫一遍所有任务，未完成的各 poll 一次。
    let mut remaining = tasks.len();
    while remaining > 0 {
        for t in tasks.iter_mut() {
            if t.is_done() {
                continue;
            }
            match t.poll() {
                Step::Pending(v) => log.trace.push(v),
                Step::Ready(f) => {
                    log.finals.push(f);
                    remaining -= 1;
                }
            }
        }
    }
    log
    // ELSE[b] 也可维护一个显式就绪队列（VecDeque），出队 poll、Pending 再入队。
}

// ════════════════════════ 06.3 就绪与唤醒（学生填）════════════════════════

/// 玩具 reactor + waker：每个 Pending 只「登记一次唤醒」，执行器只重 poll 被唤醒者，
/// 而不是把所有 Pending 反复空转。返回 (总 poll 次数, 总唤醒次数)。
///
/// 模型：就绪队列初始装入全部任务（这是每个任务的「首次」poll，不算唤醒）；
/// 之后任务每让出一次（Pending）= reactor 替它登记一次唤醒并重新入队。
/// 于是 总 poll = 任务数 + 唤醒数，绝不忙等。
fn reactor_run(tasks: &mut [Stepper]) -> (u32, u32) {
    let mut polls = 0u32;
    let mut wakes = 0u32;
    // TODO[a] 极简就绪队列：只 poll 队列里的任务（= 被唤醒的）。
    let mut ready: VecDeque<usize> = (0..tasks.len()).collect();
    while let Some(id) = ready.pop_front() {
        let step = tasks[id].poll();
        polls += 1;
        match step {
            Step::Pending(_) => {
                wakes += 1; // reactor：事件到了，重新入队（= 一次唤醒）
                ready.push_back(id);
            }
            Step::Ready(_) => { /* 完成，不再入队 */ }
        }
    }
    (polls, wakes)
    // ELSE[b] 也可用 ready-flag 位图：只置位的才 poll；或真挂到 std 的 Waker。
}

// ════════════════════════ 06.4 让编译器替你写状态机（async/await）════════════════════════
//
// 把 06.1 那段顺序代码用 async fn 重写：编译器会把它「掰成」一台状态机——
// 与你手写的 enum 状态机本质同构。acc / i / v 跨 .await 存活，被编译器塞进生成的 Future。

/// 一个只让出一次的 Future：第一次 poll 返回 Pending，第二次 Ready。
/// 这是 async fn 里 `.await` 让出点的最小零件。
struct Yield {
    armed: bool,
}
impl Future for Yield {
    type Output = ();
    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<()> {
        if self.armed {
            Poll::Ready(())
        } else {
            self.armed = true;
            cx.waker().wake_by_ref();
            Poll::Pending
        }
    }
}
fn yield_now() -> Yield {
    Yield { armed: false }
}

/// 06.1 的协程，用 async/await 重写一遍（学生填循环体）。
/// 让出的值写进共享 log（侧信道），终值 = seed + 让出值之和。
async fn stepper_async(seed: u32, start: u32, step: u32, count: u32, log: Rc<RefCell<Vec<u32>>>) -> u32 {
    let mut acc = 0u32; // 跨 .await 存活 → 编译器塞进生成的状态机
    let mut i = 0u32;
    // TODO: while i < count { 算 v；acc += v；log.push(v)；yield_now().await；i += 1 }
    while i < count {
        let v = start + i * step;
        acc += v;
        log.borrow_mut().push(v);
        yield_now().await; // 让出点 = 状态机的一个 case
        i += 1;
    }
    seed + acc
}

// ── 自写最小 executor（std-only，无第三方 crate）──
// noop waker：本执行器靠 round-robin 重 poll 推进，wake 不需要做事。
fn noop_waker() -> Waker {
    fn no_op(_: *const ()) {}
    fn clone_fn(_: *const ()) -> RawWaker {
        RawWaker::new(std::ptr::null(), &VTABLE)
    }
    static VTABLE: RawWakerVTable = RawWakerVTable::new(clone_fn, no_op, no_op, no_op);
    unsafe { Waker::from_raw(RawWaker::new(std::ptr::null(), &VTABLE)) }
}

/// 把若干 Future round-robin poll 到全部完成，返回各自终值（按任务下标）。
/// 这正是 06.2 执行器的「编译器生成 Future」版——同一台机器。
fn run_all(mut tasks: Vec<Pin<Box<dyn Future<Output = u32>>>>) -> Vec<u32> {
    let w = noop_waker();
    let mut cx = Context::from_waker(&w);
    let mut results: Vec<Option<u32>> = vec![None; tasks.len()];
    let mut remaining = tasks.len();
    while remaining > 0 {
        for (i, t) in tasks.iter_mut().enumerate() {
            if results[i].is_some() {
                continue;
            }
            if let Poll::Ready(v) = t.as_mut().poll(&mut cx) {
                results[i] = Some(v);
                remaining -= 1;
            }
        }
    }
    results.into_iter().map(|x| x.unwrap()).collect()
}

// ═══════════════════════════ 测试 harness（勿改）═══════════════════════════

fn check_statemachine() -> bool {
    // 手写协程：start=10 step=10 count=3 → 让出 [10,20,30]，终值 = 0+60 = 60。
    let mut co = Stepper::new(0, 10, 10, 3);
    let mut seq = Vec::new();
    let mut final_v = None;
    for _ in 0..100 {
        match co.poll() {
            Step::Pending(v) => seq.push(v),
            Step::Ready(f) => {
                final_v = Some(f);
                break;
            }
        }
    }
    let mut ok = true;
    if seq != vec![10, 20, 30] {
        println!("YIELD_FAIL 让出序列={:?} 期望=[10, 20, 30]", seq);
        ok = false;
    } else {
        println!("YIELD_PASS");
    }
    // 状态机：终值正确，且恰好 count+1 次 poll 到 Ready（不多不少）。
    let polls = seq.len() + 1;
    if final_v == Some(60) && polls == 4 && co.is_done() {
        println!("STATEMACHINE_PASS");
    } else {
        println!("STATEMACHINE_FAIL 终值={:?} poll次数={} 期望终值=60 次数=4", final_v, polls);
        ok = false;
    }
    ok
}

fn check_exec() -> bool {
    // 两个协程交错：A=让出[1,2,3]，B=让出[10,20]。round-robin 一轮各一 poll。
    let mut tasks = [Stepper::new(0, 1, 1, 3), Stepper::new(0, 10, 10, 2)];
    let log = exec_run(&mut tasks);
    if log.trace == vec![1, 10, 2, 20, 3] {
        println!("EXEC_PASS");
        true
    } else {
        println!("EXEC_FAIL 交错序列={:?} 期望=[1, 10, 2, 20, 3]", log.trace);
        false
    }
}

fn check_batch() -> bool {
    // 全程不让出（count=0）：每个任务首 poll 即完成 → 退化为顺序批处理，无交错。
    let mut tasks = [Stepper::new(100, 0, 0, 0), Stepper::new(200, 0, 0, 0), Stepper::new(300, 0, 0, 0)];
    let log = exec_run(&mut tasks);
    if log.trace.is_empty() && log.finals == vec![100, 200, 300] {
        println!("BATCH_PASS");
        true
    } else {
        println!("BATCH_FAIL 让出={:?} 完成序={:?} 期望 让出=[] 完成=[100, 200, 300]", log.trace, log.finals);
        false
    }
}

fn check_waker() -> bool {
    // 三个任务，需 poll 次数 = count+1。总 poll 应 ≤ 唤醒数 + 任务数（无忙等）。
    let mut tasks = [Stepper::new(0, 1, 1, 2), Stepper::new(0, 5, 5, 1), Stepper::new(0, 7, 7, 3)];
    let n = tasks.len() as u32;
    let (polls, wakes) = reactor_run(&mut tasks);
    let all_done = tasks.iter().all(|t| t.is_done());
    if all_done && polls > 0 && polls <= wakes + n {
        println!("WAKER_PASS polls={} <= wakes({})+tasks({})", polls, wakes, n);
        true
    } else {
        println!(
            "WAKER_FAIL polls={} wakes={} tasks={} all_done={} （要求 0<polls<=wakes+tasks 且全完成）",
            polls, wakes, n, all_done
        );
        false
    }
}

fn check_async() -> bool {
    // async 版必须与 06.1 手写版逐位一致：让出 [10,20,30]，终值 60。
    let log = Rc::new(RefCell::new(Vec::new()));
    let fut: Pin<Box<dyn Future<Output = u32>>> = Box::pin(stepper_async(0, 10, 10, 3, log.clone()));
    let finals = run_all(vec![fut]);
    let seq = log.borrow().clone();
    if seq == vec![10, 20, 30] && finals == vec![60] {
        println!("ASYNC_PASS");
        true
    } else {
        println!("ASYNC_FAIL 让出={:?} 终值={:?} 期望 [10,20,30] / [60]", seq, finals);
        false
    }
}

fn check_join() -> bool {
    // join 两个 async 任务并发跑：与 06.2 手写执行器的交错本质一致。
    let la = Rc::new(RefCell::new(Vec::new()));
    let lb = Rc::new(RefCell::new(Vec::new()));
    let fa: Pin<Box<dyn Future<Output = u32>>> = Box::pin(stepper_async(0, 1, 1, 3, la.clone()));
    let fb: Pin<Box<dyn Future<Output = u32>>> = Box::pin(stepper_async(0, 10, 10, 2, lb.clone()));
    let finals = run_all(vec![fa, fb]);
    let sa = la.borrow().clone();
    let sb = lb.borrow().clone();
    if sa == vec![1, 2, 3] && sb == vec![10, 20] && finals == vec![6, 30] {
        println!("JOIN_PASS");
        true
    } else {
        println!("JOIN_FAIL A={:?} B={:?} finals={:?} 期望 [1,2,3]/[10,20]/[6,30]", sa, sb, finals);
        false
    }
}

fn main() {
    let mut all = true;
    all &= check_statemachine(); // 06.1
    all &= check_exec(); // 06.2
    all &= check_batch(); // 06.2
    all &= check_waker(); // 06.3
    all &= check_async(); // 06.4（辅助分）
    all &= check_join(); // 06.4（辅助分）

    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
