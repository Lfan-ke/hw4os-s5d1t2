//! 设备文件「一切皆文件」—— Rust 参考解。
//! 母题：设备文件 = 给硬件的副作用穿上 read/write 的外衣。
//! 两个「不存数据的文件」：
//!   ① ConstDev   —— read 恒 1、write 恒 0（数据石沉大海，无存储）。
//!   ② RingSumDev —— 深度 2 的环形寄存器；write 推入/233 复位，read 求和。
//! 学生只实现两个 FileLike 的方法体；下方测试 harness（向量 + PASS 打印）勿改。

/// 「一切皆文件」= 一切皆 read/write 接口。对应 rcore 的 `File` trait、
/// xv6 的 `struct devsw{read,write}`、Linux 的 `file_operations`。
trait FileLike {
    /// 读出一个字（设备语义自定：常量源恒 1 / 环形设备返回求和）。
    fn read(&mut self) -> u32;
    /// 写入一个字，返回「写了几个」（常量空洞恒 0 / 环形设备恒 0）。
    fn write(&mut self, x: u32) -> u32;
}

// ── 子实验 1：常量设备（read 恒 1 / write 恒 0）──────────────────────
// 这里用 `// ELSE[b]` 合成一个 ConstDev 同时实现读写两面；
// `// TODO[a]` 也可拆成 OneSource(只读恒 1) + NullSink(只写吞掉) 两个对象。
struct ConstDev;
impl FileLike for ConstDev {
    fn read(&mut self) -> u32 {
        1
    }
    fn write(&mut self, _x: u32) -> u32 {
        0
    }
}

// ── 子实验 2：RingSum 有副作用的文件 ────────────────────────────────
// 深度 2 环形寄存器：write(x)=x==233 复位，否则移位 r1<=r0;r0<=x；read()=r0+r1。
struct RingSumDev {
    r0: u32,
    r1: u32,
}
impl FileLike for RingSumDev {
    fn read(&mut self) -> u32 {
        self.r0 + self.r1
    }
    fn write(&mut self, x: u32) -> u32 {
        if x == 233 {
            // 233 当 magic：清空环
            self.r0 = 0;
            self.r1 = 0;
        } else {
            // 移位写法（`// TODO[a]`）：挤掉最旧的 r1，新值入 r0
            self.r1 = self.r0;
            self.r0 = x;
        }
        0
    }
}

// ───────────────────────── 测试 harness（勿改）─────────────────────────

fn check_filelike(d: &mut dyn FileLike) -> bool {
    let mut ok = true;
    for _ in 0..3 {
        let r = d.read();
        if r != 1 {
            println!("FILELIKE_FAIL read 应=1 实得={}", r);
            ok = false;
        }
    }
    for x in [5u32, 700, 233] {
        let w = d.write(x);
        if w != 0 {
            println!("FILELIKE_FAIL write({}) 应=0 实得={}", x, w);
            ok = false;
        }
    }
    if ok {
        println!("FILELIKE_PASS");
    }
    ok
}

fn check_ring(d: &mut dyn FileLike) -> bool {
    // 灌入序列并逐步读：666→666、111→777、222→333、233→0
    let steps = [(666u32, 666u32), (111, 777), (222, 333), (233, 0)];
    let mut ok = true;
    for (w, want) in steps {
        d.write(w);
        let got = d.read();
        if got != want {
            println!("RING_FAIL write {} 后 read={} 应={}", w, got, want);
            ok = false;
        }
    }
    if ok {
        println!("RING_PASS");
    }
    ok
}

fn main() {
    let mut all = true;
    all &= check_filelike(&mut ConstDev);
    all &= check_ring(&mut RingSumDev { r0: 0, r1: 0 });
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
