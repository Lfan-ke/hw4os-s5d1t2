# 16a · 总线与缓存：仲裁=地址译码 / cache / 突发 / 直通

> 不正经赛道 · 第 16a 课 - 软件 host/qemu-user 直接跑；硬件走 iverilog/bsc 仿真。
> 一句话母题：**“总线 / 互连 / 仲裁器”祛魅之后，不过是一张按地址区间译码的查找表。**
> 同一组地址、同一张期望路由表，在四种语言里跑出**完全相同**的 `ARB_PASS` / `ALL_PASS`；
> 软件再往上加一层 `cache` 与“突发摊薄握手”，把“为什么 MMIO 常要直通/volatile”讲透。

## 0. 这节课在讲什么

上一课（`16-driver`）你给一个 MMIO 设备写驱动；这一课退一步问：**地址写出去，凭什么到得了那个设备？**
答案朴素到让人失望 - **仲裁器就是地址区间译码**：拿 `addr` 和几段 `[base, end)` 比一比，命中谁就选谁，
全落空就是总线错误。本课把这件事摊成四层：

- **仲裁（arbitration）= 地址区间译码**：`addr 落区间 → sel`。硬件是 `assign sel = (addr>=BASE)&&(addr<END)`
  的纯组合译码；软件是 `if / else if` 链 + 设备 handler 分发。四变体跑**同一组 8 个地址**、对照**同一张**
  期望路由表，打印逐位一致的 `ARB_PASS`。
- **cache 暂存**：可缓存区（`regdev`，类内存）首读 `miss` 取设备、再读 `hit` 命中缓存，省一次设备访问且读值一致。
- **直通 / uncached（反例）**：`sensor`（TEMP 每 tick 变）与 `switchdev`（写=翻转副作用）**不可缓存** - 
  缓存它们会 `STALE` / `MISSED_SIDEEFFECT`，必须直通。由此，**非存储语义的 MMIO 必须 UNCACHED**，
  这正是 16b/C 里寄存器要 `volatile` + `fence` 的根因。
- **突发摊薄握手**：单次总线握手按 `sleep(0.2s)` 计；逐字节传 24B = 24 次握手 = 4.8s，
  突发一次搬完 = 0.2s，**提速 24×**。C 用 `nanosleep`、Rust 用 `thread::sleep` **真实睡眠**，并用单调时钟测量兜底。

对应真实系统：SoC 的地址译码器 / interconnect（AXI/TileLink）、PCIe 的 BAR 区间、RISC-V 的 PMA
（哪些物理区可缓存）；下一站 proper `S06`/`S06c` 里 NS16550/PLIC 的 `base` 常量，就是这张 §2.2 表的真身。

## 1. toy 总线地址图（§2.2 的标本）

| 区间 | 设备 | 可缓存 | 语义 |
| :-- | :-- | :-- | :-- |
| `0x4000_0000..0x4000_1000` | `regdev`    | √ 缓存 + 突发 | 类内存（被内存背书，无读副作用） |
| `0x4001_0000..0x4001_0010` | `sensor`    | × 直通 | `TEMP` 每 tick 变（读有“时间副作用”） |
| `0x4002_0000..0x4002_0010` | `switchdev` | × 直通 | 写 = 翻转设备状态（写有副作用） |
| else | 总线错误 `BUS_ERR` | - | 区间全落空 |

固定仲裁场景（四语逐位对照用同一组地址 + 同一张期望路由）：

```
0x4000_0000→regdev  0x4000_0FFC→regdev   0x4001_0000→sensor  0x4001_000C→sensor
0x4002_0000→switch  0x4002_0008→switch   0x4003_0000→BUS_ERR 0x3FFF_FFFC→BUS_ERR
```

## 2. 四语对位的“同一台译码器”

| 概念 | Verilog | BSV | C | Rust |
| :-- | :-- | :-- | :-- | :-- |
| 区间判定 | `(addr>=BASE)&&(addr<END)` | 同（`Bit#(32)` 比较） | `a>=BASE && a<END` | `a>=BASE && a<END` |
| 选择结果 | 单热 `sel_*` + `bus_err` / `dev[1:0]` | `enum Dev` | `enum {DEV_REG,…}` | `enum Dev` |
| 设备分发 | tb 当参考驱动 | tb（每 rule 一次 `route`） | handler 函数表 | `Bus` 方法 |
| cache | - （硬件不建模） | - | `reg_cache` + `dev_reads` | `Option<u32>` 缓存 |
| 直通 | - | - | `fence rw,rw` + 每次访设备 | 每次访设备 |
| 突发计时 | - | - | `nanosleep(0.2s)` | `thread::sleep(0.2s)` |

> **为什么硬件只考仲裁**：cache/突发/直通是“软件总线模型 + 时间账”，放在 host 软件里跑最直观；
> 硬件侧只把**仲裁=地址区间译码**这件最核心的事用 RTL 写实（纯组合译码 + 单热校验），与软件**逐位**对上 `ARB_PASS`。

