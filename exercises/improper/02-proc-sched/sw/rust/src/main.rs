//! 进程管理（软件模拟调度）—— Rust。
//! 调度 = 存储结构（谁在就绪）+ 选择策略（选谁）。三段逐题递进。
//! 你只需填三个函数体；下方测试 harness 勿改。
#![allow(unused_variables, dead_code)]

#[derive(Clone, Copy)]
struct Proc {
    pid: u32,
    prio: u32,
    tag: u32, // 0=普通 1=GHOST 2=A 3=B
}

const GHOST: u32 = 1;
const TAG_A: u32 = 2;
const TAG_B: u32 = 3;

// ── 三段核心逻辑（学生填）──────────────────────────────────────────

/// FIFO：按到达（输入）顺序返回各进程 pid。
fn run_fifo(procs: &[Proc]) -> Vec<u32> {
    // TODO: 最朴素的 FIFO——把 procs 的 pid 按原顺序收集。
    // HINT: procs.iter().map(|p| p.pid).collect()
    Vec::new() // ← 占位
}

/// 约束调度：去掉 GHOST；保证 B 恒在 A 之后；其余进程各出现一次。
fn run_constrained(procs: &[Proc]) -> Vec<u32> {
    // TODO: 先 filter 掉 tag==GHOST；再保证 tag==TAG_B 的 pid 排在 tag==TAG_A 的 pid 之后。
    // HINT: 若 B 当前在 A 之前，把 B 删掉再插到 A 之后即可。
    // 也可分支择一：
    //   // TODO[a] 入队门控：A 未完成前不把 B 放进就绪
    //   // ELSE[b] 选择时挪后：先收集再把 B 移到 A 之后
    Vec::new() // ← 占位
}

/// 优先级调度：优先级高者先出队，平级按到达顺序（FIFO 兜底）。
fn run_priority(procs: &[Proc]) -> Vec<u32> {
    // TODO: 按 prio 降序输出 pid，平级保持到达顺序（稳定排序）。
    // HINT: 对下标做 sort_by(prio 降序, 平级 idx 升序)。
    Vec::new() // ← 占位
}

// ───────────────────────── 测试 harness（勿改）─────────────────────────

fn arrival_index(procs: &[Proc], pid: u32) -> usize {
    procs.iter().position(|p| p.pid == pid).unwrap_or(usize::MAX)
}

fn check_fifo(procs: &[Proc], order: &[u32]) -> bool {
    let want: Vec<u32> = procs.iter().map(|p| p.pid).collect();
    if order != want.as_slice() {
        println!("FIFO_FAIL want={:?} got={:?}", want, order);
        return false;
    }
    println!("FIFO_PASS");
    true
}

fn check_constrained(procs: &[Proc], order: &[u32]) -> bool {
    let mut ok = true;
    for p in procs.iter().filter(|p| p.tag == GHOST) {
        if order.contains(&p.pid) {
            println!("SCHED_FAIL ghost pid={} 不应被调度", p.pid);
            ok = false;
        }
    }
    for p in procs.iter().filter(|p| p.tag != GHOST) {
        if order.iter().filter(|&&x| x == p.pid).count() != 1 {
            println!("SCHED_FAIL pid={} 应恰好出现一次", p.pid);
            ok = false;
        }
    }
    let a = procs.iter().find(|p| p.tag == TAG_A).map(|p| p.pid);
    let b = procs.iter().find(|p| p.tag == TAG_B).map(|p| p.pid);
    if let (Some(a), Some(b)) = (a, b) {
        let pa = order.iter().position(|&x| x == a);
        let pb = order.iter().position(|&x| x == b);
        match (pa, pb) {
            (Some(pa), Some(pb)) if pa < pb => {}
            _ => {
                println!("SCHED_FAIL B(pid={}) 必须在 A(pid={}) 之后", b, a);
                ok = false;
            }
        }
    }
    if ok {
        println!("SCHED_PASS");
    }
    ok
}

fn check_priority(procs: &[Proc], order: &[u32]) -> bool {
    let prio = |pid: u32| procs.iter().find(|p| p.pid == pid).map(|p| p.prio).unwrap_or(0);
    let mut ok = true;
    for i in 1..order.len() {
        let (a, b) = (order[i - 1], order[i]);
        if prio(b) > prio(a) {
            println!("PRIO_FAIL 优先级非单调: pid={}(p{}) 在 pid={}(p{}) 之后", b, prio(b), a, prio(a));
            ok = false;
        } else if prio(b) == prio(a) && arrival_index(procs, b) < arrival_index(procs, a) {
            println!("PRIO_FAIL 平级未按到达顺序: pid={} 应在 pid={} 之前", b, a);
            ok = false;
        }
    }
    if order.len() != procs.len() {
        println!("PRIO_FAIL 应调度全部 {} 个进程", procs.len());
        ok = false;
    }
    if ok {
        println!("PRIO_PASS");
    }
    ok
}

fn main() {
    let mut all = true;

    let fifo = [
        Proc { pid: 10, prio: 0, tag: 0 },
        Proc { pid: 20, prio: 0, tag: 0 },
        Proc { pid: 30, prio: 0, tag: 0 },
        Proc { pid: 40, prio: 0, tag: 0 },
    ];
    all &= check_fifo(&fifo, &run_fifo(&fifo));

    let con = [
        Proc { pid: 1, prio: 0, tag: 0 },
        Proc { pid: 2, prio: 0, tag: GHOST },
        Proc { pid: 3, prio: 0, tag: TAG_B },
        Proc { pid: 4, prio: 0, tag: TAG_A },
        Proc { pid: 5, prio: 0, tag: 0 },
    ];
    all &= check_constrained(&con, &run_constrained(&con));

    let pri = [
        Proc { pid: 10, prio: 1, tag: 0 },
        Proc { pid: 20, prio: 3, tag: 0 },
        Proc { pid: 30, prio: 2, tag: 0 },
        Proc { pid: 40, prio: 3, tag: 0 },
    ];
    all &= check_priority(&pri, &run_priority(&pri));

    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
