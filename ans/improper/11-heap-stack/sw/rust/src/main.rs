//! 11 堆与栈：SP 是一根指针，allocator 是一个记账员 —— Rust 参考解。
//! 四段逐题递进（同一可执行里逐段点亮 *_PASS）：
//!   11.1 单设备单栈：SP 是一根向下生长的指针            → STACK_PASS
//!   11.2 两块设备：栈在 A、堆在 B，各管各的地盘，互不侵犯 → HEAP_INDEP_PASS
//!   11.3 一块设备：堆↑ 栈↓ 对向生长，手动防侵犯（比较器） → COEXIST_PASS
//!   11.4 把 bump allocator 注册成 #[global_allocator]    → GLOBAL_PASS
//! 学生填各结构体方法 + 文件末尾「注册」那一行；下方 harness（勿改）打印 *_PASS。

use std::alloc::{GlobalAlloc, Layout};
use std::cell::UnsafeCell;
use std::sync::atomic::{AtomicUsize, Ordering};

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ 11.4 用的 bump allocator（结构 + alloc/dealloc 给好；学生只补「注册」一行）║
// ╚══════════════════════════════════════════════════════════════════════════╝

const ARENA_BYTES: usize = 4 * 1024 * 1024;

struct Arena(UnsafeCell<[u8; ARENA_BYTES]>);
unsafe impl Sync for Arena {}
static ARENA_MEM: Arena = Arena(UnsafeCell::new([0u8; ARENA_BYTES]));

/// 一个「记账员」：在静态 arena 上只进不退地划地盘（cursor 就是账本）。
struct Bump {
    cursor: AtomicUsize,
}
impl Bump {
    const fn new() -> Self {
        Bump { cursor: AtomicUsize::new(0) }
    }
    /// 已经记到账上的字节数（= 游标位置）。
    fn used(&self) -> usize {
        self.cursor.load(Ordering::SeqCst)
    }
}
// Bump 仅含 AtomicUsize（自带 Sync），无需再手动 impl Sync。
unsafe impl GlobalAlloc for Bump {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let base = ARENA_MEM.0.get() as *mut u8 as usize;
        let align = layout.align();
        let size = layout.size();
        let mut off = self.cursor.load(Ordering::Relaxed);
        loop {
            let aligned = (base + off + align - 1) & !(align - 1);
            let new_off = aligned - base + size;
            if new_off > ARENA_BYTES {
                return std::ptr::null_mut();
            }
            match self
                .cursor
                .compare_exchange_weak(off, new_off, Ordering::SeqCst, Ordering::Relaxed)
            {
                Ok(_) => return aligned as *mut u8,
                Err(x) => off = x,
            }
        }
    }
    unsafe fn dealloc(&self, _ptr: *mut u8, _layout: Layout) {
        // bump 只进不退：free 是 no-op（合并/回收留作引申）。
    }
}

// ★★★ 学生填（11.4 的核心一行）★★★
// 取消下一行的注释，把 Bump「注册」成全局分配器——这正是 rust 编译器替你做、
// 而 C 里要你手动接 g_alloc 的那件事。注册后，下方 exp_global 里的 Box/Vec 直接落进 arena。
#[global_allocator]
static GLOBAL: Bump = Bump::new();

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ 11.1 单设备单栈：SP 是一根向下生长的指针                                    ║
// ╚══════════════════════════════════════════════════════════════════════════╝

struct Stack {
    mem: Vec<u32>,
    base: usize, // 栈底（最低地址下标）
    top: usize,  // 栈顶界（最高地址下标 + 1）
    sp: usize,   // 满递减栈：指向「最近压入」的槽；空栈时 sp == top
}
impl Stack {
    fn new(cap: usize) -> Self {
        Stack { mem: vec![0; cap], base: 0, top: cap, sp: cap }
    }

    /// 学生填：把 SP 初始化到内存区「顶端」。栈向下生长，故初值 = top。
    fn sp_init(&mut self) {
        self.sp = self.top;
    }

    /// 学生填：满递减栈 push = 先 sp-=1 再写。SP 已到 base 还要压 → 越界（Err）。
    fn push(&mut self, w: u32) -> Result<(), &'static str> {
        if self.sp <= self.base {
            return Err("STACK_OVERFLOW");
        }
        self.sp -= 1;
        self.mem[self.sp] = w;
        Ok(())
    }

    /// 学生填：pop = 先读 mem[sp] 再 sp+=1。空栈返回 None。
    fn pop(&mut self) -> Option<u32> {
        if self.sp >= self.top {
            return None;
        }
        let w = self.mem[self.sp];
        self.sp += 1;
        Some(w)
    }
}

