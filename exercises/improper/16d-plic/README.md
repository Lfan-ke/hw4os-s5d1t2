# 16d · 核外中断 PLIC：共享外设 IRQ 路由器

> 不正经赛道 · 第 16d 课 - 软件 host/qemu-user 直接跑；硬件走 iverilog/bsc 仿真。
> 一句话母题：**核外中断 = 一台共享的“IRQ 路由器” - 把 N 个外设的中断按 priority/enable/threshold
> 过滤、仲裁，再用 claim/complete 握手交给某个 hart 处理。**
> 同一段场景，在 C / Rust / Verilog / BSV 四语里逐位一致跑出同一条抽干序列 - 
> 它们打印**完全相同**的 `ROUTE_PASS` / `CLAIM_PASS` / `ALL_PASS`。

## 0. 这节课在讲什么

上一课 `16c-clint` 讲**核内中断**（每个 hart 自己的 timer 与软件中断/IPI）；这一课讲**核外中断**：
一堆**共享的外设**（UART、网卡、磁盘、GPIO……）各自有 IRQ 线，但 hart 只有一根外部中断脚。
谁来收编这些 IRQ、决定先处理哪个、避免多核重复处理？答案是 **PLIC**（Platform-Level Interrupt Controller）。

把 PLIC 祛魅成**三件套 + 一次握手**：

- **priority[源]**：每个中断源一个优先级寄存器（`0` = 永久屏蔽）。
- **enable[上下文][源]**：每个“上下文”（= 某 hart 的某特权级）能不能看见某个源。
- **threshold[上下文]**：该上下文的优先级门槛 - 只有 `priority > threshold` 的源才可见。
- **claim / complete**：hart 取中断时**读 CLAIM 寄存器** → 返回当前最高优先级的可见源，并把它的
  pending 清掉（进入 in-service）；处理完**写 CLAIM 寄存器** = `complete`（EOI），让网关放行该源的下次中断。

一个源对某上下文“可见” ⟺ `pending & enable & (priority > threshold)`；仲裁器在所有可见源里
取 **priority 最高者**，priority 相同则取**最小源 id**（这是 PLIC 规范的裁决）。

对应真实系统：RISC-V PLIC（SiFive/QEMU virt 的 `0x0c00_0000`）、Linux `irqchip/irq-sifive-plic.c`、
xv6 的 `plic.c`（`plic_claim`/`plic_complete`）。下一站 proper `S06c-plic` 把同一套用到 qemu-virt 真 PLIC。

## 1. 本课 toy PLIC 寄存器契约

`addr` = 字节偏移；`bit i` = 源 `i`（`i = 1..4`，源 0 表示“无中断”）。

| 偏移 | 名 | 访问 | 含义 |
| :-- | :-- | :-- | :-- |
| 0x00..0x0C | `PRIO1..PRIO4` | RW | 每源 2 位优先级（`0` = 屏蔽） |
| 0x10 | `PENDING`   | RO | 挂起 bitmap（`bit i` = 源 i 挂起） |
| 0x14 | `ENABLE`    | RW | 上下文使能 bitmap |
| 0x18 | `THRESHOLD` | RW | 上下文阈值（仅 `prio > threshold` 的源可见） |
| 0x1C | `CLAIM`     | R=claim / W=complete | 读 = 取顶源 id 并清其 pending；写 = EOI |
| 0x20 | `RAISE`     | WO | OR 进 pending（模拟外设拉高 IRQ 线） |

固定场景（四语逐位对照用同一组值）：

- `priority` = 源1→1、源2→2、源3→3、源4→3
- `enable` = 使能 `{1,2,3}`（= `0x0E`；**源4 不使能**）
- `threshold` = 1（**源1 prio1 ≤ 1 被挡**）
- `raise` 全部源 `{1,2,3,4}`（pending = `0x1E`）

## 2. 同一场景的三条断言（四语逐位一致）

| 里程碑 | 含义 | 期望结果 |
| :-- | :-- | :-- |
| `ROUTE_PASS` | 满 pending 下取最高优先级源 | 顶源 = **3**，prio = 3（源4 prio 也 3 但未使能；源1 被阈值挡） |
| `THRESH_PASS`（软件）| 阈值/使能过滤 | 仅 `{1}` → 0（阈值挡）；仅 `{4}` → 0（未使能）；仅 `{2}` → 2（可见） |
| `DEV_PASS`（硬件）| 寄存器字段响应 | prio/enable/threshold 回读正确；PENDING 为 RO（写被忽略） |
| `CLAIM_PASS` | claim/complete 握手 | 抽干序列 = **3,2,0**；余 `{1,4}`(=`0x12`) 被挡；complete 后重 raise 源3 → 再 claim = 3 |
| `ALL_PASS` | 全部通过 | - |

