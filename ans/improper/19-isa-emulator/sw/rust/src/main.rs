//! ISA 模拟器：CPU 不过是一个「取指 → 译码 → 执行」的循环 —— Rust 参考解。
//!
//! 母题：一颗 CPU 的本质，就是反复地
//!   1) 取指 fetch：从 pc 处读 4 字节机器码；
//!   2) 译码 decode：拆出 opcode/funct3/funct7、rd/rs1/rs2、立即数(符号扩展)；
//!   3) 执行 execute：更新寄存器 / 内存 / pc。
//! NEMU、QEMU、Spike 这些模拟器，就是用软件跑这个循环。
//!
//! 本课实现一颗**软件 RV64 CPU**：u64 regs[32](x0 恒 0) + pc + 一小块字节内存，
//! 支持够跑一个小程序的若干 RV64I：addi/add/sub/lui/ld/sd/beq/bne/jal/jalr。
//! 内嵌的小程序求 1+2+...+10 = 55，再调一个子程序把它翻倍成 110。
//!
//! 三个判据：
//!   DECODE_PASS   —— 译码字段(opcode/imm 等)正确。
//!   EXEC_PASS     —— 跑完小程序，寄存器/内存结果正确。
//!   DIFFTEST_PASS —— 工业级验证法「差分对拍」：把本模型(DUT)与一份可信「黄金参考」
//!                    轨迹逐指令比对，第一处寄存器/PC 不一致就抓出来（NEMU 抓 CPU bug 之道）。
//!
//! 你只需填两处 // TODO：① add/sub/beq/bne 的译码语义与执行；② difftest 逐步对拍。
//! 其余(取指框架、内存、黄金参考、机器码、harness)均已给定，勿改。
#![allow(dead_code)]

// ════════════════════════════════════════════════════════════════
// 内嵌机器码小程序（RV64I，小端）。地址 = 下标*4。
//   0x00: addi x5,x0,0        sum = 0
//   0x04: addi x6,x0,1        i   = 1
//   0x08: addi x7,x0,11       n1  = 11        (i 跑到 11 即退出)
//   0x0c: lui  x8,1           base= 0x1000    (数据区基址)
//   0x10: beq  x6,x7,+16      LOOP: if i==11 goto 0x20
//   0x14: add  x5,x5,x6       sum += i
//   0x18: addi x6,x6,1        i  += 1
//   0x1c: jal  x0,-12         goto 0x10
//   0x20: sd   x5,0(x8)       mem[0x1000] = sum
//   0x24: ld   x9,0(x8)       x9 = mem[0x1000]
//   0x28: sub  x12,x7,x6      x12 = 11 - 11 = 0
//   0x2c: jal  x1,+16         call dbl(0x3c), ra=0x30
//   0x30: bne  x13,x0,+8      if x13!=0 goto 0x38 (halt)
//   0x34: addi x14,x0,777     —— 被跳过，x14 应保持 0
//   0x38: 0x00000000          HALT（取到全 0 字即停机）
//   0x3c: add  x13,x5,x5      dbl: x13 = sum*2 = 110
//   0x40: jalr x0,0(x1)       return 到 0x30
// ════════════════════════════════════════════════════════════════
const PROG: [u32; 17] = [
    0x00000293, 0x00100313, 0x00b00393, 0x00001437, 0x00730863, 0x006282b3, 0x00130313, 0xff5ff06f,
    0x00543023, 0x00043483, 0x40638633, 0x010000ef, 0x00069463, 0x30900713, 0x00000000, 0x005286b3,
    0x00008067,
];

const MEM_SIZE: usize = 0x2000;
const STEP_CAP: usize = 10_000; // 防跑飞：模型不收敛也不会死循环（NEMU 也有指令上限）

// 差分对拍跟踪的寄存器（其余为 0 不变，跟它们足以抓住所有指令的错）。
// 快照布局：col0 = pc，col1..=6 = x5,x6,x7,x9,x12,x13。
const TRACK: [usize; 6] = [5, 6, 7, 9, 12, 13];
const SNAP: usize = 1 + 6;

// ════════════════════════════════════════════════════════════════
// 译码结果：一条指令拆出的全部字段（取指框架给定）。
// ════════════════════════════════════════════════════════════════
#[derive(Clone, Copy, Default)]
struct Decoded {
    opcode: u32,
    rd: usize,
    funct3: u32,
    rs1: usize,
    rs2: usize,
    funct7: u32,
    imm_i: i64, // I 型立即数（符号扩展）
    imm_s: i64, // S 型（store）
    imm_b: i64, // B 型（branch，偏移）
    imm_u: i64, // U 型（lui，已 <<12）
    imm_j: i64, // J 型（jal 偏移）
}

