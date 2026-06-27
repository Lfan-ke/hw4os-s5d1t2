//! 进程调度（软件模拟）—— Rust 参考解。
//! 调度 = 存储结构（谁在就绪）+ 选择策略（选谁）。三段逐题递进：
//!   FIFO → 约束调度（GHOST 永不上、B 恒在 A 后、其余任意）→ 优先级（优先队列）。
//! 学生只填三个函数体；下方测试 harness（向量 + 不变量校验 + PASS 打印）勿改。

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
    procs.iter().map(|p| p.pid).collect()
}

/// 约束调度：去掉 GHOST；保证 B 恒在 A 之后；其余进程各出现一次（顺序任意）。
fn run_constrained(procs: &[Proc]) -> Vec<u32> {
    let mut order: Vec<u32> = procs
        .iter()
        .filter(|p| p.tag != GHOST)
        .map(|p| p.pid)
        .collect();
    let a = procs.iter().find(|p| p.tag == TAG_A).map(|p| p.pid);
    let b = procs.iter().find(|p| p.tag == TAG_B).map(|p| p.pid);
    if let (Some(a), Some(b)) = (a, b) {
        let pa = order.iter().position(|&x| x == a).unwrap();
        let pb = order.iter().position(|&x| x == b).unwrap();
        if pb < pa {
            order.remove(pb);
            let pa2 = order.iter().position(|&x| x == a).unwrap();
            order.insert(pa2 + 1, b);
        }
    }
    order
}

/// 优先级调度：优先级高者先出队，平级按到达顺序（FIFO 兜底）。
fn run_priority(procs: &[Proc]) -> Vec<u32> {
    let mut idx: Vec<usize> = (0..procs.len()).collect();
    idx.sort_by(|&i, &j| procs[j].prio.cmp(&procs[i].prio).then(i.cmp(&j)));
    idx.iter().map(|&i| procs[i].pid).collect()
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
    // 无 GHOST
    for p in procs.iter().filter(|p| p.tag == GHOST) {
        if order.contains(&p.pid) {
            println!("SCHED_FAIL ghost pid={} 不应被调度", p.pid);
            ok = false;
        }
    }
    // 非 GHOST 各一次
    for p in procs.iter().filter(|p| p.tag != GHOST) {
        if order.iter().filter(|&&x| x == p.pid).count() != 1 {
            println!("SCHED_FAIL pid={} 应恰好出现一次", p.pid);
            ok = false;
        }
    }
    // B 在 A 之后
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

    // 1) FIFO
    let fifo = [
        Proc { pid: 10, prio: 0, tag: 0 },
        Proc { pid: 20, prio: 0, tag: 0 },
        Proc { pid: 30, prio: 0, tag: 0 },
        Proc { pid: 40, prio: 0, tag: 0 },
    ];
    all &= check_fifo(&fifo, &run_fifo(&fifo));

    // 2) 约束（含 GHOST，且 B 先于 A 到达，迫使实现真的把 B 挪到 A 后）
    let con = [
        Proc { pid: 1, prio: 0, tag: 0 },
        Proc { pid: 2, prio: 0, tag: GHOST },
        Proc { pid: 3, prio: 0, tag: TAG_B },
        Proc { pid: 4, prio: 0, tag: TAG_A },
        Proc { pid: 5, prio: 0, tag: 0 },
    ];
    all &= check_constrained(&con, &run_constrained(&con));

    // 3) 优先级（平级 20 先于 40 到达）
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
