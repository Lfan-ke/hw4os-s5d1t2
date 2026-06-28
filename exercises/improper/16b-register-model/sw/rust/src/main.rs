//! 16b · 寄存器模型 - Rust（学生填空版，类型化四级）。
//! 同一张 regdev 寄存器表，从手摇 volatile 一路升级到类型化寄存器图：
//!   ① volatile 裸字  ② bitflags! 纯标志  ③ tock-registers 类型化寄存器图  ④ bytemuck raw↔struct 镜像。
//! 你只需把 register_bitfields! 里各字段的 OFFSET/NUMBITS 按 §2.1 填对；其余 harness 勿改。
#![allow(dead_code)]

use std::ptr::{read_volatile, write_volatile};

use bitflags::bitflags;
use bytemuck::{Pod, Zeroable};
use tock_registers::interfaces::{Readable, Writeable};
use tock_registers::registers::{ReadOnly, ReadWrite, WriteOnly};
use tock_registers::{register_bitfields, register_structs};

const REG_MAGIC: u32 = 0x5245_4744; // "REGD"
const TRACE_CTRL: u32 = 0x0000_000B; // EN=1 IE=1 MODE=2(solid) RST=0
const TRACE_STATUS: u32 = 0x0000_0005; // READY=1 BUSY=0 IRQ=1
const TRACE_BYTE: u32 = 0x0000_00A5; // DATA.BYTE[7:0]

// ① volatile 裸字：read_volatile/write_volatile 对齐 C 的 readl/writel。
fn level_volatile() -> bool {
    let mut buf = [0u32; 4];
    buf[3] = REG_MAGIC;
    let base = buf.as_mut_ptr();
    unsafe {
        if read_volatile(base.add(3)) != REG_MAGIC {
            return false;
        }
        write_volatile(base, TRACE_CTRL);
        let c = read_volatile(base);
        let st = (c & 1) | if c & 1 != 0 && (c >> 1) & 1 != 0 { 4 } else { 0 };
        write_volatile(base.add(1), st);
        if read_volatile(base.add(1)) != TRACE_STATUS {
            return false;
        }
        write_volatile(base.add(2), TRACE_BYTE);
        if read_volatile(base.add(2)) & 0xFF != TRACE_BYTE {
            return false;
        }
    }
    println!("RAW_PASS  volatile read/write: ID={REG_MAGIC:08X} CTRL={TRACE_CTRL:08X}");
    true
}

// ② bitflags!：纯标志寄存器（STATUS 与 CTRL 的标志位）；多位 MODE 不进 bitflags。
bitflags! {
    #[derive(Debug, Clone, Copy, PartialEq)]
    struct Status: u32 { const READY = 1 << 0; const BUSY = 1 << 1; const IRQ = 1 << 2; }
    #[derive(Debug, Clone, Copy, PartialEq)]
    struct CtrlFlags: u32 { const EN = 1 << 0; const IE = 1 << 1; const RST = 1 << 8; }
}

fn level_bitflags() -> bool {
    let st = Status::from_bits_truncate(TRACE_STATUS);
    let cf = CtrlFlags::EN | CtrlFlags::IE;
    if !st.contains(Status::READY) || st.contains(Status::BUSY) || !st.contains(Status::IRQ) {
        return false;
    }
    if cf.bits() != 0x3 {
        return false;
    }
    println!("FLAGS_PASS bitflags: STATUS={st:?} CTRL={cf:?}");
    true
}

// ③ tock-registers：把 §2.1 的布局写成类型化寄存器图（bitfields + structs）。
// ── 学生填：按 §2.1 把每个字段的 OFFSET(n)/NUMBITS(w) 填对（占位全是 OFFSET(0)，会让 TOCK 校验失败）──
register_bitfields![u32,
    CTRL [
        EN   OFFSET(0) NUMBITS(1) [],                                   // 样例：EN 在 bit0
        IE   OFFSET(0) NUMBITS(1) [],                                   // TODO: 填 OFFSET(1)
        MODE OFFSET(0) NUMBITS(2) [ Off = 0, Blink = 1, Solid = 2, Burst = 3 ], // TODO: 填 OFFSET(2)
        RST  OFFSET(0) NUMBITS(1) [],                                   // TODO: 填 OFFSET(8)
    ],
    STATUS [
        READY OFFSET(0) NUMBITS(1) [],                                  // 样例：READY 在 bit0
        BUSY  OFFSET(0) NUMBITS(1) [],                                  // TODO: 填 OFFSET(1)
        IRQ   OFFSET(0) NUMBITS(1) [],                                  // TODO: 填 OFFSET(2)
    ],
    DATA [ BYTE OFFSET(0) NUMBITS(8) [] ],
    ID [ MAGIC OFFSET(0) NUMBITS(32) [] ],
];

register_structs! {
    pub RegDev {
        (0x00 => ctrl: ReadWrite<u32, CTRL::Register>),
        (0x04 => status: ReadOnly<u32, STATUS::Register>),
        (0x08 => data: WriteOnly<u32, DATA::Register>),
        (0x0c => id: ReadOnly<u32, ID::Register>),
        (0x10 => @END),
    }
}

fn level_tock() -> bool {
    let buf = [TRACE_CTRL, TRACE_STATUS, 0u32, REG_MAGIC];
    let ok = {
        let regs: &RegDev = unsafe { &*(buf.as_ptr() as *const RegDev) };
        regs.data.write(DATA::BYTE.val(TRACE_BYTE));
        regs.id.get() == REG_MAGIC
            && regs.ctrl.is_set(CTRL::EN)
            && regs.ctrl.is_set(CTRL::IE)
            && regs.ctrl.read(CTRL::MODE) == 2
            && regs.status.is_set(STATUS::READY)
            && !regs.status.is_set(STATUS::BUSY)
            && regs.status.is_set(STATUS::IRQ)
    };
    if !ok || buf[2] & 0xFF != TRACE_BYTE {
        return false;
    }
    println!("TOCK_PASS register_structs!/register_bitfields!: MODE=Solid IRQ=1");
    true
}

// ④ bytemuck：整个寄存器块的 raw 字节 ↔ 类型化 struct 双向无损（逐位镜像）。
#[repr(C)]
#[derive(Clone, Copy, Pod, Zeroable)]
struct RegFile {
    ctrl: u32,
    status: u32,
    data: u32,
    id: u32,
}

fn level_bytemuck() -> bool {
    let mut raw = [0u8; 16];
    raw[0..4].copy_from_slice(&TRACE_CTRL.to_le_bytes());
    raw[4..8].copy_from_slice(&TRACE_STATUS.to_le_bytes());
    raw[8..12].copy_from_slice(&TRACE_BYTE.to_le_bytes());
    raw[12..16].copy_from_slice(&REG_MAGIC.to_le_bytes());

    let rf: RegFile = bytemuck::cast(raw);
    if rf.ctrl != TRACE_CTRL || rf.status != TRACE_STATUS || rf.id != REG_MAGIC {
        return false;
    }
    let back: [u8; 16] = bytemuck::cast(rf);
    if back != raw {
        return false;
    }
    println!("MIRROR_PASS raw↔struct 逐位一致 (bytemuck): CTRL={:08X} ID={:08X}", rf.ctrl, rf.id);
    true
}

fn main() {
    let mut ok = true;
    ok &= level_volatile();
    ok &= level_bitflags();
    ok &= level_tock();
    ok &= level_bytemuck();
    if ok {
        println!("ALL_PASS");
    } else {
        println!("SOME_FAIL");
        std::process::exit(1);
    }
}