/// 符号扩展：把 `bits` 的低 `n` 位当成有符号数扩展到 i64。
fn sext(bits: u32, n: u32) -> i64 {
    let shift = 32 - n;
    ((bits << shift) as i32 >> shift) as i64
}

/// 译码：把 32-bit 机器码拆成字段 + 各型立即数（给定，勿改）。
fn decode(inst: u32) -> Decoded {
    Decoded {
        opcode: inst & 0x7f,
        rd: ((inst >> 7) & 0x1f) as usize,
        funct3: (inst >> 12) & 0x7,
        rs1: ((inst >> 15) & 0x1f) as usize,
        rs2: ((inst >> 20) & 0x1f) as usize,
        funct7: (inst >> 25) & 0x7f,
        imm_i: sext((inst >> 20) & 0xFFF, 12),
        imm_s: sext((((inst >> 25) & 0x7f) << 5) | ((inst >> 7) & 0x1f), 12),
        imm_b: sext(
            (((inst >> 31) & 1) << 12)
                | (((inst >> 7) & 1) << 11)
                | (((inst >> 25) & 0x3f) << 5)
                | (((inst >> 8) & 0xf) << 1),
            13,
        ),
        imm_u: sext((inst >> 12) & 0xFFFFF, 20) << 12,
        imm_j: sext(
            (((inst >> 31) & 1) << 20)
                | (((inst >> 12) & 0xff) << 12)
                | (((inst >> 20) & 1) << 11)
                | (((inst >> 21) & 0x3ff) << 1),
            21,
        ),
    }
}

// ════════════════════════════════════════════════════════════════
// 软件 RV64 CPU。
// ════════════════════════════════════════════════════════════════
struct Cpu {
    regs: [u64; 32],
    pc: u64,
    mem: Vec<u8>,
}

impl Cpu {
    fn new() -> Self {
        let mut mem = vec![0u8; MEM_SIZE];
        for (i, w) in PROG.iter().enumerate() {
            mem[i * 4..i * 4 + 4].copy_from_slice(&w.to_le_bytes());
        }
        Cpu { regs: [0; 32], pc: 0, mem }
    }

    /// 取指：从 pc 处读小端 4 字节。
    fn fetch(&self) -> u32 {
        let p = self.pc as usize;
        u32::from_le_bytes([self.mem[p], self.mem[p + 1], self.mem[p + 2], self.mem[p + 3]])
    }

    fn load8(&self, addr: u64) -> u64 {
        let a = addr as usize;
        let mut v = [0u8; 8];
        v.copy_from_slice(&self.mem[a..a + 8]);
        u64::from_le_bytes(v)
    }

    fn store8(&mut self, addr: u64, val: u64) {
        let a = addr as usize;
        self.mem[a..a + 8].copy_from_slice(&val.to_le_bytes());
    }

    /// 写寄存器：x0 永远是 0（硬连线零）。
    fn wreg(&mut self, rd: usize, val: u64) {
        if rd != 0 {
            self.regs[rd] = val;
        }
    }

