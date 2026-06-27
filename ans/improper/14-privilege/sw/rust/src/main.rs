//! 三态特权机 —— 软件路径（Rust，参考解）。
//! 状态字 csr[4:0] = { saved_priv[4:3], feat_en[2], cur_priv[1:0] }
//! 操作 op = (kind, arg_priv, arg_en)；纯函数 step(csr, op) → (csr', trap)。
//!
//! 特权级：A(最高,≈M)=2，B(≈S)=1，C(最低,≈U)=0。
//! 与硬件 priv_gate / mkTbPriv 完全同构，输出逐位一致。

#![allow(dead_code)]

// 特权级编码（数值越大权限越高，比较器 cur >= need 直接成立）
const A: u32 = 2; // 最高
const B: u32 = 1;
const C: u32 = 0; // 最低

// 操作 kind
const NORMAL: u32 = 0; // 执行需 arg_priv 权限的普通指令
const DROP: u32 = 1; // 主动下放到 arg_priv
const ECALL: u32 = 2; // 陷入提权（合法）
const XRET: u32 = 3; // 从处理程序返回
const SETFEAT: u32 = 4; // 置/清功能使能位
const USEFEAT: u32 = 5; // 使用被门控的功能

// csr 取位 / 打包
#[inline]
fn cur_of(csr: u32) -> u32 {
    csr & 0b11
}
#[inline]
fn fe_of(csr: u32) -> u32 {
    (csr >> 2) & 1
}
#[inline]
fn sp_of(csr: u32) -> u32 {
    (csr >> 3) & 0b11
}
#[inline]
fn pack(cur: u32, fe: u32, sp: u32) -> u32 {
    ((sp & 0b11) << 3) | ((fe & 1) << 2) | (cur & 0b11)
}

/// 核心：与硬件 priv_gate 同构的纯逻辑。返回 (csr', trap)。
fn step(csr: u32, kind: u32, arg_priv: u32, arg_en: u32) -> (u32, bool) {
    let cur = cur_of(csr);
    let fe = fe_of(csr);
    let sp = sp_of(csr);

    // 默认：csr 不变、不陷入。各分支只改需要改的。
    let (mut ncur, mut nfe, mut nsp, mut trap) = (cur, fe, sp, false);

    match kind {
        // 子实验 1：特权比较器本体——“有没有权限”就是一根 cur < need 的线。
        // TODO[a]（推荐）：一行比较器。
        NORMAL => {
            trap = cur < arg_priv;
        }
        // ELSE[b]：把 3×3 等级关系展开成显式真值表（等价写法），形如
        //   match (cur, arg_priv) { (C,B)|(C,A)|(B,A) => trap=true, _ => {} }

        // 子实验 2：向下放权 = 写低位。A→B→C 自由下行；上行非法。
        DROP => {
            if arg_priv > cur {
                trap = true; // 不许直接提权
            } else {
                ncur = arg_priv; // 合法下放：把新值写进 cur_priv 触发器
            }
        }

        // 子实验 3：陷入提权 + 返回（SPP/xret 同构）。
        // ECALL：合法陷入，保存前态、跳到最高态。
        ECALL => {
            nsp = cur; // saved_priv ← cur_priv（同 mstatus.MPP/sstatus.SPP）
            ncur = A; // 进入最高态处理
        }
        // XRET：从处理程序返回，恢复 saved_priv；非最高态不得 xret。
        // TODO[a]：用 2 位 saved_priv 存完整前态（三态需要 2 位）。
        XRET => {
            if cur != A {
                trap = true; // 只有最高态能 xret
            } else {
                ncur = sp;
            }
        }

        // 子实验 4：开启功能也是置位。能力 = 特权够 且 使能位亮。
        SETFEAT => {
            if cur < B {
                trap = true; // 配置使能位至少要 B 态
            } else {
                nfe = arg_en;
            }
        }
        USEFEAT => {
            // 缺一不可：特权不足 或 使能位灭 → 陷入。
            trap = cur < arg_priv || fe == 0;
        }

        _ => {}
    }

    (pack(ncur, nfe, nsp), trap)
}

// ───────────────────────── 测试 harness（勿改）─────────────────────────

fn mkcsr(cur: u32, fe: u32, sp: u32) -> u32 {
    pack(cur, fe, sp)
}

struct Case {
    csr: u32,
    kind: u32,
    arg_priv: u32,
    arg_en: u32,
    exp_csr: u32,
    exp_trap: bool,
}

fn run_group(name: &str, cases: &[Case]) -> bool {
    let mut ok = true;
    for (i, c) in cases.iter().enumerate() {
        let (got_csr, got_trap) = step(c.csr, c.kind, c.arg_priv, c.arg_en);
        if got_csr != c.exp_csr || got_trap != c.exp_trap {
            println!(
                "{}_FAIL case#{} csr=0x{:02X} kind={} ap={} ae={} | exp(csr=0x{:02X},trap={}) got(csr=0x{:02X},trap={})",
                name, i, c.csr, c.kind, c.arg_priv, c.arg_en,
                c.exp_csr, c.exp_trap as u32, got_csr, got_trap as u32
            );
            ok = false;
        }
    }
    if ok {
        println!("{}_PASS", name);
    }
    ok
}

fn c(csr: u32, kind: u32, arg_priv: u32, arg_en: u32, exp_csr: u32, exp_trap: bool) -> Case {
    Case { csr, kind, arg_priv, arg_en, exp_csr, exp_trap }
}

