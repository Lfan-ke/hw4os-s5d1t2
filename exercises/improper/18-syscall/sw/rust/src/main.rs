//! 系统调用：从 MCU 中断向量表 → MPU 系统调用（Rust）。
//!
//! 一条演化链，四段递进（全在本程序里跑）：
//!   S1 向量分发器   —— 硬件按号查表的间接跳转（与 hw/v、hw/bsv 同构）
//!   S2 Trap 上下文  —— 陷入时整存寄存器现场、返回时 sepc+=4 整取
//!   S3 Syscall ABI  —— a7=号、a0..a5=参、a0=返，分发表 + 三个 handler
//!   S4 真实 syscall —— 内联汇编真的陷入内核（host x86-64 `syscall` 指令）
//!
//! 你只需填【STUDENT】标注的函数体；下方 harness（向量 + 校验 + PASS 打印）勿改。
//! 判题：每组打印 *_PASS，全过再 ALL_PASS；任何 *_FAIL 即挂。
#![allow(unused_variables, dead_code, unused_mut, unused_imports)]

use std::arch::asm;

// ════════════════════════════════════════════════════════════════════
// S1 · 硬件向量分发器（MCU 模型）
// ════════════════════════════════════════════════════════════════════

/// 向量表：第 cause 项 = base + 4*cause（= 一张「按号存地址」的跳转表）。给定。
fn build_vector(base: u64) -> [u64; 16] {
    let mut v = [0u64; 16];
    let mut i = 0usize;
    while i < 16 {
        v[i] = base + (i as u64) * 4;
        i += 1;
    }
    v
}

/// 【STUDENT】组合分发：返回 (handler_pc, accept)。
fn dispatch(mode: u32, base: u64, cause: u32, trap_req: bool) -> (u64, bool) {
    // TODO: vectored(mode==1) → handler_pc = base + 4*cause（用 build_vector 查表）
    //       direct(mode==0)   → handler_pc = base；accept = trap_req
    // HINT: let v = build_vector(base);
    //       let pc = if mode == 1 { v[(cause & 0xF) as usize] } else { base };
    // 也可分支择一：
    //   // TODO[a] 只实现向量化公式即可过；
    //   // ELSE[b] 额外把 direct 也独立接好。
    (0, false) // ← 占位：恒错，判 FAIL
}

// ════════════════════════════════════════════════════════════════════
// S2 · Trap 上下文保存 / 恢复（the trap frame）
// ════════════════════════════════════════════════════════════════════

#[derive(Clone, Copy)]
struct Ctx {
    regs: [u64; 32], // x0..x31；x10=a0、x17=a7
    sepc: u64,
    sstatus: u64,
}

/// 【STUDENT】陷入：把调用者寄存器现场整存进 TrapContext 快照。
fn ctx_save(regs: &[u64; 32], sepc: u64, sstatus: u64) -> Ctx {
    // TODO: 把 32 个 GPR + sepc + sstatus 整组拷进快照返回。
    // HINT: Ctx { regs: *regs, sepc, sstatus }
    Ctx { regs: [0; 32], sepc: 0, sstatus: 0 } // ← 占位
}

/// 【STUDENT】返回前推进：把 handler 结果写回 a0(x10)，并跳过 ecall 指令（sepc+=4）。
fn ctx_advance(ctx: &mut Ctx, retval: u64) {
    // TODO: ctx.regs[10] = retval; ctx.sepc += 4;  其余 31 个寄存器一律不动。
}

// ════════════════════════════════════════════════════════════════════
// S3 · Syscall ABI 分发表（GNU 规范化）
// ════════════════════════════════════════════════════════════════════

const NR_WRITE: i64 = 64;
const NR_EXIT: i64 = 93;
const NR_GETPID: i64 = 172;
const ENOSYS: i64 = 38;
const FAKE_PID: i64 = 42;

/// 寄存器组：a[7]=调用号，a[0..=5]=参数，a[0]=返回值。
struct Regs {
    a: [i64; 8],
}