    /// 执行一条指令：取指 → 译码 → 执行，更新 regs/mem/pc。返回是否停机。
    fn step(&mut self) -> bool {
        let inst = self.fetch();
        if inst == 0 {
            return true; // 取到全 0 字 → 停机
        }
        let d = decode(inst);
        let mut npc = self.pc.wrapping_add(4); // 缺省顺序流；分支/跳转会改写

        match d.opcode {
            // OP-IMM：addi（给定）
            0x13 if d.funct3 == 0 => {
                self.wreg(d.rd, self.regs[d.rs1].wrapping_add(d.imm_i as u64));
            }
            // OP：R 型 add / sub
            0x33 if d.funct3 == 0 => {
                // ───────────────── TODO ① (R 型) ─────────────────
                // 用 funct7 区分：0x00=add（rs1+rs2），0x20=sub（rs1-rs2），写入 rd。
                // 提示：self.regs[d.rs1].wrapping_add / wrapping_sub(self.regs[d.rs2])
                match d.funct7 {
                    0x00 => self.wreg(d.rd, self.regs[d.rs1].wrapping_add(self.regs[d.rs2])),
                    0x20 => self.wreg(d.rd, self.regs[d.rs1].wrapping_sub(self.regs[d.rs2])),
                    _ => {}
                }
                // ──────────────────────────────────────────────────
            }
            // LUI（给定）：imm_u 已经是 <<12 后的有符号值。
            0x37 => self.wreg(d.rd, d.imm_u as u64),
            // LOAD：ld（给定）
            0x03 if d.funct3 == 3 => {
                let addr = self.regs[d.rs1].wrapping_add(d.imm_i as u64);
                let v = self.load8(addr);
                self.wreg(d.rd, v);
            }
            // STORE：sd（给定）
            0x23 if d.funct3 == 3 => {
                let addr = self.regs[d.rs1].wrapping_add(d.imm_s as u64);
                self.store8(addr, self.regs[d.rs2]);
            }
            // BRANCH：beq / bne
            0x63 => {
                // ───────────────── TODO ① (分支) ─────────────────
                // funct3==0 是 beq（相等则跳），funct3==1 是 bne（不等则跳）。
                // 跳转目标 = pc + imm_b（注意是相对 pc 的有符号偏移，不是 pc+4）。
                // 条件成立时把 npc 改成目标；不成立则保持顺序流 pc+4。
                let take = match d.funct3 {
                    0 => self.regs[d.rs1] == self.regs[d.rs2], // beq
                    1 => self.regs[d.rs1] != self.regs[d.rs2], // bne
                    _ => false,
                };
                if take {
                    npc = self.pc.wrapping_add(d.imm_b as u64);
                }
                // ──────────────────────────────────────────────────
            }
            // JAL（给定）：rd = 返回地址(pc+4)；pc += imm_j。
            0x6f => {
                self.wreg(d.rd, self.pc.wrapping_add(4));
                npc = self.pc.wrapping_add(d.imm_j as u64);
            }
            // JALR（给定）：rd = pc+4；pc = (rs1+imm_i) & ~1。
            0x67 => {
                let t = self.pc.wrapping_add(4);
                npc = self.regs[d.rs1].wrapping_add(d.imm_i as u64) & !1u64;
                self.wreg(d.rd, t);
            }
            _ => {} // 本课不实现的指令：当 nop（真模拟器会报非法指令异常）
        }

        self.regs[0] = 0; // 兜底：x0 恒 0
        self.pc = npc;
        false
    }

    fn snapshot(&self) -> [u64; SNAP] {
        let mut s = [0u64; SNAP];
        s[0] = self.pc;
        for (i, &r) in TRACK.iter().enumerate() {
            s[i + 1] = self.regs[r];
        }
        s
    }

    /// 跑到停机（或撞上限），返回每步执行后的快照轨迹。
    fn run_trace(&mut self) -> Vec<[u64; SNAP]> {
        let mut tr = Vec::new();
        for _ in 0..STEP_CAP {
            if self.step() {
                break;
            }
            tr.push(self.snapshot());
        }
        tr
    }
}

