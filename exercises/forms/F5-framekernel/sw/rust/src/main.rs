//! 形态认知 · F5 框内核(framekernel, Asterinas) —— Rust。
//!
//! 一句话母题：**宏内核的单地址空间性能 + 微内核式的隔离**，怎么同时拿到？
//! 答案是把可信计算基(TCB)收敛成一个最小「框架(OS Framework / OSTD)」，
//! 框架内部用 `unsafe`/裸指针把底层能力包成**安全 API**；其余「内核子系统」
//! 全用安全 Rust，经该 API 访问资源——**隔离来自类型系统，不是 MMU**。
//!
//! 真实原型：Asterinas（蚂蚁 OS Lab，SOSP'25 Best Paper）。
//!   - `ostd/`  = OS Framework，允许 unsafe，把裸操作包成安全 API。
//!   - `kernel/src/lib.rs:8` = `#![deny(unsafe_code)]` —— 整个服务层禁 unsafe。
//!
//! 你只需填 2 处（标 TODO）：
//!   A) 安全 API `Frame::write` 的**边界检查**封装。
//!   B) `check_mintcb` 里的 **unsafe 计数**逻辑。
//! 下方测试 harness 勿改。

#![allow(dead_code)]

// ════════════════════════════════════════════════════════════════
// OS Framework（≈ asterinas OSTD）：**唯一**允许 unsafe 的最小可信框架。
// ════════════════════════════════════════════════════════════════
mod framework {
    // LABCTL-TCB-BEGIN  —— 本区段内出现的 unsafe 计入「框架 TCB」
    /// 安全 API 的错误类型：越界访问被**类型化地拒绝**（返回 Err），而非 UB。
    #[derive(Debug, PartialEq, Eq, Clone, Copy)]
    pub enum FrameError {
        OutOfBounds,
    }

    /// 受控内存区句柄（≈ asterinas OSTD 的 `Frame`）。
    /// 内部持有裸指针 + 长度，但**只暴露经边界检查的 safe 方法**。
    pub struct Frame {
        base: *mut u8,
        len: usize,
    }

    impl Frame {
        pub fn len(&self) -> usize {
            self.len
        }

        /// 安全写：先做边界检查，越界返回 Err；唯有合法下标才落到裸指针写。
        pub fn write(&self, i: usize, v: u8) -> Result<(), FrameError> {
            // TODO[A]: 边界检查 —— 若 i >= self.len，return Err(FrameError::OutOfBounds)。
            //   缺了这一步，下面框架内部的裸指针写就会**越权改坏邻居 Frame**
            //   （TYPESAFE 子题会用相邻的两块 Frame 把这个失效演示出来）。
            //   HINT: if i >= self.len { return Err(FrameError::OutOfBounds); }
            unsafe {
                *self.base.add(i) = v; // 框架内唯一裸写（已给，勿动）
            }
            Ok(())
        }

        /// 安全读：先边界检查，再裸指针读（已给，作为封装范例）。
        pub fn read(&self, i: usize) -> Result<u8, FrameError> {
            if i >= self.len {
                return Err(FrameError::OutOfBounds);
            }
            unsafe { Ok(*self.base.add(i)) }
        }
    }

    /// 物理内存池（框架私有）。把一整块 buffer 切成互不重叠的 Frame 句柄。
    pub struct Pool {
        buf: Vec<u8>,
    }

    impl Pool {
        pub fn new(size: usize) -> Self {
            Pool {
                buf: vec![0u8; size],
            }
        }
        pub fn size(&self) -> usize {
            self.buf.len()
        }
        /// 切出 [base, base+len) 这一段为一个 Frame 句柄（裸指针算术，框架职责）。
        pub fn carve(&mut self, base: usize, len: usize) -> Frame {
            assert!(base + len <= self.buf.len());
            let p = unsafe { self.buf.as_mut_ptr().add(base) };
            Frame { base: p, len }
        }
        /// 仅供 harness 旁路核对「物理真相」：直接读底层字节（绕过 Frame 边界）。
        pub fn peek(&self, i: usize) -> u8 {
            self.buf[i]
        }
    }
    // LABCTL-TCB-END
}

// ════════════════════════════════════════════════════════════════
// OS Services（≈ asterinas kernel/）：内核子系统，**全部安全 Rust**。
// 模块级 `#![forbid(unsafe_code)]` 在**编译期**保证此处 0 unsafe。
// ════════════════════════════════════════════════════════════════
mod subsystems {
    #![forbid(unsafe_code)]
    // LABCTL-SUBSYS-BEGIN  —— 本区段内裸操作计数必须为 0（区段内不出现该英文关键词）
    use super::framework::{Frame, FrameError};

    /// 子系统：把一段数据存进自己的 Frame（全程安全 API）。
    pub fn store(frame: &Frame, data: &[u8]) -> Result<(), FrameError> {
        for (i, &b) in data.iter().enumerate() {
            frame.write(i, b)?;
        }
        Ok(())
    }

    /// 子系统：把自己的 Frame 读回到本地缓冲（全程安全 API）。
    pub fn load(frame: &Frame, out: &mut [u8]) -> Result<(), FrameError> {
        for (i, slot) in out.iter_mut().enumerate() {
            *slot = frame.read(i)?;
        }
        Ok(())
    }