fn main() {
    let mut all = true;

    // 子实验 1：特权比较器（CMP）
    all &= run_group("CMP", &[
        c(mkcsr(A, 0, 0), NORMAL, A, 0, mkcsr(A, 0, 0), false),
        c(mkcsr(C, 0, 0), NORMAL, A, 0, mkcsr(C, 0, 0), true),
        c(mkcsr(B, 0, 0), NORMAL, B, 0, mkcsr(B, 0, 0), false),
        c(mkcsr(B, 0, 0), NORMAL, A, 0, mkcsr(B, 0, 0), true),
        c(mkcsr(C, 0, 0), NORMAL, C, 0, mkcsr(C, 0, 0), false),
        c(mkcsr(A, 0, 0), NORMAL, C, 0, mkcsr(A, 0, 0), false),
    ]);

    // 子实验 2：向下放权（DROP）
    all &= run_group("DROP", &[
        c(mkcsr(A, 0, 0), DROP, B, 0, mkcsr(B, 0, 0), false),
        c(mkcsr(A, 0, 0), DROP, C, 0, mkcsr(C, 0, 0), false),
        c(mkcsr(B, 0, 0), DROP, C, 0, mkcsr(C, 0, 0), false),
        c(mkcsr(C, 0, 0), DROP, A, 0, mkcsr(C, 0, 0), true),
        c(mkcsr(B, 0, 0), DROP, A, 0, mkcsr(B, 0, 0), true),
        c(mkcsr(B, 0, 0), DROP, B, 0, mkcsr(B, 0, 0), false),
    ]);

    // 子实验 3：陷入提权 + 返回（TRAP）
    all &= run_group("TRAP", &[
        c(mkcsr(C, 0, 0), ECALL, 0, 0, mkcsr(A, 0, C), false),
        c(mkcsr(B, 0, 0), ECALL, 0, 0, mkcsr(A, 0, B), false),
        c(mkcsr(A, 0, B), XRET, 0, 0, mkcsr(B, 0, B), false),
        c(mkcsr(C, 0, 0), XRET, 0, 0, mkcsr(C, 0, 0), true),
        c(mkcsr(B, 0, 0), XRET, 0, 0, mkcsr(B, 0, 0), true),
        c(mkcsr(C, 1, 0), ECALL, 0, 0, mkcsr(A, 1, C), false),
    ]);

    // 子实验 4：开启功能也是置位（FEAT）
    all &= run_group("FEAT", &[
        c(mkcsr(A, 0, 0), SETFEAT, 0, 1, mkcsr(A, 1, 0), false),
        c(mkcsr(B, 0, 0), SETFEAT, 0, 1, mkcsr(B, 1, 0), false),
        c(mkcsr(C, 0, 0), SETFEAT, 0, 1, mkcsr(C, 0, 0), true),
        c(mkcsr(A, 1, 0), SETFEAT, 0, 0, mkcsr(A, 0, 0), false),
        c(mkcsr(C, 1, 0), USEFEAT, C, 0, mkcsr(C, 1, 0), false),
        c(mkcsr(C, 0, 0), USEFEAT, C, 0, mkcsr(C, 0, 0), true),
        c(mkcsr(A, 1, 0), USEFEAT, B, 0, mkcsr(A, 1, 0), false),
        c(mkcsr(C, 1, 0), USEFEAT, B, 0, mkcsr(C, 1, 0), true),
        c(mkcsr(B, 1, 0), USEFEAT, B, 0, mkcsr(B, 1, 0), false),
    ]);

    // 子实验 5：三态贯通小程序（CAPSTONE）。
    // 一条轨迹，csr 在各步之间串起来；harness 逐步喂、逐位校验。
    // A 启动 → DROP 到 B → SETFEAT 开功能 → DROP 到 C
    //        → C 态 USEFEAT 触发 trap（被内核接住）→ ECALL 提权 → XRET 返回
    {
        let traj: &[(u32, u32, u32, u32, bool)] = &[
            // (kind, arg_priv, arg_en, exp_csr, exp_trap)
            (DROP, B, 0, mkcsr(B, 0, 0), false),
            (SETFEAT, 0, 1, mkcsr(B, 1, 0), false),
            (DROP, C, 0, mkcsr(C, 1, 0), false),
            (USEFEAT, B, 0, mkcsr(C, 1, 0), true),
            (ECALL, 0, 0, mkcsr(A, 1, C), false),
            (XRET, 0, 0, mkcsr(C, 1, 0), false),
        ];
        let mut csr = mkcsr(A, 0, 0); // A 启动
        let mut ok = true;
        for (i, &(kind, ap, ae, exp_csr, exp_trap)) in traj.iter().enumerate() {
            let (ncsr, trap) = step(csr, kind, ap, ae);
            if ncsr != exp_csr || trap != exp_trap {
                println!(
                    "CAPSTONE_FAIL step#{} csr=0x{:02X} kind={} | exp(csr=0x{:02X},trap={}) got(csr=0x{:02X},trap={})",
                    i, csr, kind, exp_csr, exp_trap as u32, ncsr, trap as u32
                );
                ok = false;
            }
            csr = ncsr; // 串到下一步
        }
        if ok {
            println!("CAPSTONE_PASS");
        }
        all &= ok;
    }

    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
