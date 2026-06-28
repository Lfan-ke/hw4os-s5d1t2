//! 16a · 总线与缓存 - Rust（学生填空版）。软件总线模型 = 地址区间译码分发 + cache 层 + 突发计时。
//! env=host：纯逻辑直接跑（pure std）。你需填三处（其余 harness/设备模型/计时骨架勿改）：
//!   ① 仲裁区间判定 decode()（§2.2）   ② 直通判定 *_passthrough()（§2.3 反例）
//!   ③ 突发分支 level_burst() 的握手次数（§2.3 时间模型）。

use std::thread::sleep;
use std::time::{Duration, Instant};

const REG_BASE: u32 = 0x4000_0000;
const REG_END: u32 = 0x4000_1000;
const SEN_BASE: u32 = 0x4001_0000;
const SEN_END: u32 = 0x4001_0010;
const SW_BASE: u32 = 0x4002_0000;
const SW_END: u32 = 0x4002_0010;

#[derive(PartialEq, Clone, Copy)]
enum Dev {
    Reg,
    Sensor,
    Switch,
    Err,
}

fn decode(a: u32) -> Dev {
    // TODO①: 仲裁 = 地址区间译码。按 §2.2 判 a 落哪段区间（`a>=BASE && a<END`），
    //   命中返回 Dev::Reg/Sensor/Switch，全落空返回 Dev::Err。
    let _ = (a, REG_BASE, REG_END, SEN_BASE, SEN_END, SW_BASE, SW_END);
    Dev::Err // ← 占位：所有地址都判成总线错误 → 路由不符 → 无 ARB_PASS
}

fn level_arb() -> bool {
    let addrs = [
        0x4000_0000u32, 0x4000_0FFC, 0x4001_0000, 0x4001_000C,
        0x4002_0000, 0x4002_0008, 0x4003_0000, 0x3FFF_FFFC,
    ];
    let exp = [
        Dev::Reg, Dev::Reg, Dev::Sensor, Dev::Sensor,
        Dev::Switch, Dev::Switch, Dev::Err, Dev::Err,
    ];
    for i in 0..8 {
        if decode(addrs[i]) != exp[i] {
            return false;
        }
    }
    println!("ARB_PASS bus arbitration = address-range decode (8/8 routed)");
    true
}

/// 设备后端 + cache 层（勿改 reg_read_cached；填 *_passthrough）。
struct Bus {
    reg_mem: u32,
    sensor_tick: u32,
    switch_state: u32,
    reg_cache: Option<u32>,
    dev_reads: u32,
}

impl Bus {
    fn reg_read_cached(&mut self) -> u32 {
        match self.reg_cache {
            Some(v) => v, // HIT
            None => {
                self.dev_reads += 1; // MISS → 取设备
                self.reg_cache = Some(self.reg_mem);
                self.reg_mem
            }
        }
    }
    fn sensor_read_passthrough(&mut self) -> u32 {
        // TODO②: sensor 不可缓存，应每次直通取新值：self.sensor_tick += 1; self.sensor_tick
        self.sensor_tick // ← 占位：未推进 tick → 两次读相同 → STALE（缓存了 MMIO 的错误）
    }
    fn switch_write_passthrough(&mut self, v: u32) {
        // TODO②: switchdev 写有副作用，应直通下达：self.switch_state ^= v & 1
        let _ = v; // ← 占位：写被吞 → 副作用没下达 → MISSED_SIDEEFFECT
    }
}

fn level_cache() -> bool {
    let mut bus = Bus {
        reg_mem: 0xA5,
        sensor_tick: 0,
        switch_state: 0,
        reg_cache: None,
        dev_reads: 0,
    };
    let r0 = bus.reg_read_cached(); // miss
    let r1 = bus.reg_read_cached(); // hit
    if r0 != r1 || bus.dev_reads != 1 {
        return false;
    }

    let first = bus.sensor_read_passthrough();
    let stale = first;
    let fresh = bus.sensor_read_passthrough();
    if fresh == stale {
        return false;
    }

    bus.switch_state = 0;
    bus.switch_write_passthrough(1);
    let switched = bus.switch_state == 1;
    if !switched {
        return false;
    }

    println!("CACHE regdev: miss->hit consistent, 1 device read saved (cacheable=memory-like)");
    println!("CACHE sensor: passthrough FRESH={fresh} (cached would be STALE={stale})");
    println!("CACHE switchdev: passthrough SWITCHED state={} (cached would be MISSED_SIDEEFFECT)", bus.switch_state);
    println!("UNCACHED sensor/switchdev bypass cache: MMIO != memory (why volatile/fence)");
    println!("CACHE_PASS");
    true
}

const HANDSHAKE: Duration = Duration::from_millis(200); // 单次总线握手 = 0.2s（§2.3）
const PAYLOAD_N: u32 = 24; // 载荷 24B

fn handshake() {
    sleep(HANDSHAKE);
}

fn level_burst() -> bool {
    // TODO③: 突发分支。逐字节 = 每字节一次握手 → hs_byte 应为 PAYLOAD_N；
    //   突发（MODE=burst 且 ≥3 单位）整块只一次握手 → hs_burst 应为 1。SPEEDUP 须为 24。
    let _ = PAYLOAD_N;
    let hs_byte = 1u32; // ← 占位：应为 PAYLOAD_N（逐字节 24 次握手）
    let hs_burst = 1u32; // ← 占位：突发分支正确即为 1

    let t = Instant::now();
    for _ in 0..hs_byte {
        handshake();
    }
    let byte_meas = t.elapsed().as_secs_f64();

    let t = Instant::now();
    for _ in 0..hs_burst {
        handshake();
    }
    let burst_meas = t.elapsed().as_secs_f64();

    let byte_t = hs_byte as f64 * 0.2;
    let burst_t = hs_burst as f64 * 0.2;
    let speedup = hs_byte / hs_burst;

    if byte_meas < 4.0 || burst_meas > 1.0 || byte_meas < burst_meas * 5.0 {
        return false;
    }
    if speedup != 24 {
        return false;
    }

    println!("BURST byte-by-byte={hs_byte} handshakes  burst={hs_burst} handshake (>=3 units)");
    println!("BYTE_T={byte_t:.1} BURST_T={burst_t:.1} SPEEDUP={speedup}");
    println!("BURST_PASS");
    true
}

fn main() {
    let mut ok = true;
    ok &= level_arb();
    ok &= level_cache();
    ok &= level_burst();
    if ok {
        println!("ALL_PASS");
    } else {
        println!("SOME_FAIL");
        std::process::exit(1);
    }
}
