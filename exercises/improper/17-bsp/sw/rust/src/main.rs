//! 板级入门 · BSP 与设备树 —— Rust。
//! 母题：BSP = 把"散落的硬编码板级常量"收敛成一层可替换的板级胶水；
//!       设备树（DT）= firmware↔OS 的稳定 ABI，让同一个 kmain 跑遍多块板。
//! 四段递进：
//!   1) bsp_probe   —— 硬编码 BSP 表（换块板就跑飞的痛）
//!   2) parse_dt    —— 删掉 match 表，改"读平面图"（设备树）
//!   3) driver_bind —— compatible 字符串匹配，驱动与硬件解耦
//!   4) parse_dt_v2 —— bootloader/DT 不变，OS 升级仍向后兼容
//! 你只需填四个函数体；下方测试 harness（MMIO 总线模型 + PASS 打印）勿改。
#![allow(unused_variables, dead_code)]

// ── 板级配置 ──────────────────────────────────────────────────────
#[derive(Clone, Copy, PartialEq, Debug)]
struct BoardConfig {
    uart_base: u32, // UART 的 MMIO 基址
    clk_hz: u32,    // 总线时钟
}

// 两块板的"真身"地址：板 A=地址 C，板 B=地址 D（同一内核换板只有这里不同）。
const A_UART_BASE: u32 = 0x1000_0000;
const A_CLK_HZ: u32 = 10_000_000;
const B_UART_BASE: u32 = 0x1002_0000;
const B_CLK_HZ: u32 = 50_000_000;
const DEFAULT_CLK_HZ: u32 = 10_000_000; // v2 内核对缺失 clock-frequency 的兜底
const UART_WIN: u32 = 0x1000; // 4KB MMIO 窗口

// ── 用内存数组模拟 MMIO 总线（设备 = 一个地址窗口 + 捕获缓冲）──────
struct Device {
    base: u32,
    size: u32,
    captured: Vec<u8>, // 写到 THR(offset 0) 的字节
}
struct Bus {
    devices: Vec<Device>,
    faults: u32, // 落在任何窗口外的写 = 总线错误
}
impl Bus {
    fn write8(&mut self, addr: u32, byte: u8) {
        for d in self.devices.iter_mut() {
            if addr >= d.base && addr < d.base + d.size {
                if addr == d.base {
                    d.captured.push(byte); // THR
                }
                return;
            }
        }
        self.faults += 1; // 地址不落在任何设备 → 写飞了
    }
    fn device_at(&self, base: u32) -> Option<&Device> {
        self.devices.iter().find(|d| d.base == base)
    }
}

fn make_uart_bus(uart_base: u32) -> Bus {
    Bus {
        devices: vec![Device { base: uart_base, size: UART_WIN, captured: Vec::new() }],
        faults: 0,
    }
}

const BANNER: &[u8] = b"vlab-os\n";

/// 通用内核入口：只认 cfg.uart_base，板级细节一概不知。
fn kmain(bus: &mut Bus, cfg: &BoardConfig) {
    for &b in BANNER {
        bus.write8(cfg.uart_base, b);
    }
}

// ── 设备树（mini-DT：扁平节点数组 + 命名属性，代替真实 FDT 二进制）──
#[derive(Clone)]
struct DtProp {
    name: &'static str,
    val: u32,
}
#[derive(Clone)]
struct DtNode {
    compatible: &'static str,
    reg_base: u32,
    reg_size: u32,
    irq: u32,
    props: Vec<DtProp>,
}
type Dtb = Vec<DtNode>;

fn dtb_a() -> Dtb {
    vec![
        DtNode {
            compatible: "vlab,uart",
            reg_base: A_UART_BASE,
            reg_size: UART_WIN,
            irq: 1,
            props: vec![DtProp { name: "clock-frequency", val: A_CLK_HZ }],
        },
        DtNode {
            compatible: "vlab,timer",
            reg_base: 0x0200_0000,
            reg_size: 0x1000,
            irq: 7,
            props: vec![DtProp { name: "clock-frequency", val: A_CLK_HZ }],
        },
    ]
}