/// 受控的「内核」侧状态：用户内存 + 写回显捕获缓冲 + 退出记录。
struct Machine {
    mem: Vec<u8>,
    capture: Vec<u8>,
    exited: bool,
    exit_code: i64,
}

/// 【STUDENT】sys_write：把 mem[ptr..ptr+len] 回显进 capture 并打印，返回写出的字节数。
fn sys_write(m: &mut Machine, ptr: i64, len: i64) -> i64 {
    // TODO: for i in 0..len { m.capture.push(m.mem[(ptr+i) as usize]) }；打印；返回 len。
    0 // ← 占位
}

/// 【STUDENT】sys_getpid：返回固定 pid。
fn sys_getpid(_m: &mut Machine) -> i64 {
    // TODO: 返回 FAKE_PID。
    0 // ← 占位
}

/// 【STUDENT】sys_exit：记录退出码，返回 0。
fn sys_exit(m: &mut Machine, code: i64) -> i64 {
    // TODO: m.exited = true; m.exit_code = code; 返回 0。
    0 // ← 占位（未记录退出码）
}

/// 【STUDENT】分发：按 a7 选 handler，结果写回 a0；未知号 → -ENOSYS。
fn syscall(m: &mut Machine, regs: &mut Regs) {
    // TODO: let r = match regs.a[7] { 64 => sys_write(m, regs.a[1], regs.a[2]),
    //                                 172 => sys_getpid(m),
    //                                 93 => sys_exit(m, regs.a[0]),
    //                                 _ => -ENOSYS };
    //       regs.a[0] = r;
    regs.a[0] = 0; // ← 占位：未分发
}

// ════════════════════════════════════════════════════════════════════
// S4 · 真实 syscall 往返（user → kernel → user 的安检门）
//   host x86-64：`syscall` 指令，rax=号(write=1)、rdi/rsi/rdx=参，rax=返回值。
// ════════════════════════════════════════════════════════════════════

/// 【STUDENT】裸 syscall 包装：装好寄存器、执行 `syscall`、取回返回值。
unsafe fn raw_syscall3(n: u64, a1: u64, a2: u64, a3: u64) -> i64 {
    // TODO: 用内联汇编真的陷入内核：rax=n, rdi=a1, rsi=a2, rdx=a3, 执行 `syscall`, 取回 rax。
    // HINT:
    //   let ret: i64;
    //   asm!("syscall",
    //        inlateout("rax") n => ret,
    //        in("rdi") a1, in("rsi") a2, in("rdx") a3,
    //        lateout("rcx") _, lateout("r11") _, options(nostack));
    //   ret
    -1 // ← 占位：未真正陷入，S4 判 FAIL（也不会打印 HELLO_SYSCALL）
}

// ───────────────────────── 测试 harness（勿改）─────────────────────────

fn one(mode: u32, base: u64, cause: u32, trap_req: bool, exp_pc: u64, exp_acc: bool) -> bool {
    let (pc, acc) = dispatch(mode, base, cause, trap_req);
    if pc != exp_pc || acc != exp_acc {
        println!(
            "S1_FAIL mode={} cause={} exp=(0x{:x},{}) got=(0x{:x},{})",
            mode, cause, exp_pc, exp_acc, pc, acc
        );
        return false;
    }
    true
}

fn check_s1() -> bool {
    let base = 0x8000_0000u64;
    let mut ok = true;

    let mut g = true;
    g &= one(0, base, 0, true, base, true);
    g &= one(0, base, 3, true, base, true);
    g &= one(0, base, 8, true, base, true);
    if g { println!("DIRECT_PASS"); } else { ok = false; }

    let mut g = true;
    g &= one(1, base, 0, true, base, true);
    g &= one(1, base, 1, true, base + 4, true);
    g &= one(1, base, 8, true, base + 0x20, true);
    g &= one(1, base, 15, true, base + 0x3C, true);
    if g { println!("VECTORED_PASS"); } else { ok = false; }

    let mut g = true;
    g &= one(1, base, 2, true, base + 0x08, true);
    g &= one(1, base, 5, false, base + 0x14, false);
    g &= one(0, base, 9, true, base, true);
    if g { println!("DISPATCH_PASS"); } else { ok = false; }

    if ok { println!("S1_PASS"); }
    ok
}