// ════════════════════════════════════════════════════════════════
// 黄金参考轨迹（GIVEN）：由一份独立、可信的高层参考实现预先算出，
// 每步执行后的 [pc, x5, x6, x7, x9, x12, x13]。DUT 必须逐步与之一致。
// —— 这正是 DiffTest 的灵魂：拿可信 oracle 当裁判，抓「第一处分歧」。
// ════════════════════════════════════════════════════════════════
const GOLDEN: [[u64; SNAP]; 52] = [
    [0x4, 0, 0, 0, 0, 0, 0],
    [0x8, 0, 1, 0, 0, 0, 0],
    [0xc, 0, 1, 11, 0, 0, 0],
    [0x10, 0, 1, 11, 0, 0, 0],
    [0x14, 0, 1, 11, 0, 0, 0],
    [0x18, 1, 1, 11, 0, 0, 0],
    [0x1c, 1, 2, 11, 0, 0, 0],
    [0x10, 1, 2, 11, 0, 0, 0],
    [0x14, 1, 2, 11, 0, 0, 0],
    [0x18, 3, 2, 11, 0, 0, 0],
    [0x1c, 3, 3, 11, 0, 0, 0],
    [0x10, 3, 3, 11, 0, 0, 0],
    [0x14, 3, 3, 11, 0, 0, 0],
    [0x18, 6, 3, 11, 0, 0, 0],
    [0x1c, 6, 4, 11, 0, 0, 0],
    [0x10, 6, 4, 11, 0, 0, 0],
    [0x14, 6, 4, 11, 0, 0, 0],
    [0x18, 10, 4, 11, 0, 0, 0],
    [0x1c, 10, 5, 11, 0, 0, 0],
    [0x10, 10, 5, 11, 0, 0, 0],
    [0x14, 10, 5, 11, 0, 0, 0],
    [0x18, 15, 5, 11, 0, 0, 0],
    [0x1c, 15, 6, 11, 0, 0, 0],
    [0x10, 15, 6, 11, 0, 0, 0],
    [0x14, 15, 6, 11, 0, 0, 0],
    [0x18, 21, 6, 11, 0, 0, 0],
    [0x1c, 21, 7, 11, 0, 0, 0],
    [0x10, 21, 7, 11, 0, 0, 0],
    [0x14, 21, 7, 11, 0, 0, 0],
    [0x18, 28, 7, 11, 0, 0, 0],
    [0x1c, 28, 8, 11, 0, 0, 0],
    [0x10, 28, 8, 11, 0, 0, 0],
    [0x14, 28, 8, 11, 0, 0, 0],
    [0x18, 36, 8, 11, 0, 0, 0],
    [0x1c, 36, 9, 11, 0, 0, 0],
    [0x10, 36, 9, 11, 0, 0, 0],
    [0x14, 36, 9, 11, 0, 0, 0],
    [0x18, 45, 9, 11, 0, 0, 0],
    [0x1c, 45, 10, 11, 0, 0, 0],
    [0x10, 45, 10, 11, 0, 0, 0],
    [0x14, 45, 10, 11, 0, 0, 0],
    [0x18, 55, 10, 11, 0, 0, 0],
    [0x1c, 55, 11, 11, 0, 0, 0],
    [0x10, 55, 11, 11, 0, 0, 0],
    [0x20, 55, 11, 11, 0, 0, 0],
    [0x24, 55, 11, 11, 0, 0, 0],
    [0x28, 55, 11, 11, 55, 0, 0],
    [0x2c, 55, 11, 11, 55, 0, 0],
    [0x3c, 55, 11, 11, 55, 0, 0],
    [0x40, 55, 11, 11, 55, 0, 110],
    [0x30, 55, 11, 11, 55, 0, 110],
    [0x38, 55, 11, 11, 55, 0, 110],
];

/// 快照某一列的人类可读名字。
fn col_name(c: usize) -> String {
    if c == 0 {
        "pc".to_string()
    } else {
        format!("x{}", TRACK[c - 1])
    }
}

// ════════════════════════════════════════════════════════════════
// 测试 harness（给定，勿改）
// ════════════════════════════════════════════════════════════════

/// 子题 1：译码字段正确。
fn check_decode() -> bool {
    let mut ok = true;

    // addi x7,x0,11
    let d = decode(0x00b00393);
    if d.opcode != 0x13 || d.rd != 7 || d.rs1 != 0 || d.funct3 != 0 || d.imm_i != 11 {
        println!("DECODE_BAD addi 字段错: op=0x{:x} rd={} rs1={} imm_i={}", d.opcode, d.rd, d.rs1, d.imm_i);
        ok = false;
    }
    // lui x8,1 → imm_u 应为 0x1000
    let d = decode(0x00001437);
    if d.opcode != 0x37 || d.rd != 8 || d.imm_u != 0x1000 {
        println!("DECODE_BAD lui 字段错: op=0x{:x} rd={} imm_u=0x{:x} 应=(0x37,8,0x1000)", d.opcode, d.rd, d.imm_u);
        ok = false;
    }
    // sub x12,x7,x6 → funct7=0x20
    let d = decode(0x40638633);
    if d.opcode != 0x33 || d.funct7 != 0x20 || d.funct3 != 0 || d.rd != 12 || d.rs1 != 7 || d.rs2 != 6 {
        println!("DECODE_BAD sub 字段错: op=0x{:x} f7=0x{:x} rd={} rs1={} rs2={}", d.opcode, d.funct7, d.rd, d.rs1, d.rs2);
        ok = false;
    }
    // beq x6,x7,+16 → imm_b=16
    let d = decode(0x00730863);
    if d.opcode != 0x63 || d.funct3 != 0 || d.rs1 != 6 || d.rs2 != 7 || d.imm_b != 16 {
        println!("DECODE_BAD beq 字段错: op=0x{:x} rs1={} rs2={} imm_b={} 应 imm_b=16", d.opcode, d.rs1, d.rs2, d.imm_b);
        ok = false;
    }
    // jal x1,+16 → imm_j=16
    let d = decode(0x010000ef);
    if d.opcode != 0x6f || d.rd != 1 || d.imm_j != 16 {
        println!("DECODE_BAD jal 字段错: op=0x{:x} rd={} imm_j={} 应=(0x6f,1,16)", d.opcode, d.rd, d.imm_j);
        ok = false;
    }
    // jal x0,-12 → imm_j 应为负
    let d = decode(0xff5ff06f);
    if d.imm_j != -12 {
        println!("DECODE_BAD jal 负偏移错: imm_j={} 应=-12（符号扩展没做对？）", d.imm_j);
        ok = false;
    }
    // sd x5,0(x8) → S 型 imm=0, rs1=8, rs2=5
    let d = decode(0x00543023);
    if d.opcode != 0x23 || d.rs1 != 8 || d.rs2 != 5 || d.imm_s != 0 {
        println!("DECODE_BAD sd 字段错: op=0x{:x} rs1={} rs2={} imm_s={}", d.opcode, d.rs1, d.rs2, d.imm_s);
        ok = false;
    }

    if ok {
        println!("DECODE_PASS");
    }
    ok
}

