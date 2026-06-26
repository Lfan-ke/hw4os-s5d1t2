//! 简化 VLAN Tag 处理 —— 软件路径（Rust）。
//! 包字: [31]VALID [30]HAS_TAG [29]DROP [28]DIR(in) [21:16]VID(6b) [15:0]PAYLOAD
//!
//! 你只需填 `process()` 函数体；下方测试 harness（向量 + PASS 打印）勿改。

#![allow(dead_code)]

const VALID: u32 = 1 << 31;
const HAS_TAG: u32 = 1 << 30;
const DROP: u32 = 1 << 29;
const DIR: u32 = 1 << 28;
const VID_SH: u32 = 16;
const VID_MASK: u32 = 0x3F;

const ACCESS: u32 = 0;
const TRUNK: u32 = 1;
const HYBRID: u32 = 2;

// 四个基本操作（建议直接复用）
#[inline]
fn strip(inp: u32) -> u32 {
    VALID | (inp & 0xFFFF)
}
#[inline]
fn insert(inp: u32, pvid: u32) -> u32 {
    VALID | HAS_TAG | ((pvid & VID_MASK) << VID_SH) | (inp & 0xFFFF)
}
#[inline]
fn keep(inp: u32) -> u32 {
    VALID | (inp & (HAS_TAG | (VID_MASK << VID_SH) | 0xFFFF))
}
#[inline]
fn drop_pkt() -> u32 {
    VALID | DROP
}

/// 与硬件 vlan_proc 同构的“软件 if-else”实现。
fn process(mode: u32, pvid: u32, allow: u64, untag: u64, inp: u32) -> u32 {
    let has_tag = inp & HAS_TAG != 0;
    let egress = inp & DIR != 0;
    let vid = ((inp >> VID_SH) & VID_MASK) as u64;
    let _ = (mode, pvid, allow, untag, has_tag, egress, vid);

    // TODO: 按 README §3 真值表实现 process（复用 strip/insert/keep/drop_pkt）：
    //   收包(ingress, !egress)
    //     Access: has_tag ? strip(inp) : insert(inp, pvid)
    //     Trunk/Hybrid: !has_tag → drop；allow[vid]==0 → drop；否则 keep(inp)
    //   发包(egress)
    //     Access: strip(inp)；Trunk: keep(inp)
    //     Hybrid: has_tag && untag[vid] ? strip(inp) : keep(inp)
    // HINT: allow[vid] 用 (allow >> vid) & 1 取位。
    0 // ← 占位：删掉它，返回正确的 out_pkt
}

// ───────────────────────── 测试 harness（勿改）─────────────────────────

fn mk(dir: u32, tag: u32, vid: u32, pl: u32) -> u32 {
    VALID
        | (if dir != 0 { DIR } else { 0 })
        | (if tag != 0 { HAS_TAG } else { 0 })
        | ((vid & VID_MASK) << VID_SH)
        | (pl & 0xFFFF)
}

fn run_group(name: &str, mode: u32, pvid: u32, allow: u64, untag: u64, cases: &[(u32, u32)]) -> bool {
    let mut ok = true;
    for (i, (inp, exp)) in cases.iter().enumerate() {
        let got = process(mode, pvid, allow, untag, *inp);
        if got != *exp {
            println!(
                "{}_FAIL case#{} in=0x{:08X} exp=0x{:08X} got=0x{:08X}",
                name, i, inp, exp, got
            );
            ok = false;
        }
    }
    if ok {
        println!("{}_PASS", name);
    }
    ok
}

fn main() {
    let allow: u64 = (1u64 << 10) | (1u64 << 20);
    let untag: u64 = 1u64 << 10;
    let mut all = true;

    all &= run_group(
        "ACCESS",
        ACCESS,
        5,
        0,
        0,
        &[
            (mk(0, 1, 10, 0x1234), VALID | 0x1234),
            (mk(0, 0, 0, 0x1234), VALID | HAS_TAG | (5 << 16) | 0x1234),
            (mk(1, 1, 10, 0x1234), VALID | 0x1234),
        ],
    );
    all &= run_group(
        "TRUNK",
        TRUNK,
        0,
        allow,
        0,
        &[
            (mk(0, 1, 10, 0xABCD), VALID | HAS_TAG | (10 << 16) | 0xABCD),
            (mk(0, 1, 30, 0x1111), VALID | DROP),
            (mk(0, 0, 0, 0x2222), VALID | DROP),
            (mk(1, 1, 10, 0xABCD), VALID | HAS_TAG | (10 << 16) | 0xABCD),
        ],
    );
    all &= run_group(
        "HYBRID",
        HYBRID,
        0,
        allow,
        untag,
        &[
            (mk(0, 1, 20, 0x0F0F), VALID | HAS_TAG | (20 << 16) | 0x0F0F),
            (mk(0, 1, 30, 0x3333), VALID | DROP),
            (mk(1, 1, 10, 0x0F0F), VALID | 0x0F0F),
            (mk(1, 1, 20, 0x0F0F), VALID | HAS_TAG | (20 << 16) | 0x0F0F),
        ],
    );

    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
