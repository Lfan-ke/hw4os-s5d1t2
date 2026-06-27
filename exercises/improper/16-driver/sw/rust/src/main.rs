//! 16 · 驱动入门 —— Rust（学生填空版）。
//! 一节贯通四子题：① 裸机 MMIO 手工艺 ② 设备树解析 + compatible 匹配
//! ③ driver derive 可插拔注册 ④ 平台总线 + 用户态 /dev 访问。
//! 设备/MMIO 全部用软件寄存器模型建模（host 直接跑，纯逻辑）。
//! 你只需填四处 TODO（每子题一处）+ board_nodes 的 compatible/reg；测试 harness 勿改。
#![allow(dead_code, unused_variables)]

use std::collections::HashMap;

// ════════════════════════════════════════════════════════════════════
// 16.1 裸机 MMIO 手工艺人 —— 对固定地址手敲一个设备
// ════════════════════════════════════════════════════════════════════
// 最小 MMIO 寄存器契约（偏移）：
const REG_ID: usize = 0x0; // 只读：读到 magic 才算探到设备
const REG_CTRL: usize = 0x4; // 写：bit0=使能
const REG_STATUS: usize = 0x8; // 只读：bit0=ready
const REG_DATA: usize = 0xC; // 读/写：数据口（写=驱动设备吐字节）
const DEV_MAGIC: u32 = 0x426C_6E6B; // "Blnk"
const CTRL_ENABLE: u32 = 1;
const STATUS_READY: u32 = 1;

/// 设备模型（“硬件”侧）。驱动只许透过 mmio_read/mmio_write 访问它。
struct MmioDevice {
    enabled: bool,
    last: u8,
    out: Vec<u8>, // 设备真正“打印”出去的字节——证明你真把它驱动起来了
}
impl MmioDevice {
    fn new() -> Self {
        MmioDevice { enabled: false, last: 0, out: Vec::new() }
    }
    /// 设备对一次 MMIO 写的反应（寄存器状态机）。
    fn bus_write(&mut self, off: usize, val: u32) {
        match off {
            REG_CTRL => self.enabled = (val & CTRL_ENABLE) != 0,
            REG_DATA => {
                if self.enabled {
                    self.last = val as u8;
                    self.out.push(val as u8);
                }
            }
            _ => {} // ID/STATUS 只读，写被忽略
        }
    }
    /// 设备对一次 MMIO 读的反应。
    fn bus_read(&self, off: usize) -> u32 {
        match off {
            REG_ID => DEV_MAGIC,
            REG_CTRL => if self.enabled { CTRL_ENABLE } else { 0 },
            REG_STATUS => if self.enabled { STATUS_READY } else { 0 },
            REG_DATA => self.last as u32,
            _ => 0,
        }
    }
}

// 模拟 readl/writel：对“地址”的 volatile 读写。
fn mmio_read(dev: &MmioDevice, off: usize) -> u32 {
    dev.bus_read(off)
}
fn mmio_write(dev: &mut MmioDevice, off: usize, val: u32) {
    dev.bus_write(off, val);
}

// ── 学生填 16.1 ─────────────────────────────────────────────────────

/// probe：volatile 读 ID 寄存器，与 DEV_MAGIC 比对，相等才算探到设备。
fn driver_probe(dev: &MmioDevice) -> bool {
    // TODO: 用 mmio_read(dev, REG_ID) 读 ID，和 DEV_MAGIC 比对，相等返回 true。
    // HINT: mmio_read(dev, REG_ID) == DEV_MAGIC
    false // ← 占位：未探到 → PROBE_FAIL
}

