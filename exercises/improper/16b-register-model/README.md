# 16b · 寄存器模型：抄布局四语同构

> 不正经赛道 · 第 16b 课 - 软件 host/qemu-user 直接跑；硬件走 iverilog/bsc 仿真。
> **软件驱动里的“寄存器模型”，不过是硬件寄存器布局的一层语法皮。**
> 同一张 `regdev` 寄存器表，在四种语言、由低到高的四种手法里逐位转写同一段读写 trace - 
> 它们打印**完全相同**的 `RAW_PASS` / `MIRROR_PASS` / `ALL_PASS`，证明“抄布局”抄到了逐位一致。

## 0. 这节课在讲什么

上一课你给设备手敲 MMIO；这一课退一步问：**驱动凭什么知道“第 0 位是使能、第 3:2 位是模式”？**
答案是它在**照抄硬件的寄存器布局**。这层“抄布局”分两条线、四个台阶：

- **C = 规矩·自底向上**：`裸 rv 汇编 lw/sw/fence` → `volatile 命名寄存器` → `struct 位域 + union` → `inline-asm 访问宏`。
  一路看清“高层的具名字段访问，编译下去就是 base+off 的 load/store”。
- **Rust = 类型化·层层封装**：`read_volatile` → `bitflags!` → `tock-registers`（`register_bitfields!`+`register_structs!`）→ `bytemuck`。
  一路把“裸字”包成带类型、带字段名、带 RO/WO/RW 语义的寄存器图。
- **Verilog / BSV = 硬件同构**：`regdev` 设备 RTL - 字段就是 `ctrl_r[3:2]` 这样的 wire 切片 / `Bit` 切片，
  正是上面所有软件库**对位的源头**。tb 当参考驱动跑同一段 trace。

对应真实系统：Linux `readl/writel` + `struct ... __iomem`、Tock OS 的 `register_bitfields!`、
zircon/RT-Thread 的寄存器封装；下一站 proper `S06d-regmap` 把同一手法用到真 NS16550 / PLIC。

## 1. `regdev` 寄存器契约（§2.1 的标本）

MMIO base = `0x4000_0000`（toy），小端，u32 对齐。

| 偏移 | 名 | 访问 | 位域 |
| :-- | :-- | :-- | :-- |
| 0x00 | `CTRL`   | RW | `EN`[0] `IE`[1] `MODE`[3:2]（0=off/1=blink/2=solid/3=burst） `RST`[8] |
| 0x04 | `STATUS` | RO | `READY`[0] `BUSY`[1] `IRQ`[2]（设备由 CTRL 推导：READY=EN，IRQ=EN&IE） |
| 0x08 | `DATA`   | WO | `BYTE`[7:0] |
| 0x0c | `ID`     | RO | magic `0x5245_4744`（"REGD"） |

固定 trace（四语逐位对照用同一组值）：
`CTRL=0x0000000B`（EN=1,IE=1,MODE=2/solid）· `STATUS=0x00000005`（READY=1,IRQ=1）· `DATA.BYTE=0xA5`。

## 2. 四语对位的“同一张表”

| 字段 | Verilog（wire 切片） | BSV（Bit 切片） | C（位域 + union） | Rust（tock-registers） |
| :-- | :-- | :-- | :-- | :-- |
| `EN`[0]      | `ctrl_r[0]`     | `ctrl[0]`     | `uint32_t en:1;`        | `EN OFFSET(0) NUMBITS(1)` |
| `IE`[1]      | `ctrl_r[1]`     | `ctrl[1]`     | `uint32_t ie:1;`        | `IE OFFSET(1) NUMBITS(1)` |
| `MODE`[3:2]  | `ctrl_r[3:2]`   | `ctrl[3:2]`   | `uint32_t mode:2;`      | `MODE OFFSET(2) NUMBITS(2) [...]` |
| `RST`[8]     | `ctrl_r[8]`     | `ctrl[8]`     | `... rsv0:4, rst:1;`    | `RST OFFSET(8) NUMBITS(1)` |
| 寄存器 RW/RO/WO | 读多路器 + 写使能 case | `read`/`write` 方法 | `volatile` + 写忽略 | `ReadWrite`/`ReadOnly`/`WriteOnly` |
| 整块 raw↔struct | 字段抽头重建整字 | 字段切片重建整字 | `union{u32 raw; struct b;}` | `bytemuck::cast::<[u8;16],RegFile>` |
| 纯标志寄存器 | - （位即线） | - | - （位域） | `bitflags!`（仅 EN/IE、READY/BUSY/IRQ） |

> **为什么 MODE 不进 `bitflags!`**：`bitflags` 表达的是“一组独立的 1 位标志”；`MODE` 是 2 位**多值枚举**
> （off/blink/solid/burst），要用 `tock-registers` 的 `NUMBITS(2) [ ... ]` 或 C 的 `:2` 位域。
> 纯标志寄存器（`STATUS` 的 READY/BUSY/IRQ、CTRL 的 EN/IE）才是 `bitflags!` 的主场。

