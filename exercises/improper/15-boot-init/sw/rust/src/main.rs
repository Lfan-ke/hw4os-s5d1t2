//! 引导入门 · 启动握手（软件模拟 MMIO）—— Rust 参考解。
//! 一句话母题：软件是硬件的开机咒语，链接顺序决定谁先念咒。
//!
//! 设备模型与测试 harness 已给定、勿改。你只需填两处：
//!   1) boot_init()      —— 四步握手：解锁 → 配时钟 → 使能 → 轮询 READY。
//!   2) register_inits() —— 把 boot_init 登记进 .init_array，让它先于 app_main 跑。
#![allow(dead_code)]

// ── 寄存器图（偏移即下标）──
const MMIO_UNLOCK: usize = 0;
const MMIO_CLKDIV: usize = 1;
const MMIO_CTRL: usize = 2;
const MMIO_STATUS: usize = 3;
const MMIO_DATA: usize = 4;

const MAGIC: u32 = 0xB007_C0DE; // 解锁咒语
const CTRL_EN: u32 = 0x1; // CTRL.bit0 使能
const CTRL_LE: u32 = 0x2; // CTRL.bit1 锁定使能

const ST_READY: u32 = 0x1; // STATUS.bit0 就绪
const ST_LOCKED: u32 = 0x2; // STATUS.bit1 未解锁
const ST_BADCLK: u32 = 0x4; // STATUS.bit2 CLKDIV 非法
const ST_NOTEN: u32 = 0x8; // STATUS.bit3 未使能

const BADBOOT: u32 = 0x0BAD_B007; // 未就绪误用 DATA 读回的胡话

// ── 设备模型（给定，勿改）──────────────────────────────────────────
#[derive(Default)]
struct Device {
    unlocked: bool,
    clkdiv: u32,
    en: bool,
    ready: bool,
    lock_ctr: u32,            // 就绪倒计时（模拟 PLL lock 轮询）
    data_raw: u32,           // 最近写入 DATA（低 16 位）
    touched_before_ready: bool, // 首次 DATA 访问发生在 READY 之前
}

impl Device {
    fn clkdiv_valid(&self) -> bool {
        (1..=15).contains(&self.clkdiv)
    }

    fn write(&mut self, reg: usize, val: u32) {
        match reg {
            MMIO_UNLOCK => self.unlocked = val == MAGIC,
            MMIO_CLKDIV => self.clkdiv = val,
            MMIO_CTRL => {
                self.en = val & CTRL_EN != 0;
                // EN 写入且已解锁且 CLKDIV 合法 → 启动锁定倒计时
                if self.en && self.unlocked && self.clkdiv_valid() {
                    if !self.ready && self.lock_ctr == 0 {
                        self.lock_ctr = 3;
                    }
                } else {
                    self.ready = false;
                    self.lock_ctr = 0;
                }
            }
            MMIO_DATA => {
                self.data_raw = val & 0xFFFF;
                if !self.ready {
                    self.touched_before_ready = true;
                }
            }
            _ => {}
        }
    }

    fn read(&mut self, reg: usize) -> u32 {
        match reg {
            MMIO_STATUS => {
                // 轮询期间时钟逐渐稳定：每读一次 STATUS 推进一拍锁定
                if self.lock_ctr > 0 {
                    self.lock_ctr -= 1;
                    if self.lock_ctr == 0 {
                        self.ready = true;
                    }
                }
                let mut s = 0;
                if self.ready {
                    s |= ST_READY;
                }
                if !self.unlocked {
                    s |= ST_LOCKED;
                }
                if !self.clkdiv_valid() {
                    s |= ST_BADCLK;
                }
                if !self.en {
                    s |= ST_NOTEN;
                }
                s
            }
            MMIO_DATA => {
                if !self.ready {
                    self.touched_before_ready = true;
                    BADBOOT // 未就绪误用 → 胡话
                } else {
                    dev_transform(self.data_raw)
                }
            }
            _ => 0,
        }
    }
}

fn dev_transform(raw: u32) -> u32 {
    (raw ^ 0xCAFE) & 0xFFFF
}

