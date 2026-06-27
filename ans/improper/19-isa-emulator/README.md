# 19 · ISA 模拟器：取指→译码→执行循环 + DiffTest 差分对拍

> 不正经赛道 · 第 19 课 —— 纯软件模型，host 直接跑（rust / c 双语言）。
> 一句话母题：**CPU 不过是一个「取指→译码→执行」的循环；模拟器（NEMU/QEMU/Spike）
> 就是用软件跑这个循环。** 再教一招工业级验证法——**DiffTest 差分对拍**：把你的模型
> 与一个可信「参考」逐指令比对，第一处寄存器/PC 不一致就抓出来（NEMU 正是这样找 CPU bug）。

## 0. 这节课在讲什么

你将亲手写一颗**软件 RV64 CPU**，看清「计算机执行程序」这件事最朴素的真相：

```
loop {
    inst = fetch(pc);     // 取指：从 pc 处读 4 字节机器码
    d    = decode(inst);  // 译码：拆 opcode/funct/rd/rs1/rs2 + 立即数(符号扩展)
    execute(d);           // 执行：更新 regs[32] / 内存 / pc
}
```

模型 = `u64 regs[32]`（`x0` 恒 0）+ `pc` + 一块字节内存。内嵌一段机器码小程序
（求 `1+2+...+10 = 55`，再调子程序翻倍成 `110`），跑出结果即验证你的循环对不对。

然后是本课的重头戏 **DiffTest**：给定一份**黄金参考**——每步执行后期望的
`(pc, 关键寄存器)` 轨迹（由独立可信的参考实现预先算出，当 oracle）。你的模型每执行一条
就与黄金对拍，**首处不一致**就打印「第 k 条指令 regX: ref=.. dut=..」。这就是 NEMU
对着 QEMU/Spike 抓 CPU bug 的方法论：把「最终结果错了」变成「从哪条指令开始错」。

## 1. 内嵌的小程序（RV64I）

| 地址 | 指令 | 含义 |
| :-- | :-- | :-- |
| 0x00 | `addi x5,x0,0` | sum = 0 |
| 0x04 | `addi x6,x0,1` | i = 1 |
| 0x08 | `addi x7,x0,11` | 上界（i 到 11 退出） |
| 0x0c | `lui x8,1` | 数据区基址 = 0x1000 |
| 0x10 | `beq x6,x7,+16` | 循环：i==11 则跳出 |
| 0x14 | `add x5,x5,x6` | sum += i |
| 0x18 | `addi x6,x6,1` | i += 1 |
| 0x1c | `jal x0,-12` | 跳回 0x10 |
| 0x20 | `sd x5,0(x8)` | mem[0x1000] = sum |
| 0x24 | `ld x9,0(x8)` | x9 = mem[0x1000] |
| 0x28 | `sub x12,x7,x6` | 11 - 11 = 0 |
| 0x2c | `jal x1,+16` | 调子程序 dbl，ra=0x30 |
| 0x30 | `bne x13,x0,+8` | 翻倍非零则跳停机 |
| 0x34 | `addi x14,x0,777` | 被跳过（x14 应保持 0） |
| 0x38 | `0x00000000` | HALT（取到全 0 字即停机） |
| 0x3c | `add x13,x5,x5` | dbl：x13 = sum*2 = 110 |
| 0x40 | `jalr x0,0(x1)` | 返回 0x30 |

支持的 RV64I：`addi/add/sub/lui/ld/sd/beq/bne/jal/jalr`。

## 2. 你要填的两处 `// TODO`

软件在 `sw/rust/src/main.rs` 或 `sw/c/isa_emu.c`。取指框架、`decode()`（已拆好所有字段）、
内存、黄金参考、机器码、harness 都已给定，**只动两处**：

| TODO | 位置 | 要做什么 | 判据 |
| :-- | :-- | :-- | :-- |
| ① 执行 | `execute` 的 `OP`/`BRANCH` 分支 | R 型 `add`(funct7=0x00)/`sub`(0x20)；`beq`(funct3=0)/`bne`(1) 的目标 `pc+imm_b` | `EXEC_PASS` |
| ② 对拍 | `check_difftest` | 逐步比较 `dut[k]` 与 `GOLDEN[k]` 每列，找第一处不等 | `DIFFTEST_PASS` |

三项判据：`DECODE_PASS`（译码字段正确，框架已给）、`EXEC_PASS`（跑出结果正确）、
`DIFFTEST_PASS`（全程与黄金一致）。三者全过再打印 `ALL_PASS`。

失败诊断不含 `FAIL`：用 `DECODE_BAD` / `EXEC_BAD` / `DIFF_BAD`（如
`DIFF_BAD 第 6 条指令 x5: ref=1 dut=0` —— 你的 `add` 没把和写回去）。

```
labctl run improper/19-isa-emulator   # 跑 rust/c 两条路径
labctl watch                          # 边改边自动判定
labctl hint improper/19-isa-emulator  # 卡住看提示
```

## 3. 关键约定（判题用）

- **取指**：定长 4 字节小端；取到 `0x00000000`（全 0 字）视为停机。设 `STEP_CAP=10000`
  防跑飞——分支没跳通也不会死循环，只会撞上限后 `EXEC_BAD`。
- **x0 恒 0**：任何写 `rd` 都经 `wreg`，`rd==0` 不写；`step()` 末尾再兜底清零。
- **分支/跳转目标是相对 `pc` 的有符号偏移**：`beq/bne/jal` 目标 = `pc + imm`（不是 `pc+4`）；
  `jalr` 目标 = `(rs1 + imm_i) & ~1`。立即数 `imm_*` 已在 `decode()` 里符号扩展好。
- **黄金参考**：`GOLDEN[52][7]`，每步快照 `[pc, x5, x6, x7, x9, x12, x13]`，由独立可信参考
  预先算出。DUT 跑出的 `dut` 轨迹必须逐步逐列与之相等。
- **DiffTest 的精神**：每条指令后对拍，第一处分歧即第一条错指令——别等最终值才比对。

## 4. 完成标准 (DoD)

- [ ] 至少一条变体跑出 `DECODE_PASS`/`EXEC_PASS`/`DIFFTEST_PASS`/`ALL_PASS`，无 `*_BAD`（必修）。
- [ ] rust 与 c 两路对同一机器码、同一黄金参考逐步一致（跨语言都过计辅助分）。
- [ ] 能口述「取指→译码→执行」三步，以及 ISA 为何是软硬件契约。
- [ ] 能解释 DiffTest 为何强：可信 oracle + 抓第一处分歧。
- [ ] essay 子题答出三步循环、ISA 契约、DiffTest、解释器/二进制翻译/真硬件/跨架构模拟器的关系。

## 5. 思考题（`essay/THINKING.md` 作答即可通过）

1. 用一句话概括 CPU 的本质，并解释「取指→译码→执行」三步各做什么。
2. 为什么说 ISA 是「软硬件之间的契约」？编码里的 `opcode/funct/imm` 字段体现了什么？
3. DiffTest 为什么强？它如何把「最终错了」定位到「从哪条指令开始错」？（可信 oracle + 第一处分歧）
4. 解释器 vs 二进制翻译（对照 `proper/S18-tcg`）vs 真硬件 vs 跨架构模拟器（QEMU），四者关系？
5. 引申：这颗玩具 CPU 还能怎么长大？（加指令/CSR/异常、接真 Spike/QEMU 作参考、加 watchpoint）