fn check_s2() -> bool {
    let mut ok = true;
    let mut regs = [0u64; 32];
    let mut i = 0usize;
    while i < 32 {
        regs[i] = 0xA000 + i as u64;
        i += 1;
    }
    let sepc = 0x1000u64;
    let sstatus = 0x0000_0100u64;

    let ctx = ctx_save(&regs, sepc, sstatus);
    let mut g = ctx.regs == regs && ctx.sepc == sepc && ctx.sstatus == sstatus;
    if g { println!("SAVE_PASS"); } else { println!("SAVE_FAIL 快照与输入不一致"); ok = false; }

    let mut c2 = ctx;
    ctx_advance(&mut c2, 7);

    if c2.regs[10] == 7 { println!("RETVAL_PASS"); } else { println!("RETVAL_FAIL a0={}", c2.regs[10]); ok = false; }

    g = true;
    let mut k = 0usize;
    while k < 32 {
        if k != 10 && c2.regs[k] != regs[k] {
            println!("RESTORE_FAIL x{} 被改动 {}->{}", k, regs[k], c2.regs[k]);
            g = false;
        }
        k += 1;
    }
    if c2.sepc != sepc + 4 { println!("RESTORE_FAIL sepc=0x{:x} 应=0x{:x}", c2.sepc, sepc + 4); g = false; }
    if c2.sstatus != sstatus { println!("RESTORE_FAIL sstatus 被改动"); g = false; }
    if g { println!("RESTORE_PASS"); } else { ok = false; }

    if ok { println!("S2_PASS"); }
    ok
}

fn check_s3() -> bool {
    let mut ok = true;
    let mut m = Machine { mem: vec![b'h', b'i'], capture: Vec::new(), exited: false, exit_code: 0 };

    let mut r = Regs { a: [0; 8] };
    r.a[7] = NR_WRITE; r.a[1] = 0; r.a[2] = 2;
    syscall(&mut m, &mut r);
    if r.a[0] == 2 && m.capture == b"hi" {
        println!();
        println!("NR_WRITE_PASS");
    } else {
        println!();
        println!("NR_WRITE_FAIL a0={} capture={:?}", r.a[0], m.capture);
        ok = false;
    }

    let mut r = Regs { a: [0; 8] };
    r.a[7] = NR_GETPID;
    syscall(&mut m, &mut r);
    if r.a[0] == FAKE_PID { println!("NR_GETPID_PASS"); } else { println!("NR_GETPID_FAIL a0={}", r.a[0]); ok = false; }

    let mut r = Regs { a: [0; 8] };
    r.a[7] = NR_EXIT; r.a[0] = 5;
    syscall(&mut m, &mut r);
    if m.exited && m.exit_code == 5 && r.a[0] == 0 {
        println!("NR_EXIT_PASS");
    } else {
        println!("NR_EXIT_FAIL exited={} code={} a0={}", m.exited, m.exit_code, r.a[0]);
        ok = false;
    }

    let mut r = Regs { a: [0; 8] };
    r.a[7] = 999;
    syscall(&mut m, &mut r);
    if r.a[0] == -ENOSYS { println!("ENOSYS_PASS"); } else { println!("ENOSYS_FAIL a0={}", r.a[0]); ok = false; }

    if ok { println!("S3_PASS"); }
    ok
}

fn check_s4() -> bool {
    let mut ok = true;
    let msg = b"HELLO_SYSCALL\n";

    let r = unsafe { raw_syscall3(1, 1, msg.as_ptr() as u64, msg.len() as u64) };
    if r >= 0 { println!("ECALL_PASS"); } else { println!("ECALL_FAIL ret={}", r); ok = false; }

    if r == msg.len() as i64 { println!("SYSRET_PASS"); } else { println!("SYSRET_FAIL ret={}", r); ok = false; }

    if ok { println!("S4_PASS"); }
    ok
}

fn main() {
    let mut all = true;
    all &= check_s1();
    all &= check_s2();
    all &= check_s3();
    all &= check_s4();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