## 3. 分阶段 Pass 与判题 (DoD)

每条软件路径按级打印自己的台阶，并都打印共同里程碑：

- `sw-c`：`RAW_PASS`（rv lw/sw/fence）· `VOL_PASS`（volatile 具名）· `STRUCT_PASS`（位域+union）·
  `MACRO_PASS`（readl/writel 宏）· `MIRROR_PASS`（union 逐位镜像）· `ALL_PASS`。
- `sw-rust`：`RAW_PASS`（volatile）· `FLAGS_PASS`（bitflags）· `TOCK_PASS`（tock-registers）·
  `MIRROR_PASS`（bytemuck）· `ALL_PASS`。
- `hw-v` / `hw-bsv`：`RAW_PASS`（读写 trace）· `DEV_PASS`（设备字段响应：RO 拒写 / WO 读 0 / 捕获字节）·
  `MIRROR_PASS`（字段抽头/切片重建）· `ALL_PASS`，且 **0 warning**。

统一 `expect = ["RAW_PASS","MIRROR_PASS","ALL_PASS"]`，`forbid = ["FAIL","panic","ERROR"]`。

```
labctl run improper/16b-register-model   # 跑 sw-c/sw-rust/hw-v/hw-bsv/essay
labctl hint improper/16b-register-model  # 卡住看分级提示
```

- [ ] ①：最底层 load/store 读 ID 比对 magic、按 trace 读写 → `RAW_PASS`。
- [ ] ②③：把布局抄成 `volatile`/`bitflags!`/`tock-registers`（或 C 位域+union）→ 各级 Pass。
- [ ] ④：raw 与 struct 双向重建逐位相等 → `MIRROR_PASS`。
- [ ] 硬件 `regdev` RTL 字段位选 + tb 跑过 → `DEV_PASS`（0 warning）。
- [ ] 软件任一语言全过（必修）；另一语言、两条硬件路径多过计辅助分。
- [ ] `essay/THINKING.md`：把给定的 Chisel `regmap` 抄成 `register_structs!`+`register_bitfields!`。

## 4. 关键决策（本实验定，影响后续模板）

1. **C 的“裸 rv 汇编”台阶走真跑（方案 b），而非只在 essay 展示（方案 a）。**
   improper 默认 `env=host`（x86），真 RISC-V `lw/sw` 跑不了。但本仓库**确有** `riscv64-linux-gnu-gcc`
   + `qemu-riscv64`（labctl 的 `gcc-rv64` 构建器），故 `sw-c` 整程序交叉编成 riscv64 静态 ELF 跑在
   qemu-user - `①` 与 `④` 是**真的** `lw`/`sw`/`fence`。这能成立的另一半原因：16b 考的是**抄布局/逐位一致**，
   `regdev` 是一段**被内存背书的寄存器文件**（无读副作用），用真 load/store 访问它恰好天然。
   （对位：`volatile` 读写 = 编译成 `lw/sw`，`fence` 管 MMIO 的内存序 - 这正是“寄存器为何要 volatile”的根因，
   见 16a 的可缓存性反例。）
2. **`tock-registers` / `bitflags` / `bytemuck` 离线可构建。** 三者及其传递依赖
   （`bytemuck_derive`/`proc-macro2`/`quote`/`syn`/`unicode-ident`）均已在本地 cargo 注册表缓存中，
   `Cargo.lock` 已随附固定版本（bitflags 2.13 / tock-registers 0.9 / bytemuck 1.25），离线即可编。

## 5. 简化取舍（留完整版作引申）

- `regdev` 是被内存背书的寄存器文件：只考布局转写与读写语义（RO 拒写 / WO 读 0 / 捕获字节），
  不做时序握手 / 突发 / 中断（那些在 16a/16c–16e）。
- STATUS 由设备从 CTRL 一拍推导（READY=EN，IRQ=EN&IE）；不建模真实多周期 BUSY 脉冲。
- C 位域的物理布局是实现定义的；本课锁定 riscv64-linux-gnu-gcc（小端、LSB 优先），并用运行期 MIRROR 校验兜底。
- Rust 侧把 `register_structs!` 覆盖在一块栈上缓冲（`&*(ptr as *const RegDev)`），等价真 MMIO 的 base 指针。

## 6. 思考题（`essay/THINKING.md` 作答）

给定一段 Chisel `regmap(RegField...)`，把它**抄成** `register_structs!` + `register_bitfields!`：
逐寄存器判定 RO/WO/RW、逐位写出 `OFFSET/NUMBITS`，并说明 raw↔struct 镜像为何成立。