fn exp_stack() -> bool {
    let mut s = Stack::new(4);
    s.sp_init();
    if s.sp != s.top {
        println!("STACK_FAIL sp_init 未把 SP 置到区顶");
        return false;
    }
    let data = [0x11u32, 0x22, 0x33, 0x44];
    for &w in &data {
        if s.push(w).is_err() {
            println!("STACK_FAIL 容量内 push 不应失败");
            return false;
        }
    }
    // 满栈后再压：必须报越界，绝不静默覆盖。
    if s.push(0x55).is_ok() {
        println!("STACK_FAIL 越界 push 未被检出");
        return false;
    }
    // LIFO 还原：弹出顺序应为 0x44,0x33,0x22,0x11。
    for &w in data.iter().rev() {
        match s.pop() {
            Some(x) if x == w => {}
            other => {
                println!("STACK_FAIL LIFO 还原错: 期望 {:#x} 得 {:?}", w, other);
                return false;
            }
        }
    }
    if s.pop().is_some() {
        println!("STACK_FAIL 空栈 pop 应返回 None");
        return false;
    }
    println!("STACK_PASS");
    true
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ 11.2 两块设备：栈在 A、堆在 B，各管各的地盘                                 ║
// ╚══════════════════════════════════════════════════════════════════════════╝

/// 设备上的 bump 堆：从底向上生长。设备 = 一段平坦字数组 + 一个游标。
struct HeapDev {
    mem: Vec<u32>,
    cap: usize,
    top: usize, // 下一个空闲槽
}
impl HeapDev {
    fn new(cap: usize) -> Self {
        HeapDev { mem: vec![0; cap], cap, top: 0 }
    }
    fn heap_init(&mut self) {
        self.top = 0;
    }
    /// 学生填：bump 向上分配 n 个字，越上限 → OOM（Err）；返回起始下标。
    fn alloc(&mut self, n: usize) -> Result<usize, &'static str> {
        if self.top + n > self.cap {
            return Err("OOM");
        }
        let start = self.top;
        self.top += n;
        Ok(start)
    }
}

fn exp_indep() -> bool {
    // 选「小而快的设备 A 作栈、大的设备 B 作堆」。
    // 学生择一（言之成理即可）：
    //   TODO[a] 选 A（小）作栈、B（大）作堆 —— 栈帧小、堆需要大块连续空间。
    //   ELSE[b] 选 B 作栈也行，只要两设备各管各的。
    // 这里取 TODO[a]。
    let mut a = Stack::new(4); // 设备 A：小，作栈
    let mut b = HeapDev::new(16); // 设备 B：大，作堆
    a.sp_init();
    b.heap_init();

    // 在堆 B 里放两个哨兵，记下它们的位置。
    let off = match b.alloc(2) {
        Ok(o) => o,
        Err(_) => {
            println!("HEAP_FAIL 堆 B 首次 alloc 不应 OOM");
            return false;
        }
    };
    b.mem[off] = 0xDEAD_BEEF;
    b.mem[off + 1] = 0x0BAD_F00D;

    // 把栈 A 压到物理上限：A 必须自己 overflow（且不波及 B）。
    let mut pushed = 0u32;
    loop {
        match a.push(0xA5A5_0000 | pushed) {
            Ok(()) => pushed += 1,
            Err(_) => break,
        }
        if pushed > 100 {
            println!("HEAP_FAIL 栈 A 永不溢出？");
            return false;
        }
    }
    if pushed != 4 {
        println!("HEAP_FAIL 栈 A 容量应为 4，实测 {}", pushed);
        return false;
    }

    // 把堆 B 撑到物理上限：B 必须 OOM（且不波及 A）。
    let mut got = 2usize;
    loop {
        match b.alloc(2) {
            Ok(_) => got += 2,
            Err(_) => break,
        }
        if got > 100 {
            println!("HEAP_FAIL 堆 B 永不 OOM？");
            return false;
        }
    }
    if got != 16 {
        println!("HEAP_FAIL 堆 B 容量应为 16，实测 {}", got);
        return false;
    }

    // 关键：两设备 = 两地址空间，互不侵犯。哨兵仍在、栈数据仍在。
    if b.mem[off] != 0xDEAD_BEEF || b.mem[off + 1] != 0x0BAD_F00D {
        println!("HEAP_FAIL 栈 A 溢出竟改写了堆 B 的数据");
        return false;
    }
    match a.pop() {
        Some(x) if x == (0xA5A5_0000 | 3) => {}
        other => {
            println!("HEAP_FAIL 栈 A 数据被破坏: {:?}", other);
            return false;
        }
    }
    println!("HEAP_INDEP_PASS");
    true
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ 11.3 一块设备：堆↑ 栈↓ 对向生长，手动防侵犯                                 ║
// ╚══════════════════════════════════════════════════════════════════════════╝

struct OneDev {
    mem: Vec<u32>,
    cap: usize,
    heap_top: usize, // 堆区 [0, heap_top) 已用，向上生长
    sp: usize,       // 栈区 [sp, cap) 已用，向下生长；空时 sp == cap
}
impl OneDev {
    fn new(cap: usize) -> Self {
        OneDev { mem: vec![0; cap], cap, heap_top: 0, sp: cap }
    }
    fn heap_init(&mut self) {
        self.heap_top = 0;
    }
    fn sp_init(&mut self) {
        self.sp = self.cap;
    }
    /// 学生填：堆 alloc 必须自查 heap_top + n <= sp，否则 OOM（没有独立设备兜底，碰撞全靠手算）。
    fn alloc(&mut self, n: usize) -> Result<usize, &'static str> {
        if self.heap_top + n > self.sp {
            return Err("OOM");
        }
        let start = self.heap_top;
        self.heap_top += n;
        Ok(start)
    }
    /// 学生填：栈 push 必须自查 sp - 1 >= heap_top，否则 STACK_OVERFLOW。
    fn push(&mut self, w: u32) -> Result<(), &'static str> {
        if self.sp <= self.heap_top {
            return Err("STACK_OVERFLOW");
        }
        self.sp -= 1;
        self.mem[self.sp] = w;
        Ok(())
    }
    fn pop(&mut self) -> Option<u32> {
        if self.sp >= self.cap {
            return None;
        }
        let w = self.mem[self.sp];
        self.sp += 1;
        Some(w)
    }
}