> **为什么抽干序列是 3,2,0**：满 pending 时可见集 = `{2(prio2), 3(prio3)}`（源1 阈值挡、源4 未使能）。
> claim 先取最高 prio 的 **3**（清 pending[3]）；再取 **2**（清 pending[2]）；第三次可见集空 → 返回 **0**，
> 而 pending 仍是 `{1,4}` - 这一枚 `claim=0 且 pending≠空` 同时证明了**阈值过滤**与**使能过滤**。

## 3. 四语对位的“同一台路由器”

| 部件 | Verilog | BSV | C | Rust |
| :-- | :-- | :-- | :-- | :-- |
| 源掩码 pending/enable | `reg [4:0]` bitmap | `Bit#(5)` bitmap | `volatile uint32_t` 位运算 | `bitflags! Sources` |
| 合格判定 | `pending[i]&enable[i]&(prio>thr)` | 同左（`Bool eX`） | `(pending>>i)&1 ...` | `pending.contains(bit) ...` |
| 优先级仲裁 | `always@(*)` 比较链 `>=` | 函数 `arbitrate` `>=` | for 循环 strict `>` | for 循环 strict `>` |
| claim 副作用 | 时钟沿 `pending&=~(1<<best)` | `method claim` 同左 | `pending &= ~(1<<id)` | `pending.remove(bit)` |
| complete/定序 | 写 CLAIM = no-op（EOI） | 同左 | `fence rw,rw` | （host，无需 fence） |

- **C**：claim 读 / complete 写都加 `fence rw,rw`（真 PLIC 驱动里 claim 的读、complete 的写
  必须有序，否则会丢中断或重复处理） - 延续 16b/16c 的 `fence` 主题。`env=gcc-rv64`，真 RISC-V `fence`。
- **Rust**：`pending`/`enable` 是“一组源标志”，正是 `bitflags!` 的主场；仲裁结果是源 id（`u32`）。
- **Verilog/BSV**：仲裁器就是一棵组合比较树，软件的 for 循环是它的语法外壳。

## 4. 分阶段 Pass 与判题 (DoD)

```
labctl run improper/16d-plic   # 跑 sw-c/sw-rust/hw-v/hw-bsv/essay
labctl hint improper/16d-plic  # 卡住看分级提示
```

统一 `expect = ["ROUTE_PASS","ALL_PASS"]`，`forbid = ["FAIL","panic","ERROR"]`。

- [ ] ① 合格判定：`eligible = pending & enable & (priority > threshold)` → 顶源 = 3 → `ROUTE_PASS`。
- [ ] ② 阈值/使能过滤：源1（阈值）、源4（使能）被挡，源2 可见 → `THRESH_PASS`（软件）。
- [ ] ③ 优先级仲裁 + claim/complete：抽干序列 3,2,0 + 重触发再 claim → `CLAIM_PASS`。
- [ ] 硬件 `plic` RTL 仲裁链 + claim 清 pending + tb 跑过 → `DEV_PASS`（0 warning）。
- [ ] 软件任一语言全过（必修）；另一语言、两条硬件路径多过计辅助分。
- [ ] `essay/THINKING.md`：把“中断”说清楚为**设备树里的一种总线绑定**（与 16-driver 对位）。

## 5. 简化取舍（留完整版作引申）

- **单上下文**：本课只建一个 hart-context；多 context 竞争（同一源被多核 claim、只一个赢）放在 `16e-intc-agg`。
- **源数压到 4、优先级 2 位**：真 PLIC 支持 1023 源、各 32 档优先级；这里取够讲清“过滤+仲裁+握手”的最小集。
- **pending 用寄存器 + RAISE 写注入**：真 PLIC 的 pending 由**网关**（gateway）从设备 IRQ 线的边沿/电平
  latch，且 complete 前不再转发同源 - 本课把网关简化成“claim 清 pending、complete 为显式 EOI 占位”。
- **claim 读副作用**：硬件里读 CLAIM 寄存器在时钟沿清对应 pending 位；软件里 `plic_claim()` 等价地先
  仲裁再清 pending，并用 `fence` 定序（对应真硬件“读即清”的不可重排）。

## 6. 思考题（`essay/THINKING.md` 作答）

把上一课 16-driver 的设备树接着往下读：一个设备节点除了 `compatible` / `reg`，还有
`interrupts` / `interrupt-parent`。说明**“中断”本质上也是一种总线绑定** - 设备通过设备树把自己的
IRQ 号绑定到中断控制器（PLIC）的某个输入，正如它用 `reg` 把寄存器窗口绑定到地址空间。
逐条对位 `priority/enable/threshold/claim/complete` 在这条绑定链里各处在哪一环。
