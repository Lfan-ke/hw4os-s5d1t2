# 16e · 中断聚合：多核仲裁（CLINT IPI + PLIC 多核 claim 竞争）

> 不正经赛道 · 第 16e 课 - 软件 host/qemu-user 直接跑；硬件走 iverilog/bsc 仿真。
> 一句话母题：**同一个 IRQ 源对着 N 个核，凭什么“只有一个核去处理”？凭 `claim/complete` 的 gateway 仲裁。**
> 三件事在这里合流：CLINT 的 **IPI**（核间软件中断）、PLIC 的 **外设 IRQ 路由**、以及 **多核仲裁**。
> 四种语言跑**同一聚合场景**，打印**完全相同**的 `ARBITER_PASS` / `IPI_PASS` / `ALL_PASS`。

## 0. 这节课在讲什么

前两课（16c CLINT / 16d PLIC）分别建了**核内中断**（timer + 软件中断/IPI）与**核外中断**（外设 IRQ 路由）的心智模型。
这一课把它们**聚合**，再加上真实多核里最容易踩坑的一环 - **仲裁**：

- 一个外设拉起 IRQ7，它对**每个** hart 都有一份 context（priority/enable/threshold/claim/complete）。
- 中断到来时，**多个核都可能被通知**；但这个中断只该被处理**一次**。谁来处理？
- 答案是 **`claim`**：每个核去读自己 context 的 claim 寄存器。PLIC 的 **source gateway** 保证：
  把该源从 “pending” 翻进 “in-flight” 的**第一个**核读到 `hwirq=7`（它就是处理者），
  其余核的 claim 读到 `0`（无事可做）。这就是**仲裁** - 不需要锁，gateway 本身就是仲裁器。
- 处理完，处理者写 **`complete`** 把 gateway 还回去（in-flight→空闲），源才能再次投递。
- 处理者还可能要**协调其它核**：写它们的 **MSIP**（CLINT 软件中断）敲一次 **IPI**；
  跨核传数据时，**先写数据、`barrier`、再敲 IPI**，对端**先看到 IPI、`barrier`、再读数据** - 这就是内存序。

对应真实系统：Linux `drivers/irqchip/irq-sifive-plic.c` 的 `plic_handle_irq`（`readl(claim)` 循环 + `writel(hwirq, claim)` 完成）、
RISC-V 的 `arch_send_call_function_ipi`（写 CLINT MSIP / `sbi_send_ipi`）+ `smp_wmb()/smp_mb()`。
proper `S06e-ipi` 在 qemu-virt `-smp` 上把这套跑成真内核。

## 1. 聚合场景（四语逐位对照用同一剧本）

| 量 | 值 | 含义 |
| :-- | :-- | :-- |
| `N_HART` | 4 | hart 数（每个对该 IRQ 有一份 context） |
| `IRQ_ID` | 7 | 共享外设中断号 |
| `WINNER` | hart0 | 第一个 claim 的核 → 唯一处理者 |
| 竞争者 | hart1/2/3 | claim 读 0（gateway 已 in-flight） |
| IPI 目标 | hart1/2/3 | 处理者向它们写 MSIP |
| `PAYLOAD` | 0x0000ABCD | 跨核共享数据（barrier 保证可见） |

剧本三幕：

1. **仲裁（claim）**：设备拉 IRQ7 → hart0 先 claim 得 7（gateway 0→1）→ hart1/2/3 claim 各得 0。唯一处理者 = hart0。
2. **完成（complete）**：先演“**未 complete 再 claim 读 0**（卡住）”，再 `complete` 后“**claim 又得 7**（重新武装）”。
3. **跨核（IPI）**：hart0 写 payload → `barrier` → 敲 hart1/2/3 的 MSIP；从核见 MSIP → `barrier` → 读到 payload=0xABCD → 清自己的 MSIP。

## 2. 四语对位的“同一套仲裁原语”

| 概念 | C（规矩·rv 本相） | Rust（类型化原子） | Verilog | BSV |
| :-- | :-- | :-- | :-- | :-- |
| claim 原子 | `amoswap.w`（真 RV 原子） | `AtomicU32::swap(AcqRel)` | `claim_req & pending & ~inflight` | `if (pending && !inflight)` |
| gateway in-flight | `volatile uint32_t inflight` | `AtomicU32 inflight` | `reg inflight_r` | `Reg#(Bool) inflight` |
| complete | `inflight=0` + `fence` | `store(0, Release)` | `inflight_r<=0` | `inflight<=False` |
| IPI（敲 MSIP） | `sw`（写 `msip[t]`） | `msip[t].store(1)` | `msip_r[ipi_target]<=1` | `msipR<=msipR｜(1<<target)` |
| 内存序 barrier | `fence rw,rw` | `fence(Release)/fence(Acquire)` | （时序天然有序） | （rule 间天然有序） |

