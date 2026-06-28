//! 16d · 核外中断 PLIC - Rust（参考解）。
//! 平台级共享外设中断路由器：priority/enable/threshold → claim/complete。
//! 多源仲裁：合格源 = pending & enable & (prio > threshold)，取最高优先级（同级取最小 id）。
//! env=host：纯逻辑直接跑；pending/enable 是一组源标志，用 bitflags 类型化。
#![allow(dead_code)]

use bitflags::bitflags;

bitflags! {
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    struct Sources: u32 {
        const S1 = 1 << 1;
        const S2 = 1 << 2;
        const S3 = 1 << 3;
        const S4 = 1 << 4;
    }
}

struct Plic {
    prio: [u32; 5],
    pending: Sources,
    enable: Sources,
    threshold: u32,
}

impl Plic {
    fn configured() -> Self {
        Plic {
            prio: [0, 1, 2, 3, 3],
            pending: Sources::empty(),
            enable: Sources::S1 | Sources::S2 | Sources::S3,
            threshold: 1,
        }
    }

    fn raise(&mut self, s: Sources) {
        self.pending.insert(s);
    }

    // 组合仲裁器：取最高优先级的合格源，同优先级取最小 id（自底向上 strict `>`）。
    fn arbitrate(&self) -> u32 {
        let mut best = 0u32;
        let mut bp = 0u32;
        for id in 1..=4u32 {
            let bit = Sources::from_bits_truncate(1u32 << id);
            if !self.pending.contains(bit) {
                continue;
            }
            if !self.enable.contains(bit) {
                continue;
            }
            let p = self.prio[id as usize];
            if p <= self.threshold {
                continue;
            }
            if p > bp {
                bp = p;
                best = id;
            }
        }
        best
    }

    fn claim(&mut self) -> u32 {
        let id = self.arbitrate();
        if id != 0 {
            self.pending.remove(Sources::from_bits_truncate(1u32 << id));
        }
        id
    }

    fn complete(&mut self, _id: u32) {}
}

fn level_route() -> bool {
    let mut p = Plic::configured();
    p.raise(Sources::all());
    let top = p.arbitrate();
    if top != 3 {
        return false;
    }
    println!("ROUTE_PASS top={top} prio={} (max-priority arbitration)", p.prio[top as usize]);
    true
}

fn level_thresh() -> bool {
    let mut a = Plic::configured();
    a.raise(Sources::S1); // prio1 ≤ 阈值1
    if a.arbitrate() != 0 {
        return false;
    }
    let mut b = Plic::configured();
    b.raise(Sources::S4); // prio3 但未使能
    if b.arbitrate() != 0 {
        return false;
    }
    let mut c = Plic::configured();
    c.raise(Sources::S2); // prio2 > 阈值且使能
    if c.arbitrate() != 2 {
        return false;
    }
    println!("THRESH_PASS threshold blocks s1, enable blocks s4, s2 eligible");
    true
}

fn level_claim() -> bool {
    let mut p = Plic::configured();
    p.raise(Sources::all());
    let c1 = p.claim();
    p.complete(c1);
    let c2 = p.claim();
    p.complete(c2);
    let c3 = p.claim();
    p.complete(c3);
    if c1 != 3 || c2 != 2 || c3 != 0 {
        return false;
    }
    if p.pending != (Sources::S1 | Sources::S4) {
        return false; // 余 {1,4} 被阈值/使能挡住
    }
    p.raise(Sources::S3); // 重新触发
    let c4 = p.claim();
    p.complete(c4);
    if c4 != 3 {
        return false;
    }
    println!("CLAIM_PASS seq={c1},{c2},{c3} refire={c4}");
    true
}

fn main() {
    let mut ok = true;
    ok &= level_route();
    ok &= level_thresh();
    ok &= level_claim();
    if ok {
        println!("ALL_PASS");
    } else {
        println!("SOME_FAIL");
        std::process::exit(1);
    }
}