/// 初始化握手 + 突发收发：① 置 CTRL 使能 ② 轮询 STATUS.ready ③ 逐字节写 DATA。
fn driver_io(dev: &mut MmioDevice, msg: &[u8]) {
    // TODO[a] 忙等轮询 STATUS 单字节握手：
    //   mmio_write(dev, REG_CTRL, CTRL_ENABLE);            // 置使能位
    //   while mmio_read(dev, REG_STATUS) & STATUS_READY == 0 { /* 轮询，记得设上限防死等 */ }
    //   for &b in msg { mmio_write(dev, REG_DATA, b as u32); } // 逐字节突发写
    // ELSE[b] 也可：读“可写计数”寄存器后一次性突发写——对外输出一致。
    // 占位：什么都不做 → 设备未使能、out 为空 → IO_FAIL
}

fn sub_mmio() -> bool {
    let mut dev = MmioDevice::new();
    let mut ok = true;
    if driver_probe(&dev) {
        println!("PROBE_PASS 读到 magic 0x{:08X}", DEV_MAGIC);
    } else {
        println!("PROBE_FAIL ID 寄存器未读到 magic 0x{:08X}", DEV_MAGIC);
        ok = false;
    }
    let msg = b"DRV-OK";
    driver_io(&mut dev, msg);
    if dev.enabled && dev.out == msg {
        println!("IO_PASS 设备经握手收到 {} 字节", dev.out.len());
    } else {
        println!("IO_FAIL enabled={} out={:?}", dev.enabled, dev.out);
        ok = false;
    }
    if ok {
        println!("MMIO_PASS");
    }
    ok
}

// ════════════════════════════════════════════════════════════════════
// 16.2 设备树：DTS → dtc → dtb → 解析 → compatible 匹配
// ════════════════════════════════════════════════════════════════════
// 给定：最小 FDT 序列化/遍历（大端、magic + BEGIN_NODE/PROP/END_NODE/END）。
const FDT_MAGIC: u32 = 0xd00d_feed;
const FDT_BEGIN_NODE: u32 = 0x1;
const FDT_END_NODE: u32 = 0x2;
const FDT_PROP: u32 = 0x3;
const FDT_NOP: u32 = 0x4;
const FDT_END: u32 = 0x9;

#[derive(Clone)]
struct DevNode {
    name: String,
    compatible: String,
    base: u32,
    size: u32,
}

fn be32(b: &[u8], o: usize) -> u32 {
    u32::from_be_bytes([b[o], b[o + 1], b[o + 2], b[o + 3]])
}
fn push_be32(v: &mut Vec<u8>, x: u32) {
    v.extend_from_slice(&x.to_be_bytes());
}
fn pad4(v: &mut Vec<u8>) {
    while v.len() % 4 != 0 {
        v.push(0);
    }
}
fn cstr(b: &[u8], o: usize) -> String {
    let mut e = o;
    while e < b.len() && b[e] != 0 {
        e += 1;
    }
    String::from_utf8_lossy(&b[o..e]).into_owned()
}

/// 把节点表序列化成真正的扁平设备树二进制（FDT 线格式）。
fn build_fdt(nodes: &[DevNode]) -> Vec<u8> {
    let mut strtab: Vec<u8> = Vec::new();
    let comp_off = strtab.len() as u32;
    strtab.extend_from_slice(b"compatible\0");
    let reg_off = strtab.len() as u32;
    strtab.extend_from_slice(b"reg\0");

    let mut st: Vec<u8> = Vec::new();
    push_be32(&mut st, FDT_BEGIN_NODE);
    st.push(0); // 根节点名 ""
    pad4(&mut st);
    for n in nodes {
        push_be32(&mut st, FDT_BEGIN_NODE);
        st.extend_from_slice(n.name.as_bytes());
        st.push(0);
        pad4(&mut st);
        // compatible
        let cb = n.compatible.as_bytes();
        push_be32(&mut st, FDT_PROP);
        push_be32(&mut st, (cb.len() + 1) as u32);
        push_be32(&mut st, comp_off);
        st.extend_from_slice(cb);
        st.push(0);
        pad4(&mut st);
        // reg = <base size>
        push_be32(&mut st, FDT_PROP);
        push_be32(&mut st, 8);
        push_be32(&mut st, reg_off);
        push_be32(&mut st, n.base);
        push_be32(&mut st, n.size);
        push_be32(&mut st, FDT_END_NODE);
    }
    push_be32(&mut st, FDT_END_NODE); // 关根
    push_be32(&mut st, FDT_END);

    let mem = vec![0u8; 16]; // 内存保留表：单个 (0,0) 终止项
    let off_mem = 40usize; // 头 10×u32
    let off_struct = off_mem + mem.len();
    let off_strings = off_struct + st.len();
    let total = off_strings + strtab.len();

    let mut out = Vec::new();
    push_be32(&mut out, FDT_MAGIC);
    push_be32(&mut out, total as u32);
    push_be32(&mut out, off_struct as u32);
    push_be32(&mut out, off_strings as u32);
    push_be32(&mut out, off_mem as u32);
    push_be32(&mut out, 17); // version
    push_be32(&mut out, 16); // last_comp_version
    push_be32(&mut out, 0); // boot_cpuid_phys
    push_be32(&mut out, strtab.len() as u32);
    push_be32(&mut out, st.len() as u32);
    out.extend_from_slice(&mem);
    out.extend_from_slice(&st);
    out.extend_from_slice(&strtab);
    out
}

