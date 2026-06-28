//! 16e · 中断聚合 + 多核仲裁 - Rust（参考解，类型化原子）。
//! 同一聚合场景（CLINT IPI + PLIC 多核 claim 竞争），把 C 的 amoswap.w/fence 升级为
//! 类型化原子：AtomicU32::swap(Ordering) 抢 gateway、fence(Release/Acquire) 管 IPI 内存序。
//! env=host：纯逻辑直接跑；N hart 用顺序遍历建模，原子/序语义是真的。

use std::sync::atomic::{fence, AtomicU32, Ordering};

const N_HART: usize = 4;
const IRQ_ID: u32 = 7;
const WINNER: usize = 0;
const PAYLOAD: u32 = 0x0000_ABCD;

// PLIC gateway + per-hart context。
struct Plic {
    inflight: AtomicU32,        // 0 空闲 / 1 已 claim 未 complete
    pending: bool,              // 设备在拉 IRQ
    enabled: [bool; N_HART],    // 每 hart-context 使能（prio>thresh）
}

impl Plic {
    // claim = AtomicU32::swap 原子拿 gateway：只有把 inflight 从 0 换成 1 的 hart 拿到 IRQ_ID。
    fn claim(&self, hart: usize) -> u32 {
        if !self.enabled[hart] || !self.pending {
            return 0;
        }
        if self.inflight.swap(1, Ordering::AcqRel) == 0 {
            IRQ_ID
        } else {
            0
        }
    }
    // complete = 还 gateway：Release 序写回 0，源可再次投递。
    fn complete(&self, id: u32) {
        if id == IRQ_ID {
            self.inflight.store(0, Ordering::Release);
        }
    }
}

fn phase_arbiter(plic: &Plic) -> bool {
    if plic.claim(WINNER) != IRQ_ID {
        return false;
    }
    println!("CLAIM_PASS  hart{WINNER} AtomicU32::swap 抢到 gateway，claim=IRQ{IRQ_ID}");

    let mut winners = 1usize;
    let mut who = WINNER;
    for h in 0..N_HART {
        if h == WINNER {
            continue;
        }
        if plic.claim(h) == IRQ_ID {
            winners += 1;
            who = h;
        }
    }
    if winners != 1 || who != WINNER {
        return false;
    }
    println!(
        "ARBITER_PASS 同一 IRQ{IRQ_ID} 仅 hart{WINNER} 处理：{} 个竞争者 claim 到 0",
        N_HART - 1
    );
    true
}

fn phase_complete(plic: &Plic) -> bool {
    // 漏 complete：gateway 仍 inflight，再 claim 读 0（中断丢失 / 设备卡住）。
    if plic.claim(WINNER) != 0 {
        return false;
    }
    plic.complete(IRQ_ID);
    // complete 后 gateway 重新武装，再 claim 又能拿到 IRQ_ID。
    if plic.claim(WINNER) != IRQ_ID {
        return false;
    }
    plic.complete(IRQ_ID);
    println!("COMPLETE_PASS gateway complete 后重新武装（漏 complete 则源永久卡住）");
    true
}

// send_ipi = 写目标 hart 的 MSIP，点亮其软件中断（CLINT 写 MSIP=敲 IPI）。
fn send_ipi(msip: &[AtomicU32], target: usize) {
    msip[target].store(1, Ordering::Relaxed);
}

fn phase_ipi() -> bool {
    let msip: [AtomicU32; N_HART] = Default::default();
    let payload = AtomicU32::new(0);
    let mut seen = [0u32; N_HART];

    // 生产者 hart0：先写共享数据，Release fence，再敲 IPI（写 MSIP）。
    payload.store(PAYLOAD, Ordering::Relaxed);
    fence(Ordering::Release);
    for t in 0..N_HART {
        if t != WINNER {
            send_ipi(&msip, t);
        }
    }

    // 消费者各 hart：见 MSIP→Acquire fence→读到 fence 之前写入的 payload→清自己的 MSIP。
    let mut woken = 0usize;
    for t in 0..N_HART {
        if t == WINNER {
            continue;
        }
        if msip[t].load(Ordering::Relaxed) == 1 {
            fence(Ordering::Acquire);
            seen[t] = payload.load(Ordering::Relaxed);
            msip[t].store(0, Ordering::Relaxed);
            woken += 1;
        }
    }
    if woken != N_HART - 1 || msip[WINNER].load(Ordering::Relaxed) != 0 {
        return false;
    }
    for t in 0..N_HART {
        if t != WINNER && (seen[t] != PAYLOAD || msip[t].load(Ordering::Relaxed) != 0) {
            return false;
        }
    }
    println!(
        "IPI_PASS    hart{WINNER} 经 barrier 敲 {} 路 IPI，从核见 MSIP 读到 payload={PAYLOAD:04X}",
        N_HART - 1
    );
    true
}

fn main() {
    let plic = Plic {
        inflight: AtomicU32::new(0),
        pending: true,
        enabled: [true; N_HART],
    };
    let mut ok = true;
    ok &= phase_arbiter(&plic);
    ok &= phase_complete(&plic);
    ok &= phase_ipi();
    if ok {
        println!("ALL_PASS");
    } else {
        println!("SOME_FAIL");
        std::process::exit(1);
    }
}
