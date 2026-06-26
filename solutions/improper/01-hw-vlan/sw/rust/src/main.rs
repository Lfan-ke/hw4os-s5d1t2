//! 简化 VLAN Tag 处理 —— 软件路径（Rust 参考解）。
//! 包字: [31]VALID [30]HAS_TAG [29]DROP [28]DIR(in) [21:16]VID(6b) [15:0]PAYLOAD
//!
//! 学生版只需填 `process()` 函数体；下方测试 harness（向量 + PASS 打印）给好。

const VALID: u32 = 1 << 31;
const HAS_TAG: u32 = 1 << 30;
const DROP: u32 = 1 << 29;
const DIR: u32 = 1 << 28;
const VID_SH: u32 = 16;
const VID_MASK: u32 = 0x3F;

const ACCESS: u32 = 0;
const TRUNK: u32 = 1;
const HYBRID: u32 = 2;

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

/// 同一逻辑的“软件 if-else”实现（硬件路径是同一函数的组合逻辑）。
fn process(mode: u32, pvid: u32, allow: u64, untag: u64, inp: u32) -> u32 {
    let has_tag = inp & HAS_TAG != 0;
    let egress = inp & DIR != 0;
    let vid = ((inp >> VID_SH) & VID_MASK) as u64;
    if !egress {
        // 收包 ingress
        match mode {
            ACCESS => {
                if has_tag {
                    strip(inp)
                } else {
                    insert(inp, pvid)
                }
            }
            _ => {
                // Trunk / Hybrid 收包：必须带 tag 且 vid 在 allow 位图
                if !has_tag {
                    drop_pkt()
                } else if (allow >> vid) & 1 == 0 {
                    drop_pkt()
                } else {
                    keep(inp)
                }
            }
        }
    } else {
        // 发包 egress
        match mode {
            ACCESS => strip(inp),
            TRUNK => keep(inp),
            _ => {
                if has_tag && (untag >> vid) & 1 == 1 {
                    strip(inp)
                } else {
                    keep(inp)
                }
            }
        }
    }
}

// ───────────────────────── 测试 harness（给好，勿改）─────────────────────────

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
