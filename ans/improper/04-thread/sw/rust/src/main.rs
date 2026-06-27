//! 04 · 线程管理：进程是线程的资源容器（Rust 参考解）。
//! 04-1 ctx：一个上下文 = CSR + GPRs；ctx_save / ctx_restore / ctx_switch。
//! 04-2 tcb：PCB→TCB——抽出共享资源 Process，Tcb 只存 ctx + 指向 Process 的指针；
//!           同进程两线程共享同一 fd/内存；调度器轮流 switch 多个 TCB。
//! 学生只填 4 个函数体；下方测试 harness（构造 + 不变量校验 + PASS 打印）勿改。
#![allow(dead_code, unused_variables)]

use std::cell::RefCell;
use std::rc::Rc;

const NGPR: usize = 31; // x1..x31
const NRES: usize = 8;

/// 一个上下文 = 一把寄存器 = 一个执行身份。
#[derive(Clone, Copy, PartialEq)]
struct Context {
    gpr: [u64; NGPR], // 通用寄存器 x1..x31
    sepc: u64,        // 异常返回 PC：该执行流“下一条指令”
    sstatus: u64,     // 状态 CSR（特权位/中断位等，占位）
    sp: u64,          // 栈指针（x2 单列，强调每线程独占栈）
}

impl Context {
    fn zero() -> Context {
        Context { gpr: [0; NGPR], sepc: 0, sstatus: 0, sp: 0 }
    }
}

/// 当前“虚拟 CPU”：它的寄存器现场就是此刻正在执行的那套上下文。
struct Vcpu {
    regs: Context,
}

// ── 04-1：上下文存取与切换（学生填）───────────────────────────────

/// 把当前 vCPU 的整套寄存器现场整存进 cur。
fn ctx_save(vcpu: &Vcpu, cur: &mut Context) {
    // ELSE[b]：整体赋值最直白（gpr 数组 + 三个 CSR 一并搬走）。
    *cur = vcpu.regs;
}

/// 把 next 的整套寄存器整载回 vCPU（与 save 相反）。
fn ctx_restore(vcpu: &mut Vcpu, next: &Context) {
    vcpu.regs = *next;
}

/// 协作式切换：先存当前现场到 cur，再把 next 载入当前。
fn ctx_switch(vcpu: &mut Vcpu, cur: &mut Context, next: &Context) {
    ctx_save(vcpu, cur);
    ctx_restore(vcpu, next);
}

// ── 04-2：PCB→TCB —— 共享资源结构 + 仅存上下文与指针的 TCB ─────────

/// 进程资源（全线程共享）：地址空间 / fd 表 / 信号量，统统占位。
struct SharedRes {
    mem: [u64; NRES], // 共享地址空间占位
    fd: [i64; NRES],  // fd 表占位（-1 = 空）
    sem: i64,         // 信号量计数占位
}

/// PCB = 资源容器。
struct Process {
    pid: u32,
    res: SharedRes,
}

/// TCB = 一把寄存器（ctx）+ 指向 PCB 的指针。无任何资源副本。
struct Tcb {
    tid: u32,
    ctx: Context,
    proc: Rc<RefCell<Process>>,
}

/// 造两个线程，让它们共享**同一个** Process（PCB＝资源容器）。
fn spawn_shared_pair(p: Rc<RefCell<Process>>, other: Rc<RefCell<Process>>) -> (Tcb, Tcb) {
    // TODO[a]：用 Rc<RefCell<_>>——两个 TCB 都 Rc::clone(&p)（只增引用计数，不复制资源）。
    let t1 = Tcb { tid: 1, ctx: Context::zero(), proc: Rc::clone(&p) };
    let t2 = Tcb { tid: 2, ctx: Context::zero(), proc: Rc::clone(&p) };
    (t1, t2)
}

// ───────────────────────── 测试 harness（勿改）─────────────────────────

fn ctx_demo(base: u64) -> Context {
    let mut c = Context::zero();
    for i in 0..NGPR {
        c.gpr[i] = base + i as u64;
    }
    c.sepc = base + 0x00E0;
    c.sstatus = base + 0x005A;
    c.sp = base + 0x5000;
    c
}

