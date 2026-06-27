//! 纤程（有栈协程）参考解 —— Rust（host / x86_64）。
use std::arch::global_asm;
use std::cell::RefCell;
use std::collections::VecDeque;
use std::rc::Rc;
use std::sync::mpsc::{channel, Receiver, Sender};
use std::thread;

// ── 上下文 + 手写上下文切换 ──
#[repr(C)]
struct Ctx { rsp: u64 }
impl Ctx { fn new() -> Ctx { Ctx { rsp: 0 } } }

global_asm!(
    ".globl switch_ctx",
    "switch_ctx:",
    "  push rbp", "  push rbx", "  push r12", "  push r13", "  push r14", "  push r15",
    "  mov [rdi], rsp",
    "  mov rsp, [rsi]",
    "  pop r15", "  pop r14", "  pop r13", "  pop r12", "  pop rbx", "  pop rbp",
    "  ret",
);
extern "C" { fn switch_ctx(old: *mut Ctx, new: *const Ctx); }

const STACK: usize = 64 * 1024;
enum St { Ready, Running, Done }

struct Fiber {
    ctx: Ctx,
    _stack: Vec<u8>,
    st: St,
    body: Option<Box<dyn FnMut()>>,
}

struct Rt {
    fibers: Vec<Fiber>,
    ready: VecDeque<usize>,
    cur: usize,
    main: Ctx,
}

static mut RT: *mut Rt = std::ptr::null_mut();
fn rt() -> &'static mut Rt { unsafe { &mut *RT } }

extern "C" fn trampoline() {
    let i = rt().cur;
    let mut body = rt().fibers[i].body.take().unwrap();
    body();
    rt().fibers[i].st = St::Done;
    let me: *mut Ctx = &mut rt().fibers[i].ctx;
    let main: *const Ctx = &rt().main;
    unsafe { switch_ctx(me, main); }
}

fn rt_init() {
    let b = Box::new(Rt { fibers: Vec::new(), ready: VecDeque::new(), cur: usize::MAX, main: Ctx::new() });
    unsafe { RT = Box::into_raw(b); }
}

fn spawn(f: Box<dyn FnMut()>) {
    let mut stack = vec![0u8; STACK];
    let base = stack.as_mut_ptr() as usize;
    let top = (base + STACK) & !0xf;
    let ret_slot = top - 16;
    unsafe {
        *(ret_slot as *mut u64) = trampoline as *const () as u64;
        for k in 1..=6 { *((ret_slot - 8 * k) as *mut u64) = 0; }
    }
    let rsp = (ret_slot - 48) as u64;
    let id = rt().fibers.len();
    rt().fibers.push(Fiber { ctx: Ctx { rsp }, _stack: stack, st: St::Ready, body: Some(f) });
    rt().ready.push_back(id);
}

fn yield_now() {
    let i = rt().cur;
    rt().fibers[i].st = St::Ready;
    let me: *mut Ctx = &mut rt().fibers[i].ctx;
    let main: *const Ctx = &rt().main;
    unsafe { switch_ctx(me, main); }
}

fn run() {
    let mut steps = 0u32;          // 安全阀：未实现 switch_ctx 时防止空转死循环
    loop {
        steps += 1;
        if steps > 100_000 { break; }
        let Some(i) = rt().ready.pop_front() else { break };
        rt().cur = i;
        rt().fibers[i].st = St::Running;
        let main: *mut Ctx = &mut rt().main;
        let f: *const Ctx = &rt().fibers[i].ctx;
        unsafe { switch_ctx(main, f); }
        if let St::Done = rt().fibers[i].st {} else { rt().ready.push_back(i); }
    }
}

type Log = Rc<RefCell<Vec<String>>>;
fn push(log: &Log, s: String) { log.borrow_mut().push(s); }

// ── 阶段 1：手写上下文切换 ping-pong ──
fn stage_ctxsw() -> bool {
    rt_init();
    let n = 3;
    let log: Log = Rc::new(RefCell::new(Vec::new()));
    let l1 = log.clone();
    spawn(Box::new(move || { for _ in 0..n { push(&l1, "PING".into()); yield_now(); } }));
    let l2 = log.clone();
    spawn(Box::new(move || { for _ in 0..n { push(&l2, "PONG".into()); yield_now(); } }));
    run();
    let v = log.borrow();
    for s in v.iter() { println!("{s}"); }
    let want: Vec<String> = (0..n).flat_map(|_| ["PING".to_string(), "PONG".to_string()]).collect();
    if *v == want { println!("CTXSW_PASS"); true } else { println!("CTXSW_FAIL got={:?}", *v); false }
}