/// 大端遍历 FDT 结构块，取每个设备节点的 compatible / reg。
fn parse_fdt(b: &[u8]) -> Vec<DevNode> {
    let off_struct = be32(b, 8) as usize;
    let off_strings = be32(b, 12) as usize;
    let mut pos = off_struct;
    let mut out = Vec::new();
    let mut stack: Vec<DevNode> = Vec::new();
    loop {
        let tok = be32(b, pos);
        pos += 4;
        match tok {
            x if x == FDT_BEGIN_NODE => {
                let name = cstr(b, pos);
                pos += (name.len() + 1 + 3) & !3;
                stack.push(DevNode { name, compatible: String::new(), base: 0, size: 0 });
            }
            x if x == FDT_PROP => {
                let len = be32(b, pos) as usize;
                let nameoff = be32(b, pos + 4) as usize;
                pos += 8;
                let pname = cstr(b, off_strings + nameoff);
                let val = &b[pos..pos + len];
                if let Some(top) = stack.last_mut() {
                    if pname == "compatible" {
                        top.compatible = cstr(val, 0);
                    } else if pname == "reg" && len >= 8 {
                        top.base = be32(val, 0);
                        top.size = be32(val, 4);
                    }
                }
                pos += (len + 3) & !3;
            }
            x if x == FDT_END_NODE => {
                if let Some(n) = stack.pop() {
                    if !n.name.is_empty() {
                        out.push(n);
                    }
                }
            }
            x if x == FDT_NOP => {}
            _ => break, // FDT_END 或越界
        }
    }
    out
}

// 驱动表项：compatible 字符串 → (name, probe)。
#[derive(Clone, Copy)]
struct Driver {
    compatible: &'static str,
    name: &'static str,
    probe: fn(u32, u32) -> DevFile,
}
macro_rules! driver {
    ($c:expr, $n:expr, $p:expr) => {
        Driver { compatible: $c, name: $n, probe: $p }
    };
}

fn probe_blink(_base: u32, _size: u32) -> DevFile {
    DevFile { state: 0 }
}
fn probe_gpio(_base: u32, _size: u32) -> DevFile {
    DevFile { state: 0 }
}

const D_BLINK: Driver = driver!("acme,blink", "blink", probe_blink);
const D_GPIO: Driver = driver!("acme,gpio", "gpio", probe_gpio);

/// 16.2 阶段：驱动表此时只有 blink/gpio 两个（blink-v2 尚未注册）。
fn base_drivers() -> Vec<Driver> {
    vec![D_BLINK, D_GPIO]
}