fn dtb_b() -> Dtb {
    vec![
        DtNode {
            compatible: "vlab,uart",
            reg_base: B_UART_BASE,
            reg_size: UART_WIN,
            irq: 2,
            props: vec![DtProp { name: "clock-frequency", val: B_CLK_HZ }],
        },
        DtNode {
            compatible: "vlab,timer",
            reg_base: 0x0200_0000,
            reg_size: 0x1000,
            irq: 7,
            props: vec![DtProp { name: "clock-frequency", val: B_CLK_HZ }],
        },
    ]
}

/// 老 bootloader 产出的 DT：UART 节点没有 clock-frequency（v1 还没这属性），
/// 且带一个 v2 内核不认识的新属性 —— 用来验证"向后兼容"。
fn dtb_a_old() -> Dtb {
    vec![
        DtNode {
            compatible: "vlab,uart",
            reg_base: A_UART_BASE,
            reg_size: UART_WIN,
            irq: 1,
            props: vec![DtProp { name: "vlab,unknown-feature", val: 0xdead }],
        },
        DtNode {
            compatible: "vlab,timer",
            reg_base: 0x0200_0000,
            reg_size: 0x1000,
            irq: 7,
            props: vec![],
        },
    ]
}

// ── 驱动注册表 ────────────────────────────────────────────────────
struct DriverRec {
    compatible: &'static str,
    bound: u32, // 该驱动绑定到的节点数
}

// ═══════════════ 四段核心逻辑（学生填）═══════════════════════════

/// 1) 硬编码 BSP 表：board_id → BoardConfig。
fn bsp_probe(board_id: u32) -> BoardConfig {
    // TODO: 按 board_id 返回对应板的 BoardConfig。
    //   board 0 → { uart_base: A_UART_BASE, clk_hz: A_CLK_HZ }
    //   board 1 → { uart_base: B_UART_BASE, clk_hz: B_CLK_HZ }
    // 分支择一：
    //   // TODO[a] 用静态 match 表（match board_id { 0 => …, 1 => …, _ => … }）
    //   // ELSE[b] 用按 id 索引的数组 BOARDS[board_id as usize]
    BoardConfig { uart_base: 0, clk_hz: 0 } // ← 占位：base=0 → 写飞 → 判 FAIL
}

/// 2) 用设备树替代硬编码：找 compatible=="vlab,uart" 的节点取 reg/clk。
fn parse_dt(blob: &Dtb) -> BoardConfig {
    // TODO: 遍历 blob，匹配 node.compatible == "vlab,uart"：
    //   uart_base = node.reg_base；在 node.props 里找 name=="clock-frequency" 取 val → clk_hz。
    // 分支择一：
    //   // TODO[a] 顺序扫描所有节点匹配 compatible
    //   // ELSE[b] 直接按已知偏移 blob[0] 取（前提是约定 uart 恒为首节点）
    BoardConfig { uart_base: 0, clk_hz: 0 } // ← 占位：判 FAIL
}

/// 3) compatible 字符串匹配：每节点 × 驱动表，相等则 probe，统计绑定数。
fn driver_bind(blob: &Dtb, regs: &mut [DriverRec]) -> u32 {
    // TODO: 对每个 node，遍历 regs；drv.compatible == node.compatible 则：
    //   drv.bound += 1; 总绑定数 += 1; break。返回总绑定数。
    // HINT: 字符串相等即视为"这块驱动认领这个设备"（真实内核此处会 drv.probe(reg, irq)）。
    0 // ← 占位：0 绑定 → 判 FAIL
}

/// 4) 向后兼容的 parse_dt_v2：clock-frequency 可选（缺失则默认）；
///    不认识的属性跳过而非报错。
fn parse_dt_v2(blob: &Dtb) -> BoardConfig {
    // TODO: 同 parse_dt 找 uart_base；但 clk_hz 初值设为 DEFAULT_CLK_HZ，
    //   只有遇到 name=="clock-frequency" 才覆盖；其余未知属性跳过（不要 panic/报错）。
    // 分支择一：
    //   // TODO[a] 缺省值兜底：clk_hz 先置 DEFAULT_CLK_HZ，命中才改
    //   // ELSE[b] 显式 Option：扫到则 Some(val)，最后 unwrap_or(DEFAULT_CLK_HZ)
    BoardConfig { uart_base: 0, clk_hz: 0 } // ← 占位：判 FAIL
}

// ═══════════════ 测试 harness（勿改）═════════════════════════════

fn banner_ok(dev: Option<&Device>) -> bool {
    dev.map(|d| d.captured == BANNER).unwrap_or(false)
}