// ── 你要填的 (1)：启动握手 ──────────────────────────────────────────
/// boot_init：把设备从“半睡半醒”哄到 READY。四步：
///   解锁 → 配 CLKDIV → 使能 → 轮询 STATUS.READY。
fn boot_init(d: &mut Device) {
    let _ = d;
    // TODO: 四步握手（顺序很重要：解锁必须在使能之前）。
    //   ① d.write(MMIO_UNLOCK, MAGIC);              解锁配置总线
    //   ② d.write(MMIO_CLKDIV, /* 合法值 1..15 */); 配时钟，0 会 BADCLK
    //   ③ d.write(MMIO_CTRL, CTRL_EN | CTRL_LE);    使能
    //   ④ while d.read(MMIO_STATUS) & ST_READY == 0 { }   忙等轮询 READY
    // HINT: 取某位用 (status >> bit) & 1，或与掩码 ST_READY 相与。
    // 占位：什么都不做 → 设备仍未就绪 → 后续打 BOOT_FAULT。
}

// ── 应用程序：直接使用设备（假定 boot 已先跑好）──
fn app_main(d: &mut Device) -> u32 {
    d.write(MMIO_DATA, 0x42);
    d.read(MMIO_DATA)
}

// ── 你要填的 (2)：把 boot_init 链接/排到 main 之前 ──────────────────
// 模拟 ELF 的 .init_array：crt 在进 app_main 之前会遍历这张函数指针表逐一调用。
// 把 boot_init 登记进来，app_main 才不会“抢跑”。
type InitFn = fn(&mut Device);

fn register_inits() -> Vec<InitFn> {
    // TODO: 返回需要在 app_main 之前执行的初始化函数（顺序即调用顺序）。
    //   正确做法：vec![boot_init] —— 让 crt 在进 app_main 前先把设备哄就绪。
    // 占位：空表 → 没有任何 init 先跑 → app_main 抢跑 → 打 BOOT_FAULT。
    Vec::new()
}

// 给定的 C 运行时模拟（crt0）：先遍历 .init_array，再进 app_main。勿改。
fn crt_start(d: &mut Device) -> u32 {
    for f in register_inits() {
        f(d); // .init_array 在 main 之前执行
    }
    app_main(d)
}

// ── 测试 harness（给定，勿改）──────────────────────────────────────

/// 15.1 LOCK：观察“坏掉的启动”——未握手直接用 DATA 被正确拒。
fn stage_lock() -> bool {
    let mut d = Device::default();
    let bad = d.read(MMIO_DATA); // 跳过握手直接用
    let st = d.read(MMIO_STATUS);
    if bad == BADBOOT && st & ST_LOCKED != 0 {
        println!("LOCK_PASS");
        true
    } else {
        println!(
            "LOCK_FAIL 未握手应吐 0x{:08X} 且 LOCKED：got data=0x{:08X} status=0x{:X}",
            BADBOOT, bad, st
        );
        false
    }
}

/// 15.2 BOOT/USE：握手后就绪、读写 DATA 变换正确。
fn stage_boot_use() -> bool {
    let mut d = Device::default();
    boot_init(&mut d);
    let st = d.read(MMIO_STATUS);
    if st & ST_READY == 0 {
        println!("BOOT_FAULT 握手后设备未就绪 status=0x{:X}", st);
        return false;
    }
    println!("BOOT_PASS");
    d.write(MMIO_DATA, 0x1234);
    let r = d.read(MMIO_DATA);
    let exp = dev_transform(0x1234);
    if r != exp {
        println!("USE_FAIL DATA 变换错 got=0x{:X} exp=0x{:X}", r, exp);
        return false;
    }
    println!("USE_PASS");
    true
}

/// 15.3 ORDER：boot 必须先于 app 跑。crt_start 会先遍历 .init_array 再进 app_main。
fn stage_order() -> bool {
    let mut d = Device::default();
    let r = crt_start(&mut d);
    if !d.ready || d.touched_before_ready {
        println!("BOOT_FAULT app_main 抢跑：首次 DATA 访问发生在 READY 之前");
        return false;
    }
    let exp = dev_transform(0x42);
    if r != exp {
        println!("ORDER_FAIL app 读回错 got=0x{:X} exp=0x{:X}", r, exp);
        return false;
    }
    println!("ORDER_PASS");
    true
}

fn main() {
    let mut all = true;
    all &= stage_lock();
    all &= stage_boot_use();
    all &= stage_order();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