fn check_ctx() -> bool {
    let a_snapshot = ctx_demo(0xA000);
    let b_snapshot = ctx_demo(0xB000);
    let mut a = a_snapshot;
    let mut b = b_snapshot;

    // vCPU 先载入 A 上下文
    let mut vcpu = Vcpu { regs: Context::zero() };
    ctx_restore(&mut vcpu, &a);
    if vcpu.regs != a_snapshot {
        println!("CTX_SWAP_FAIL restore 后 vCPU 现场 != A（未整组载入）");
        return false;
    }

    // A→B：整组迁移，sepc 必须指向 B 执行流
    ctx_switch(&mut vcpu, &mut a, &b);
    if vcpu.regs != b_snapshot {
        println!("CTX_SWAP_FAIL switch 后 vCPU 现场 != B（上下文未整组迁移）");
        return false;
    }
    if vcpu.regs.sepc != b_snapshot.sepc {
        println!("CTX_SWAP_FAIL sepc 未随上下文迁移到 B 执行流");
        return false;
    }
    if a != a_snapshot {
        println!("CTX_SWAP_FAIL ctx_save 未把切换前的现场整存进 A");
        return false;
    }

    // 模拟 B 执行：改寄存器、推进 sepc；再切回 A
    vcpu.regs.gpr[5] = 0xDEAD;
    vcpu.regs.sepc += 4;
    let b_after = vcpu.regs;
    ctx_switch(&mut vcpu, &mut b, &a);
    if vcpu.regs != a_snapshot {
        println!("CTX_SWAP_FAIL 切回 A 后现场 != A");
        return false;
    }
    if b != b_after {
        println!("CTX_SWAP_FAIL 切回时未把 B 的最新现场存回 B");
        return false;
    }
    println!("CTX_SWAP_PASS");
    true
}

fn check_share(t1: &Tcb, t2: &Tcb) -> bool {
    // 线程1 通过它的 proc 指针写 fd[3] / mem[0] / sem
    {
        let mut p = t1.proc.borrow_mut();
        p.res.fd[3] = 99;
        p.res.mem[0] = 0xABCD;
        p.res.sem += 1;
    }
    // 线程2 通过它的 proc 指针读——同进程应当看见同一份
    let p = t2.proc.borrow();
    if p.res.fd[3] == 99 && p.res.mem[0] == 0xABCD && p.res.sem == 1 {
        println!("SHARE_PASS");
        true
    } else {
        println!("SHARE_FAIL 线程2 看不到线程1 写入的 fd/mem/sem（两线程未共享同一 Process）");
        false
    }
}

fn check_sched() -> bool {
    let proc = Rc::new(RefCell::new(Process {
        pid: 100,
        res: SharedRes { mem: [0; NRES], fd: [-1; NRES], sem: 0 },
    }));

    // 三个线程，各自独立上下文：sepc = 各自“程序计数器”起点；gpr[0] = tid 标记
    let starts = [1000u64, 2000u64, 3000u64];
    let mut ctxs: Vec<Context> = Vec::new();
    let mut tids: Vec<u32> = Vec::new();
    let mut tcbs: Vec<Tcb> = Vec::new();
    for k in 0..3usize {
        let mut c = Context::zero();
        c.sepc = starts[k];
        c.gpr[0] = 0x1000 + k as u64; // 独立标记，证明上下文互不污染
        ctxs.push(c);
        tids.push((k + 1) as u32);
        tcbs.push(Tcb { tid: (k + 1) as u32, ctx: c, proc: Rc::clone(&proc) });
    }

    // 协作式轮转调度：复用 04-1 的 ctx_switch。
    let mut vcpu = Vcpu { regs: Context::zero() };
    ctx_restore(&mut vcpu, &ctxs[0]);
    let mut trace: Vec<u32> = vec![tids[0]];
    let mut cur = 0usize;
    let rounds = 6;
    for _ in 0..rounds {
        vcpu.regs.sepc += 4; // 当前线程执行一步：推进它的 PC
        let next = (cur + 1) % ctxs.len();
        let nxt = ctxs[next]; // Context: Copy，避开同时可变借用两个元素
        ctx_switch(&mut vcpu, &mut ctxs[cur], &nxt);
        cur = next;
        trace.push(tids[cur]);
    }

    let mut ok = true;
    let want_trace = vec![1u32, 2, 3, 1, 2, 3, 1];
    if trace != want_trace {
        println!("SCHED_FAIL 轮转执行序错: got={:?} want={:?}", trace, want_trace);
        ok = false;
    }
    // 每个线程被调度 2 次，各推进 8；标记不变
    for k in 0..3usize {
        if ctxs[k].sepc != starts[k] + 8 {
            println!("SCHED_FAIL 线程{} sepc={} 应={}（上下文未独立推进）", k + 1, ctxs[k].sepc, starts[k] + 8);
            ok = false;
        }
        if ctxs[k].gpr[0] != 0x1000 + k as u64 {
            println!("SCHED_FAIL 线程{} 标记被污染（上下文不独立）", k + 1);
            ok = false;
        }
    }
    if ok {
        println!("SCHED_PASS");
    }
    ok
}

fn main() {
    let mut all = true;

    // 04-1：上下文存取与切换
    all &= check_ctx();

    // 04-2 共享：两线程共享同一 Process（另给一个无关进程 q 做对照）
    let p = Rc::new(RefCell::new(Process {
        pid: 7,
        res: SharedRes { mem: [0; NRES], fd: [-1; NRES], sem: 0 },
    }));
    let q = Rc::new(RefCell::new(Process {
        pid: 8,
        res: SharedRes { mem: [0; NRES], fd: [-1; NRES], sem: 0 },
    }));
    let (t1, t2) = spawn_shared_pair(Rc::clone(&p), q);
    all &= check_share(&t1, &t2);

    // 04-2 调度：轮流切换多个 TCB
    all &= check_sched();

    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