/// 等价于 board.dts 的设备节点表。学生填 blink/gpio 的 compatible 与 reg。
fn board_nodes() -> Vec<DevNode> {
    vec![
        // TODO: blink@10001000：compatible 应为 "acme,blink"，reg=<0x10001000 0x1000>
        DevNode { name: "blink@10001000".into(), compatible: "TODO,fillme".into(), base: 0, size: 0 },
        // TODO: gpio@10002000：compatible 应为 "acme,gpio"，reg=<0x10002000 0x1000>
        DevNode { name: "gpio@10002000".into(), compatible: "TODO,fillme".into(), base: 0, size: 0 },
        // lamp@10003000（给定，勿改）：16.2 时是未知 compatible，16.3 注册 blink-v2 后被发现。
        DevNode { name: "lamp@10003000".into(), compatible: "acme,blink-v2".into(), base: 0x1000_3000, size: 0x1000 },
    ]
}

// ── 学生填 16.2：解析主循环里的匹配 ─────────────────────────────────
/// 遍历节点，在驱动表里逐条精确字符匹配 compatible；命中即 probe 并记录，
/// 未知项走 fallback 跳过。返回 (跳过数, 命中列表 "name@base")。
fn match_and_probe(nodes: &[DevNode], drivers: &[Driver]) -> (usize, Vec<String>) {
    let mut skipped = 0;
    let mut matched = Vec::new();
    // TODO: 遍历 nodes，对每个节点在 drivers 里逐条 d.compatible == n.compatible 精确匹配：
    //   命中 → (d.probe)(n.base, n.size); println!("PROBE {}@{:x}", d.name, n.base);
    //          matched.push(format!("{}@{:x}", d.name, n.base)); break;
    //   一个都不命中 → skipped += 1（未知 compatible 走 fallback 跳过）
    // HINT: 用 // TODO[a] fdt crate/libfdt 解析（这里已给手写 FDT）/ // ELSE[b] 手写遍历，二者择一即可。
    // 占位：什么都不匹配 → MATCH_FAIL
    (skipped, matched)
}

fn sub_dtb() -> bool {
    let nodes = board_nodes();
    let blob = build_fdt(&nodes);
    let parsed = parse_fdt(&blob);
    let mut ok = true;

    let magic = be32(&blob, 0);
    if magic == FDT_MAGIC && parsed.len() == 3 {
        println!("DTB_PASS magic=0x{:08x} nodes={}", magic, parsed.len());
    } else {
        println!("DTB_FAIL magic=0x{:08x} nodes={}", magic, parsed.len());
        ok = false;
    }

    let drivers = base_drivers();
    let (sk1, mut m1) = match_and_probe(&parsed, &drivers);
    // 隐藏向量：打乱节点顺序后匹配结果应不变（按名片匹配，而非位置）。
    let mut shuffled = parsed.clone();
    shuffled.reverse();
    let (sk2, mut m2) = match_and_probe(&shuffled, &drivers);
    m1.sort();
    m2.sort();
    let want = vec!["blink@10001000".to_string(), "gpio@10002000".to_string()];
    if sk1 == 1 && sk2 == 1 && m1 == want && m1 == m2 {
        println!("MATCH_PASS 命中 blink/gpio、跳过未知 acme,blink-v2、乱序不变");
    } else {
        println!("MATCH_FAIL skipped={}/{} matched={:?} shuffled={:?}", sk1, sk2, m1, m2);
        ok = false;
    }
    ok
}

// ════════════════════════════════════════════════════════════════════
// 16.3 driver derive：可插拔注册（加一个驱动 = 加一行登记，框架零改）
// ════════════════════════════════════════════════════════════════════
// ── 学生填 16.3：往登记表追加第三个驱动（下方框架遍历一行不改）──────
fn all_drivers() -> Vec<Driver> {
    vec![
        D_BLINK,
        D_GPIO,
        // TODO 16.3: 在此追加“这一行登记”，让 acme,blink-v2 被自动发现并 probe（框架其余代码一行不改）：
        //   driver!("acme,blink-v2", "blink2", probe_blink),
    ]
}

