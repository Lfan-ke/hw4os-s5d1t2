//! 16c · 核内中断 - Rust（参考解，花活）。
//! 软件 CLINT 模型：tock-registers 摆出寄存器图（msip/mtimecmp/mtime），
//! bitflags 给 mip 的 MTIP/MSIP 标志起名；mtime>=mtimecmp 拉起 timer 中断、msip 写 1 拉起软件中断。
//! 固定场景：PERIOD=5、NTICK=16 → timer 触发 3 次（mtime=5/10/15）；软件中断拉起 2 次。
#![allow(dead_code)]

use bitflags::bitflags;
use tock_registers::interfaces::{Readable, Writeable};
use tock_registers::register_structs;
use tock_registers::registers::ReadWrite;

const PERIOD: u64 = 5;
const NTICK: u64 = 16;
const EXP_TIMER: i32 = 3;
const EXP_SOFT: i32 = 2;

// CLINT 寄存器图（toy 紧凑偏移；真 QEMU virt 为 msip@0x0 / mtimecmp@0x4000 / mtime@0xBFF8）。
register_structs! {
    pub Clint {
        (0x00 => msip: ReadWrite<u32>),
        (0x04 => _reserved),
        (0x08 => mtimecmp: ReadWrite<u64>),
        (0x10 => mtime: ReadWrite<u64>),
        (0x18 => @END),
    }
}

// mip CSR 的中断挂起位：MSIP=bit3（机器态软件中断）、MTIP=bit7（机器态 timer 中断）。
bitflags! {
    #[derive(Debug, Clone, Copy, PartialEq)]
    struct Mip: u32 {
        const MSIP = 1 << 3;
        const MTIP = 1 << 7;
    }
}

fn poll_mip(c: &Clint) -> Mip {
    let mut mip = Mip::empty();
    if c.mtime.get() >= c.mtimecmp.get() {
        mip |= Mip::MTIP;
    }
    if c.msip.get() & 1 != 0 {
        mip |= Mip::MSIP;
    }
    mip
}

fn phase_timer(c: &Clint) -> bool {
    let mut cmp: u64 = PERIOD;
    c.mtimecmp.set(cmp);
    let mut fires = 0;
    for _ in 0..NTICK {
        if poll_mip(c).contains(Mip::MTIP) {
            fires += 1;
            cmp += PERIOD;
            c.mtimecmp.set(cmp);
        }
        c.mtime.set(c.mtime.get() + 1);
    }
    if fires != EXP_TIMER || c.mtime.get() != NTICK {
        return false;
    }
    println!("TIMER_PASS fires={fires} mtime={}", c.mtime.get());
    true
}

fn phase_soft(c: &Clint) -> bool {
    let mut handled = 0;
    for _ in 0..EXP_SOFT {
        c.msip.set(1);
        if poll_mip(c).contains(Mip::MSIP) {
            handled += 1;
            c.msip.set(0);
        }
    }
    if handled != EXP_SOFT || poll_mip(c).contains(Mip::MSIP) {
        return false;
    }
    println!("SOFT_PASS  ipi={handled}");
    true
}

fn main() {
    let buf = [0u64; 3];
    let c: &Clint = unsafe { &*(buf.as_ptr() as *const Clint) };
    let ok = phase_timer(c) & phase_soft(c);
    if ok {
        println!("ALL_PASS");
    } else {
        println!("SOME_FAIL");
        std::process::exit(1);
    }
}
