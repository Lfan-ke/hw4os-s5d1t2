// S01b · 裸机 Rust 运行时（参考答案）
// 故事：裸机上 `core` 一直可用（切片/迭代器/Option，无需分配器）；
// 但 `alloc`（Vec/Box/String）要活，必须挂一个 #[global_allocator]；
// 任何 no_std 程序还必须有一个 #[panic_handler] 才能编译。
#![no_std]
#![no_main]
extern crate alloc;

use alloc::format;
use alloc::string::String;
use alloc::vec::Vec;
use core::alloc::{GlobalAlloc, Layout};
use core::panic::PanicInfo;
use core::ptr::addr_of_mut;

core::arch::global_asm!(
    r#"
.section .text.entry
.globl _start
_start:
    la sp, _stack_top
    call kmain
    call rt_shutdown
.section .bss
.align 12
    .space 4096*16
.globl _stack_top
_stack_top:
"#
);

fn sbi(eid: i64, fid: i64, a0: i64, a1: i64) -> i64 {
    let r: i64;
    unsafe {
        core::arch::asm!("ecall", in("a7") eid, in("a6") fid,
            inlateout("a0") a0 => r, in("a1") a1, options(nostack));
    }
    r
}
fn putc(c: u8) { sbi(1, 0, c as i64, 0); }
fn puts(s: &str) { for b in s.bytes() { putc(b); } }
#[no_mangle]
extern "C" fn rt_shutdown() -> ! {
    sbi(0x5353_5354, 0, 0, 0); // SRST shutdown（a6=fid=0）
    sbi(8, 0, 0, 0);           // 回退 legacy
    loop {}
}

// —— 运行时挂钩 1：全局分配器 ——（学生在习题里实现 alloc）
const HEAP_LEN: usize = 1 << 16;
static mut HEAP: [u8; HEAP_LEN] = [0; HEAP_LEN];
static mut OFF: usize = 0;
struct Bump;
unsafe impl GlobalAlloc for Bump {
    unsafe fn alloc(&self, _l: Layout) -> *mut u8 {
        // TODO: 实现 bump 分配器——
        //   1) 把全局偏移 OFF 向上对齐到 _l.align()
        //   2) 若 OFF + _l.size() 超过 HEAP_LEN，返回 core::ptr::null_mut()
        //   3) 否则返回 &HEAP[OFF]，并把 OFF 前进 _l.size()
        // HINT: 用 addr_of_mut!(OFF) / addr_of_mut!(HEAP) 取静态可变引用，避免 static_mut_refs 告警。
        core::ptr::null_mut() // 占位：分配器没活 → kmain 探针报 ALLOC_MISS
    }
    unsafe fn dealloc(&self, _: *mut u8, _: Layout) {}
}
#[global_allocator]
static GA: Bump = Bump;

// —— 运行时挂钩 2：panic 处理器（no_std 必备）——
#[panic_handler]
fn on_panic(_: &PanicInfo) -> ! {
    puts("PANIC\n");
    rt_shutdown();
}

#[no_mangle]
extern "C" fn kmain() {
    puts("[S01b-rust] core always-on; alloc needs #[global_allocator]\n");

    // core：无需分配器（切片排序 / 迭代器 / Option）
    let mut arr = [5i32, 3, 1, 4, 2];
    arr.sort_unstable();
    let core_ok = arr == [1, 2, 3, 4, 5]
        && (1..=5).sum::<i32>() == 15
        && Some(7).map(|x| x + 1) == Some(8);
    if core_ok { puts("CORE_PASS\n"); } else { puts("CORE_MISS\n"); rt_shutdown(); }

    // 先探一下分配器是否真的活了（学生没实现则返回 null）
    let probe = unsafe { GA.alloc(Layout::from_size_align(16, 8).unwrap()) };
    if probe.is_null() {
        puts("ALLOC_MISS (global_allocator alloc 未实现?)\n");
        rt_shutdown();
    }

    // alloc：Vec / String / format!
    let mut v: Vec<i32> = Vec::new();
    for i in 0..8 { v.push(i * i); }
    let s: String = format!("vec={:?} sum={}", v, v.iter().sum::<i32>());
    puts(&s);
    putc(b'\n');
    if v.len() == 8 && v[7] == 49 { puts("ALLOC_PASS\n"); } else { puts("ALLOC_MISS\n"); rt_shutdown(); }

    puts("ALL_PASS\n");
}