// ── 阶段 2：spawn + yield + round-robin 调度 ──
fn stage_sched() -> bool {
    rt_init();
    let rounds = 2;
    let log: Log = Rc::new(RefCell::new(Vec::new()));
    for label in ['A', 'B', 'C'] {
        let l = log.clone();
        spawn(Box::new(move || {
            for r in 1..=rounds { push(&l, format!("{label}{r}")); yield_now(); }
        }));
    }
    run();
    let v = log.borrow();
    println!("{}", v.join(" "));
    let want = vec!["A1", "B1", "C1", "A2", "B2", "C2"];
    let got: Vec<&str> = v.iter().map(|s| s.as_str()).collect();
    if got == want { println!("SCHED_PASS"); true } else { println!("SCHED_FAIL got={:?}", got); false }
}

// ── 阶段 3：无让出 → 退化为顺序批处理 ──
fn stage_seq() -> bool {
    rt_init();
    let log: Log = Rc::new(RefCell::new(Vec::new()));
    for i in 0..3 {
        let l = log.clone();
        // 纯计算，无 yield：一口气跑完才轮到下一个
        spawn(Box::new(move || {
            let mut acc = 0u64; for k in 0..1000 { acc = acc.wrapping_add(k); }
            push(&l, format!("task{i}_done(sum={acc})"));
        }));
    }
    run();
    let v = log.borrow();
    println!("{}", v.join(" "));
    let ok = v.len() == 3
        && v[0].starts_with("task0_done")
        && v[1].starts_with("task1_done")
        && v[2].starts_with("task2_done");
    if ok { println!("SEQ_PASS"); true } else { println!("SEQ_FAIL got={:?}", *v); false }
}

// ── 阶段 4：插入让出点 → 交错执行 ──
fn stage_interleave() -> bool {
    rt_init();
    let log: Log = Rc::new(RefCell::new(Vec::new()));
    for i in 0..3 {
        let l = log.clone();
        spawn(Box::new(move || {
            push(&l, format!("{i}a")); yield_now(); // 模拟 I/O 阻塞处让出
            push(&l, format!("{i}b"));
        }));
    }
    run();
    let v = log.borrow();
    println!("{}", v.join(" "));
    // 交错判据：第一个 'b' 阶段产物出现前，三个任务的 'a' 阶段都已产出
    let first_b = v.iter().position(|s| s.ends_with('b')).unwrap_or(usize::MAX);
    let a_before = v[..first_b.min(v.len())].iter().filter(|s| s.ends_with('a')).count();
    if v.len() == 6 && a_before == 3 { println!("INTERLEAVE_PASS"); true }
    else { println!("INTERLEAVE_FAIL got={:?}", *v); false }
}

// ── 阶段 5：用 std 设施（线程做有栈协程）复现同序列 ──
struct Gen { resume: Option<Sender<()>>, out: Receiver<Option<i64>>, h: Option<thread::JoinHandle<()>> }
impl Gen {
    fn new(vals: Vec<i64>) -> Gen {
        let (rt_tx, rt_rx) = channel::<()>();
        let (yt_tx, yt_rx) = channel::<Option<i64>>();
        let h = thread::spawn(move || {
            // 线程 = 一根独立的栈 = 有栈协程；recv 阻塞即“被让出”，被 resume 唤醒即“切回”。
            for v in vals {
                if rt_rx.recv().is_err() { return; }     // resume 通道关闭 → 干净退出
                if yt_tx.send(Some(v)).is_err() { return; }
            }
            if rt_rx.recv().is_err() { return; }
            let _ = yt_tx.send(None);
        });
        Gen { resume: Some(rt_tx), out: yt_rx, h: Some(h) }
    }
    fn next(&self) -> Option<i64> { self.resume.as_ref().unwrap().send(()).unwrap(); self.out.recv().unwrap() }
}
impl Drop for Gen {
    fn drop(&mut self) {
        self.resume.take();                              // 先关 resume 通道，唤醒线程退出
        if let Some(h) = self.h.take() { let _ = h.join(); }
    }
}

fn stage_lib() -> bool {
    let a = Gen::new(vec![1, 2, 3]);
    let b = Gen::new(vec![10, 20]);
    // 交替 resume：A B A B A —— 与手写 round-robin 同序
    let mut seq = Vec::new();
    for &which in &[0, 1, 0, 1, 0] {
        let g = if which == 0 { &a } else { &b };
        if let Some(x) = g.next() { seq.push(x); }
    }
    println!("{:?}", seq);
    if seq == vec![1, 10, 2, 20, 3] { println!("LIB_PASS"); true } else { println!("LIB_FAIL got={:?}", seq); false }
}

fn main() {
    let mut all = true;
    all &= stage_ctxsw();
    all &= stage_sched();
    all &= stage_seq();
    all &= stage_interleave();
    all &= stage_lib();
    if all { println!("ALL_PASS"); } else { std::process::exit(1); }
}