> **为什么 claim 必须原子**：多核同时 claim，如果只是“读 inflight==0 再写 1”分两步，会有两个核都读到 0 都以为自己赢 - 
> 经典 race，IRQ 被处理两次。`amoswap.w`（或 `AtomicU32::swap`）把“读旧值 + 写新值”做成**一条不可分指令**，
> 只有把 0 换成 1 的那个核拿到旧值 0、判定为赢家。硬件侧 gateway 寄存器在一个时钟沿里完成同一件事。

## 3. 里程碑与判题 (DoD)

每条路径打印共同里程碑（逐位一致），软件路径另打印自己的台阶：

- `sw-c`：`CLAIM_PASS`（amoswap 抢到 gateway）· `ARBITER_PASS`（唯一处理者）· `COMPLETE_PASS`（重新武装）·
  `IPI_PASS`（barrier 序 + MSIP）· `ALL_PASS`。
- `sw-rust`：同序，原语换成 `AtomicU32::swap` / `fence(Release/Acquire)`。
- `hw-v` / `hw-bsv`：`ARBITER_PASS` · `DEV_PASS`（complete 后重新武装）· `IPI_PASS` · `ALL_PASS`，且 **0 warning**。

统一 `expect = ["ARBITER_PASS","ALL_PASS"]`，`forbid = ["FAIL","panic","ERROR"]`。

```
labctl run improper/16e-intc-agg   # 跑 sw-c/sw-rust/hw-v/hw-bsv/essay
labctl hint improper/16e-intc-agg  # 卡住看分级提示
```

- [ ] claim 用原子原语把 gateway 0→1：唯一一个核拿到 IRQ_ID → `ARBITER_PASS`。
- [ ] complete 后 gateway 重新武装（先演“漏 complete 卡住”）→ `COMPLETE_PASS` / `DEV_PASS`。
- [ ] 先写 payload → barrier → 敲 IPI；从核见 MSIP → barrier → 读 payload → `IPI_PASS`。
- [ ] 硬件多 context 仲裁 RTL + tb 跑过 → `ARBITER_PASS`/`IPI_PASS`/`DEV_PASS`（0 warning）。
- [ ] 软件任一语言全过（必修）；另一语言、两条硬件路径多过计辅助分（按轴）。
- [ ] `essay/THINKING.md`：读缩减版真内核 PLIC claim/IPI 源码，答“谁处理 / 为何 complete / 漏 complete 会怎样 / IPI 内存序”。

## 4. 关键决策

1. **C 的 claim 走真 `amoswap.w`（不是 `if` 模拟）。** improper 默认 `env=host`，但本仓库有 `riscv64-linux-gnu-gcc`+`qemu-riscv64`
   （`gcc-rv64` 构建器），故 `sw-c` 整程序交叉编成 riscv64 ELF 跑在 qemu-user - claim 的原子、MSIP 的 `sw`、
   跨核可见性的 `fence rw,rw` 都是**真指令**。“多核仲裁只一个赢”不再是叙述，而是 `amoswap.w` 的语义本身。
2. **N hart 用顺序遍历建模，但原语是真的。** host/qemu-user 单线程跑不出真并发，本课不追真抢占；
   建模为“逐个 hart 依次 claim”，让 gateway 的 in-flight 决定唯一赢家 - 这与真硬件“同一沿仲裁”同构，
   且把**易错点**（漏 complete、IPI 缺 barrier）显式演出来。真并发版在 proper `S06e-ipi`（qemu-virt `-smp`）。
3. **统一里程碑取 `ARBITER_PASS`/`IPI_PASS`/`ALL_PASS`**：四变体都打印；`CLAIM_PASS`/`COMPLETE_PASS`（软件）、
   `DEV_PASS`（硬件）是各轴 bonus，不进 `expect`。

## 5. 简化取舍（留完整版作引申）

- 只建 **gateway 仲裁 + MSIP**：priority/threshold/enable 默认全使能（prio>thresh），不做多源优先级仲裁（那属于 16d）。
- 单源（IRQ7）+ 固定 winner（hart0，因其先 claim）：不引入随机/抢占；仲裁的**结构**（唯一赢家、gateway 重新武装）已完整。
- IPI 的“跨核可见性”在单线程下用 `barrier` 显式标注内存序点（真重排见 proper 的多核），逐位结果不依赖时序竞争。
- 硬件 tb “一拍一个 claim”串行喂入，对位软件的顺序遍历；真多端口同沿仲裁留作 RTL 引申。

## 6. 思考题（`essay/THINKING.md` 作答）

给一段**缩减版真内核**（Linux PLIC `plic_handle_irq`/`plic_irq_eoi` + RISC-V IPI 发送/接收）源码，回答：
**谁处理该 IRQ、为什么必须 `complete`、漏 `complete` 会怎样、IPI 的内存序为何要 `barrier`**。
关键字：`claim` / `complete` / `hart` / `barrier`。