fn sub_derive() -> bool {
    let nodes = board_nodes();
    let parsed = parse_fdt(&build_fdt(&nodes));
    let drivers = all_drivers(); // 含新注册的第三个
    let (skipped, matched) = match_and_probe(&parsed, &drivers);
    let found = matched.iter().any(|s| s == "blink2@10003000");
    if skipped == 0 && found {
        println!("DERIVE_PASS 新驱动 acme,blink-v2 被自动发现并 probe");
        println!("PLUG_PASS 框架遍历逻辑零改，加一行登记即接纳新驱动");
        true
    } else {
        println!("DERIVE_FAIL skipped={} found_v2={}", skipped, found);
        false
    }
}

// ════════════════════════════════════════════════════════════════════
// 16.4 平台总线（简化）+ 用户态访问
// ════════════════════════════════════════════════════════════════════
/// 设备实例：FileLike（read/write），复用“设备即文件”抽象。
struct DevFile {
    state: u32,
}
impl DevFile {
    fn read(&self) -> u32 {
        self.state
    }
    fn write(&mut self, v: u32) {
        self.state = v;
    }
}

/// 单总线：把 /dev/<name> 映射到设备实例。
struct Bus {
    devs: Vec<(String, DevFile)>,
}

// ── 学生填 16.4：总线枚举 → match → bind → 登记 /dev ────────────────
fn bind_all(bus: &mut Bus, nodes: &[DevNode], drivers: &[Driver]) {
    let mut counter: HashMap<&str, u32> = HashMap::new();
    // TODO: 枚举 nodes，对每个匹配到的驱动 bind：
    //   let inst = (d.probe)(n.base, n.size);            // bind = 调 probe 建实例
    //   let idx = counter.entry(d.name).or_insert(0);    // 同名驱动的实例编号
    //   bus.devs.push((format!("/dev/{}{}", d.name, *idx), inst)); *idx += 1; break;
    // 占位：什么都不绑定 → BIND_FAIL
}

/// 用户态 open+read：找到 /dev 路径的 FileLike，转发 read（不直接碰 MMIO）。
fn user_read(bus: &Bus, path: &str) -> Option<u32> {
    // TODO: 在 bus.devs 里按 path 找到设备，转发它的 .read()
    // HINT: bus.devs.iter().find(|(p, _)| p == path).map(|(_, f)| f.read())
    None // ← 占位
}
/// 用户态 write：转发到设备 ops。
fn user_write(bus: &mut Bus, path: &str, v: u32) -> bool {
    // TODO: 在 bus.devs 里按 path 找到设备，调用它的 .write(v)，成功返回 true
    false // ← 占位
}

fn sub_bus() -> bool {
    let nodes = board_nodes();
    let parsed = parse_fdt(&build_fdt(&nodes));
    let drivers = all_drivers();
    let mut bus = Bus { devs: Vec::new() };
    bind_all(&mut bus, &parsed, &drivers);
    let mut ok = true;

    let have: Vec<String> = bus.devs.iter().map(|(p, _)| p.clone()).collect();
    let want = ["/dev/blink0", "/dev/gpio0", "/dev/blink20"];
    if bus.devs.len() == 3 && want.iter().all(|w| have.iter().any(|h| h == w)) {
        println!("BIND_PASS 绑定 {:?}", have);
    } else {
        println!("BIND_FAIL have={:?}", have);
        ok = false;
    }

    let path = "/dev/blink0";
    let before = user_read(&bus, path);
    let w = user_write(&mut bus, path, 0xA5);
    let after = user_read(&bus, path);
    if before == Some(0) && w && after == Some(0xA5) {
        println!("USER_PASS open/read/write 转发一致（写改状态、读回一致）");
    } else {
        println!("USER_FAIL before={:?} w={} after={:?}", before, w, after);
        ok = false;
    }

    if ok {
        println!("BUS_PASS");
    }
    ok
}

fn main() {
    let mut all = true;
    all &= sub_mmio();
    all &= sub_dtb();
    all &= sub_derive();
    all &= sub_bus();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