fn check_probe(tag: &str, board_id: u32, want_base: u32) -> bool {
    let cfg = bsp_probe(board_id);
    let mut bus = make_uart_bus(want_base);
    kmain(&mut bus, &cfg); // 同一个 kmain，换板只换 cfg
    let ok = bus.faults == 0 && banner_ok(bus.device_at(want_base));
    if ok {
        println!("PROBE_{}_PASS", tag);
    } else {
        println!(
            "PROBE_{}_FAIL board={} base=0x{:08x} faults={}",
            tag, board_id, cfg.uart_base, bus.faults
        );
    }
    ok
}

fn check_dt(tag: &str, blob: &Dtb, want_base: u32, want_clk: u32) -> bool {
    let cfg = parse_dt(blob);
    let mut bus = make_uart_bus(want_base);
    kmain(&mut bus, &cfg);
    let ok = cfg.uart_base == want_base
        && cfg.clk_hz == want_clk
        && bus.faults == 0
        && banner_ok(bus.device_at(want_base));
    if ok {
        println!("DT_{}_PASS", tag);
    } else {
        println!(
            "DT_{}_FAIL base=0x{:08x}(exp 0x{:08x}) clk={}(exp {}) faults={}",
            tag, cfg.uart_base, want_base, cfg.clk_hz, want_clk, bus.faults
        );
    }
    ok
}

fn check_bind(blob: &Dtb) -> bool {
    let mut regs = [
        DriverRec { compatible: "vlab,uart", bound: 0 },
        DriverRec { compatible: "vlab,timer", bound: 0 },
    ];
    let total = driver_bind(blob, &mut regs);
    let mut ok = true;
    let uart_bound = regs.iter().find(|d| d.compatible == "vlab,uart").map(|d| d.bound).unwrap_or(0);
    let timer_bound = regs.iter().find(|d| d.compatible == "vlab,timer").map(|d| d.bound).unwrap_or(0);
    if uart_bound == 1 {
        println!("BIND_uart_PASS");
    } else {
        println!("BIND_uart_FAIL bound={}", uart_bound);
        ok = false;
    }
    if timer_bound == 1 {
        println!("BIND_timer_PASS");
    } else {
        println!("BIND_timer_FAIL bound={}", timer_bound);
        ok = false;
    }
    if total as usize == blob.len() {
        println!("BIND_PASS");
    } else {
        println!("BIND_FAIL total={} nodes={}", total, blob.len());
        ok = false;
    }
    ok
}

fn check_upgrade() -> bool {
    let blob = dtb_a_old(); // 老 DT，未改动
    let cfg = parse_dt_v2(&blob);
    let mut bus = make_uart_bus(A_UART_BASE);
    kmain(&mut bus, &cfg);
    let mut regs = [
        DriverRec { compatible: "vlab,uart", bound: 0 },
        DriverRec { compatible: "vlab,timer", bound: 0 },
    ];
    let total = driver_bind(&blob, &mut regs);
    let ok = cfg.uart_base == A_UART_BASE
        && cfg.clk_hz == DEFAULT_CLK_HZ // 缺失 → 兜底默认值
        && bus.faults == 0
        && banner_ok(bus.device_at(A_UART_BASE))
        && total as usize == blob.len();
    if ok {
        println!("UPGRADE_PASS");
    } else {
        println!(
            "UPGRADE_FAIL base=0x{:08x} clk={} total={} faults={}",
            cfg.uart_base, cfg.clk_hz, total, bus.faults
        );
    }
    ok
}

fn main() {
    let mut all = true;

    // 1) 硬编码 BSP 表：同一 kmain 跑两块板
    all &= check_probe("A", 0, A_UART_BASE);
    all &= check_probe("B", 1, B_UART_BASE);

    // 2) 用设备树替代硬编码：同一 parse_dt + kmain 喂两份 blob
    all &= check_dt("A", &dtb_a(), A_UART_BASE, A_CLK_HZ);
    all &= check_dt("B", &dtb_b(), B_UART_BASE, B_CLK_HZ);

    // 3) compatible 字符串匹配 → 驱动绑定
    all &= check_bind(&dtb_a());

    // 4) bootloader/DT 不变，OS 升级（v2）仍可启动
    all &= check_upgrade();

    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
