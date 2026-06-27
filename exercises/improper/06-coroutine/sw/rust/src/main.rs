//! 无栈协程：被 poll 出来的「状态机」绿色线程 —— Rust。
//!
//! 主线：顺序代码 → 状态机 → 谁来生成这台状态机。
//!   06.1 手写「暂停—恢复」状态机（poll 的本质）  → YIELD_PASS / STATEMACHINE_PASS
//!   06.2 极简协作执行器（合作式调度 / 退化批处理）→ EXEC_PASS / BATCH_PASS
//!   06.3 就绪与唤醒（别空转 busy-poll）           → WAKER_PASS
//!   06.4 让编译器替你写状态机（async/await + join）→ ASYNC_PASS / JOIN_PASS（辅助分）
//!
//! 与「有栈协程（05-fiber）」对照：那边换的是栈指针，这边换的是状态号。
//! 「无栈」的肉身体验：凡是跨让出点还要活着的局部（i / acc），都必须放进协程结构体，
//! 而不是函数栈上——因为根本没有「每任务一根独立栈」。
//!
//! 你只需填带 TODO 的函数体；下方测试 harness 勿改。
#![allow(unused_variables, unused_imports, unused_mut, dead_code)]

use std::cell::RefCell;
use std::collections::VecDeque;
use std::future::Future;
use std::pin::Pin;
use std::rc::Rc;
use std::task::{Context, Poll, RawWaker, RawWakerVTable, Waker};

// ════════════════════════ 公共数据模型 ════════════════════════

/// poll 的返回：要么让出一个值（Pending），要么结束并给出终值（Ready）。
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Step {
    Pending(u32),
    Ready(u32),
}

/// 一台无栈协程：从 start 起、步长 step、共让出 count 次，终值 = seed + 让出值之和。
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
    /// poll 一次：把状态机往前推一步。状态号 = self.i。
    fn poll(&mut self) -> Step {
        // TODO[a] 显式状态字（i 即状态）+ 分支：
        //   若 self.i < self.count：算 v = start + i*step；acc += v；i += 1；返回 Pending(v)。
        //   否则：置 self.done = true；返回 Ready(seed + acc)。
        // HINT: acc/i 必须留在 self 里——它们要跨让出点存活（这就是「无栈」）。
        // ELSE[b] 也可用 protothread 宏（Duff's device，用 __LINE__ 自动生成 case），见 C 版注释。
        self.done = true;
        Step::Ready(0) // ← 占位：直接结束、不让出，会判 FAIL
    }
}

// ════════════════════════ 06.2 极简协作执行器（学生填）════════════════════════

#[derive(Default)]
struct RunLog {
    trace: Vec<u32>,  // 各任务让出值，按 poll 顺序交错
    finals: Vec<u32>, // 各任务终值，按完成顺序
}

/// round-robin 依次 poll 每个未完成任务：Pending 留在队里、记下让出值；
/// Ready 出队、记下终值。直到全部完成。
fn exec_run(tasks: &mut [Stepper]) -> RunLog {
    let mut log = RunLog::default();
    // TODO[a] 简单数组轮询：外层 while 还有未完成任务，内层 for 扫一遍所有任务，
    //   未完成的各 poll 一次：Pending → log.trace.push(v)；Ready → log.finals.push(f) 并记完成。
    // HINT: 用一个 remaining 计数控制外层循环。
    // ELSE[b] 也可维护显式就绪队列（VecDeque）。
    log // ← 占位：空账本，会判 FAIL
}

// ════════════════════════ 06.3 就绪与唤醒（学生填）════════════════════════

/// 玩具 reactor + waker：每个 Pending 只「登记一次唤醒」，执行器只重 poll 被唤醒者。
/// 返回 (总 poll 次数, 总唤醒次数)。
fn reactor_run(tasks: &mut [Stepper]) -> (u32, u32) {
    let mut polls = 0u32;
    let mut wakes = 0u32;
    // TODO[a] 极简就绪队列：ready 初始装入全部任务下标（首次 poll，不计唤醒）。
    //   循环出队一个 id，poll 它，polls += 1；
    //   若 Pending → wakes += 1 且把 id 重新入队（reactor 登记一次唤醒）；
    //   若 Ready → 不再入队。队空即止。
    // HINT: 这样 总 poll = 任务数 + 唤醒数，绝不忙等。
    // ELSE[b] 也可用 ready-flag 位图：只置位的才 poll。
    (polls, wakes) // ← 占位：什么都没跑，任务未完成会判 FAIL
}

// ════════════════════════ 06.4 让编译器替你写状态机（async/await）════════════════════════

/// 一个只让出一次的 Future：第一次 poll 返回 Pending，第二次 Ready。
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
async fn stepper_async(seed: u32, start: u32, step: u32, count: u32, log: Rc<RefCell<Vec<u32>>>) -> u32 {
    // TODO: 把 06.1 那段顺序代码原样写出来：
    //   let mut acc = 0; let mut i = 0;
    //   while i < count { let v = start + i*step; acc += v; log.borrow_mut().push(v);
    //                     yield_now().await; i += 1; }
    //   返回 seed + acc。
    // HINT: 编译器会把这段顺序代码掰成状态机——acc/i 自动塞进生成的 Future，与手写 enum 同构。
    let _ = (start, step, count, &log);
    seed // ← 占位：没让出、没累加，会判 ASYNC_FAIL
}

// ── 自写最小 executor（std-only，无第三方 crate）──
fn noop_waker() -> Waker {
    fn no_op(_: *const ()) {}
    fn clone_fn(_: *const ()) -> RawWaker {
        RawWaker::new(std::ptr::null(), &VTABLE)
    }
    static VTABLE: RawWakerVTable = RawWakerVTable::new(clone_fn, no_op, no_op, no_op);
    unsafe { Waker::from_raw(RawWaker::new(std::ptr::null(), &VTABLE)) }
}

/// 把若干 Future round-robin poll 到全部完成，返回各自终值（按任务下标）。
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