## 3. 分阶段 Pass 与判题 (DoD)

每条软件路径都打印共同里程碑，再各自打印附加台阶：

- `sw-c` / `sw-rust`：`ARB_PASS`（8/8 路由一致）· `CACHE_PASS`（regdev miss→hit 一致；sensor `FRESH`、
  switchdev `SWITCHED`，并标注缓存它们会 `STALE`/`MISSED_SIDEEFFECT`，结论 `UNCACHED`）·
  `BURST_PASS`（`BYTE_T=4.8 BURST_T=0.2 SPEEDUP=24`）· `ALL_PASS`。
- `hw-v` / `hw-bsv`：`ARB_PASS`（同一组地址、同一期望路由）· `DEV_PASS`（恰好单热选择 / 越界 `bus_err`）·
  `ALL_PASS`，且 **0 warning**。

统一 `expect = ["ARB_PASS","ALL_PASS"]`，`forbid = ["FAIL","panic","ERROR"]`。

```
labctl run improper/16a-bus-cache   # 跑 sw-c/sw-rust/hw-v/hw-bsv/essay
labctl hint improper/16a-bus-cache  # 卡住看分级提示
```

- [ ] ① 仲裁=地址区间译码：8 个地址对照期望路由全中 → `ARB_PASS`（四变体逐位一致）。
- [ ] ② cache：可缓存区 miss→hit 一致、省一次设备访问 → `CACHE_PASS` 的第一段。
- [ ] ③ 直通：`sensor` 取到 `FRESH`（缓存=`STALE`）、`switchdev` 写出 `SWITCHED`（缓存=`MISSED_SIDEEFFECT`）→ `UNCACHED` 结论。
- [ ] ④ 突发：逐字节 24×0.2=4.8s vs 突发 1×0.2=0.2s，真实睡眠测得 → `BURST_PASS`、`SPEEDUP=24`。
- [ ] 硬件仲裁器区间译码 + 单热/越界校验 → `ARB_PASS`/`DEV_PASS`（0 warning）。
- [ ] 软件任一语言全过（必修）；另一语言、两条硬件路径多过计辅助分。
- [ ] `essay/THINKING.md`：cache 仲裁、突发账、`sensor`/`switchdev` 直通两反例（`STALE`/`MISSED_SIDEEFFECT`/`UNCACHED`/`SPEEDUP`）。

## 4. 关键决策（本实验定）

1. **硬件侧只写实“仲裁=地址区间译码”，cache/突发/直通归软件。** 仲裁是“总线”里最能 RTL 化的内核
   （纯组合译码 + 单热互斥），四语都能逐位对上 `ARB_PASS`；而 cache 一致性、突发握手摊薄、可缓存性反例
   本质是**软件总线模型 + 时间账**，在 host/qemu-user 里跑最直观，故只在 `sw-c`/`sw-rust` 展开。
2. **`sw-c` 走 `gcc-rv64`（qemu-user）以拿到真 `fence`。** 直通设备访问用 `fence rw,rw` 包裹 - 
   这恰是“为什么 MMIO 寄存器要 `volatile`/不可缓存”的硬件根因（与 16b 的 `lw/sw/fence`、`volatile` 显式回链）。
   `nanosleep` 在 qemu-user 下透传到宿主，4.8s/0.2s 是**真实墙钟**。
3. **突发账打印**用按握手次数推导的**模型值**（`BYTE_T=4.8 BURST_T=0.2 SPEEDUP=24`，C/Rust 逐位一致），
   另用单调时钟**测量兜底**断言握手确实睡过（逐字节测得 ≥4.0s、突发 ≤1.0s），避免抖动影响里程碑串。

## 5. 简化取舍（留完整版作引申）

- 仲裁器只做**地址区间译码 + 单热互斥**，不建模总线时序握手 / 反压 / 多主仲裁优先级（那些在 16d-plic 的优先级仲裁里）。
- cache 是“一行直接映射 + 写直达”的最小模型：只为讲清 miss/hit 与“可缓存 vs 直通”的语义差，不建模替换策略/一致性协议。
- 突发用固定 `0.2s` 握手 + 24B 载荷凑 `SPEEDUP=24`；真实总线的突发长度/对齐/传输周期此处忽略。
- `sensor`/`switchdev` 的“副作用”用最小剧本（tick 自增 / state 翻转）演示，够说明“非存储语义不可缓存”即止。

## 6. 思考题（`essay/THINKING.md` 作答）

用 §2.2 地址图 + §2.3 时间模型，讲清三件事：(1) 仲裁=地址区间译码 + cache 暂存在“类内存”区为何成立；
(2) 突发账 4.8s vs 0.2s 的 `SPEEDUP`；(3) `sensor`/`switchdev` 两个直通反例（`STALE` / `MISSED_SIDEEFFECT`），
并由此收束“非存储语义 MMIO 必须 `UNCACHED`” - 即 16b 寄存器要 `volatile`/`fence` 的根因。