fn exp_coexist() -> bool {
    let mut m = OneDev::new(8);
    m.heap_init();
    m.sp_init();
    let mut heap_marks: Vec<(usize, u32)> = Vec::new();
    let mut stack_vals: Vec<u32> = Vec::new();
    let mut hv = 0x1000u32;
    let mut sv = 0x9000u32;
    let mut guard = 0;
    loop {
        let did_heap = match m.alloc(1) {
            Ok(at) => {
                m.mem[at] = hv;
                heap_marks.push((at, hv));
                hv += 1;
                true
            }
            Err(_) => false,
        };
        if m.heap_top > m.sp {
            println!("COLLIDE_UNDETECTED 堆栈区重叠 heap_top={} sp={}", m.heap_top, m.sp);
            return false;
        }
        let did_stack = match m.push(sv) {
            Ok(()) => {
                stack_vals.push(sv);
                sv += 1;
                true
            }
            Err(_) => false,
        };
        if m.heap_top > m.sp {
            println!("COLLIDE_UNDETECTED 堆栈区重叠 heap_top={} sp={}", m.heap_top, m.sp);
            return false;
        }
        if !did_heap && !did_stack {
            break;
        }
        guard += 1;
        if guard > 10_000 {
            println!("COEXIST_FAIL 循环未收敛（alloc/push 未正确自查边界）");
            return false;
        }
    }
    // 堆栈必须恰好相遇于边界：heap_top == sp（无缝且无重叠）。
    if m.heap_top != m.sp {
        println!("COEXIST_FAIL 未能恰好相遇于边界 heap_top={} sp={}", m.heap_top, m.sp);
        return false;
    }
    // 数据完整性：堆里的标记没被栈静默覆盖。
    for (at, w) in &heap_marks {
        if m.mem[*at] != *w {
            println!("COEXIST_FAIL 堆数据 @{} 被静默覆盖", at);
            return false;
        }
    }
    // 栈值 LIFO 还原（也即没被堆静默覆盖）。
    for w in stack_vals.iter().rev() {
        match m.pop() {
            Some(x) if x == *w => {}
            other => {
                println!("COEXIST_FAIL 栈 LIFO 还原错: 期望 {:#x} 得 {:?}", w, other);
                return false;
            }
        }
    }
    println!("COEXIST_PASS");
    true
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ 11.4 把 allocator 注册到 global（注册见文件上方 #[global_allocator] 一行）  ║
// ╚══════════════════════════════════════════════════════════════════════════╝

fn exp_global() -> bool {
    let lo = ARENA_MEM.0.get() as *mut u8 as usize;
    let hi = lo + ARENA_BYTES;

    let before = GLOBAL.used();
    // 注册成功后，这些高层容器分配的字节必然落进我们的 arena。
    let b = Box::new(0xABCD_u64);
    let mut v: Vec<u64> = Vec::new();
    for i in 0..100u64 {
        v.push(i);
    }
    let after = GLOBAL.used();

    let grew = after > before;
    let p = (&*b as *const u64) as usize;
    let inside = p >= lo && p < hi;

    std::hint::black_box(&v);
    std::hint::black_box(&b);

    if grew && inside {
        println!("GLOBAL_PASS");
        true
    } else {
        println!(
            "GLOBAL_FAIL grew={} inside={}（allocator 没注册到 global？Box/Vec 仍走系统分配器）",
            grew, inside
        );
        false
    }
}

// ───────────────────────────── 主流程（勿改）─────────────────────────────

fn main() {
    let mut all = true;
    all &= exp_stack();
    all &= exp_indep();
    all &= exp_coexist();
    all &= exp_global();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