/// 子题 2：跑完小程序，结果正确。
fn check_exec() -> bool {
    let mut ok = true;
    let mut cpu = Cpu::new();
    let mut steps = 0;
    let mut halted = false;
    for _ in 0..STEP_CAP {
        if cpu.step() {
            halted = true;
            break;
        }
        steps += 1;
    }

    if !halted {
        println!("EXEC_BAD 跑了 {} 步还没停机（撞上限，可能分支没跳/陷入死循环）", steps);
        ok = false;
    }
    let want: [(usize, u64); 7] =
        [(5, 55), (6, 11), (7, 11), (9, 55), (12, 0), (13, 110), (14, 0)];
    for (r, v) in want {
        if cpu.regs[r] != v {
            println!("EXEC_BAD x{} = {} 应 = {}", r, cpu.regs[r], v);
            ok = false;
        }
    }
    let m = cpu.load8(0x1000);
    if m != 55 {
        println!("EXEC_BAD mem[0x1000] = {} 应 = 55", m);
        ok = false;
    }
    if cpu.pc != 0x38 {
        println!("EXEC_BAD 停机 pc = 0x{:x} 应 = 0x38", cpu.pc);
        ok = false;
    }
    if cpu.regs[0] != 0 {
        println!("EXEC_BAD x0 被写脏 = {}（x0 必须恒 0）", cpu.regs[0]);
        ok = false;
    }

    if ok {
        println!("EXEC_PASS 1+2+...+10 = {}（再翻倍 = {}）", cpu.regs[5], cpu.regs[13]);
    }
    ok
}

/// 子题 3：DiffTest —— 把 DUT 轨迹与黄金参考逐步对拍，抓第一处分歧。
fn check_difftest() -> bool {
    let mut cpu = Cpu::new();
    let dut = cpu.run_trace();

    // 长度先对齐（差太多说明跑飞了）。
    if dut.len() != GOLDEN.len() {
        println!("DIFF_BAD 轨迹步数 dut={} ref={}（执行流就没对上）", dut.len(), GOLDEN.len());
        return false;
    }

    // ───────────────────── TODO ② 逐步对拍 ─────────────────────
    // 从第 0 步起，逐步比较 dut[k] 与 GOLDEN[k] 的每一列（col0=pc，其余=寄存器）。
    // 找到「第一处」不相等的 (k, c)：打印
    //   "DIFF_BAD 第 {k+1} 条指令 {列名}: ref={..} dut={..}"
    // 然后 return false。若全程一致，跳出循环打印 DIFFTEST_PASS 并 return true。
    // 列名用 col_name(c)。这就是 NEMU 抓 CPU bug 的方式：第一处分歧 = 第一条错指令。
    for k in 0..GOLDEN.len() {
        for c in 0..SNAP {
            if dut[k][c] != GOLDEN[k][c] {
                println!(
                    "DIFF_BAD 第 {} 条指令 {}: ref={} dut={}",
                    k + 1,
                    col_name(c),
                    GOLDEN[k][c],
                    dut[k][c]
                );
                return false;
            }
        }
    }
    // ────────────────────────────────────────────────────────────

    println!("DIFFTEST_PASS 全 {} 步逐指令与黄金参考一致", GOLDEN.len());
    true
}

fn main() {
    let mut all = true;
    all &= check_decode();
    all &= check_exec();
    all &= check_difftest();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