    /// 子系统试图越界写 far（想改坏邻居的状态）。
    /// 安全 API 会把它挡成 Err；想绕过只能写裸指针——但本模块顶部的 forbid 属性
    /// 在编译期禁掉一切裸操作，下面这行反例**无法通过编译**（故只能留作注释）：
    ///     // let raw = frame as *const Frame as *mut u8; *raw.add(far) = 0xFF;
    /// 于是「越权」在框架之外**连写都写不出来**——这正是类型系统替代 MMU 的隔离。
    pub fn try_overreach(frame: &Frame, far: usize) -> Result<(), FrameError> {
        frame.write(far, 0xFF)
    }
    // LABCTL-SUBSYS-END
}

// ════════════════════════════════════════════════════════════════
// 测试 harness（给定，勿改）
// ════════════════════════════════════════════════════════════════

/// 取出源码中两个标记之间的区段（找不到则返回空串，不 panic）。
fn slice_between<'a>(src: &'a str, begin: &str, end: &str) -> &'a str {
    match src.find(begin) {
        Some(bi) => match src[bi..].find(end) {
            Some(ei) => &src[bi..bi + ei],
            None => "",
        },
        None => "",
    }
}

/// 1. FRAME —— 框架给安全 API、子系统经它存取、往返一致。
fn check_frame() -> bool {
    use framework::Pool;
    let mut ok = true;
    let mut pool = Pool::new(128);
    let frame = pool.carve(0, 64);

    let data = [0x11u8, 0x22, 0x33, 0x44];
    if subsystems::store(&frame, &data).is_err() {
        println!("FRAME_MISS 安全 API 写入合法下标却被拒");
        ok = false;
    }
    let mut back = [0u8; 4];
    if subsystems::load(&frame, &mut back).is_err() {
        println!("FRAME_MISS 安全 API 读取合法下标却被拒");
        ok = false;
    }
    if back != data {
        println!("FRAME_MISS 安全 API 往返不一致 got={:?} 应={:?}", back, data);
        ok = false;
    }
    if ok {
        println!("FRAME_PASS");
    }
    ok
}

/// 2. TYPESAFE —— 越界写被类型化拒绝，邻居 Frame 毫发无损（隔离靠类型不靠 MMU）。
fn check_typesafe() -> bool {
    use framework::Pool;
    let mut ok = true;
    // 同一个池里相邻切两块：A=[0,64) B=[64,128)。两块在物理上紧紧挨着。
    let mut pool = Pool::new(128);
    let frame_a = pool.carve(0, 64);
    let frame_b = pool.carve(64, 64);

    // 邻居子系统 B 先写入哨兵值。
    if subsystems::store(&frame_b, &[0xB0, 0xB1, 0xB2, 0xB3]).is_err() {
        println!("TYPESAFE_MISS B 写入自己的 Frame 失败");
        ok = false;
    }

    // 子系统 A 试图越界写 far=64：物理上正落在 B 的第 0 字节。
    let r = subsystems::try_overreach(&frame_a, 64);
    if r.is_ok() {
        println!("TYPESAFE_MISS A 越界写被放行（安全 API 边界检查缺失，越权改坏了邻居）");
        ok = false;
    }
    if pool.peek(64) != 0xB0 {
        println!(
            "TYPESAFE_MISS 邻居 B[0] 被改坏 pool[64]=0x{:02x} 应=0xB0",
            pool.peek(64)
        );
        ok = false;
    }
    if subsystems::try_overreach(&frame_a, 63).is_err() {
        println!("TYPESAFE_MISS A 在自己合法范围(下标 63)内的写被误拒");
        ok = false;
    }
    if ok {
        println!("TYPESAFE_PASS");
    }
    ok
}

/// 3. MINTCB —— 审计源码：unsafe 仅限框架 TCB，子系统区段为 0。
fn check_mintcb() -> bool {
    let _src = include_str!("main.rs");
    // TODO[B]: 用 slice_between 取出两个区段，再 matches("unsafe").count() 计数。
    //   let tcb = slice_between(_src, "LABCTL-TCB-BEGIN", "LABCTL-TCB-END");
    //   let sub = slice_between(_src, "LABCTL-SUBSYS-BEGIN", "LABCTL-SUBSYS-END");
    //   let tcb_n = tcb.matches("unsafe").count();
    //   let sub_n = sub.matches("unsafe").count();
    let tcb_n = 0usize; // ← 占位
    let sub_n = 0usize; // ← 占位
    println!("AUDIT framework_unsafe={} subsystem_unsafe={}", tcb_n, sub_n);

    let mut ok = true;
    if sub_n != 0 {
        println!(
            "MINTCB_MISS 子系统区段出现 {} 处裸操作（应为 0；越权能力须收敛在框架 TCB）",
            sub_n
        );
        ok = false;
    }
    if tcb_n == 0 {
        println!("MINTCB_MISS 框架区段未见裸操作（说明审计/计数逻辑没接上）");
        ok = false;
    }
    if ok {
        println!("MINTCB_PASS");
    }
    ok
}

fn main() {
    let mut all = true;
    all &= check_frame();
    all &= check_typesafe();
    all &= check_mintcb();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
