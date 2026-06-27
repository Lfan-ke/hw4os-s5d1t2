# 正经·S6b · 硬件抽象层(HAL)：迷你 Abstract Machine

> 承接 S2（trap）/ S5（上下文切换）/ S6（裸 MMIO 驱动）。本课换一个视角：把“裸机硬件细节”
> 抽象成一组**可移植 HAL API**——程序只调 HAL，不碰具体寄存器，于是同一份代码能跑遍不同硬件。
> 这正是 YSYX **Abstract Machine** 的核心思想，我们在 S 态把它最小复刻一遍。

## 0. 这节课在讲什么

S6 的驱动直接碰 MMIO 寄存器、S2 的 trap 直接写 `stvec/sepc`——程序和**具体硬件**绑死了。
HAL 的主张是：在硬件之上架一层**与体系结构无关的接口**，把寄存器、CSR、SBI 全藏到接口背后。
程序只认接口；换一块板子/换一个 arch，只要重写接口的实现，**程序源码一字不改**。

AM 把一台“抽象计算机”切成五层，本课实现其中最关键的三层：

| 层 | 管什么 | 本课接口 | 背后藏着 |
|----|--------|----------|----------|
| **TRM** | 最小计算：输出/停机/一块内存 | `putch` / `halt` / `heap` | SBI 控制台、SBI 关机、一段 RAM |
| **IOE** | 设备 I/O，统一寄存器接口 | `ioe_read/ioe_write(reg, buf)` | `rdtime`、串口 |
| **CTE** | 上下文/陷入/事件 | `cte_init` / `yield` / `kcontext` | `stvec`、自陷、`sepc`、`sret` |

CTE 是骨架：它把“硬件陷入”翻译成与 arch 无关的 `Event`+`Context`，上层调度器只跟事件打交道。
在它之上，我们仿 `yield-os` 造 **2 个任务互相 `yield`**，调度器轮转——和 S5 的协作式调度同构，
但这次切换的“机制”全部由 CTE 提供，调度“策略”不碰一行汇编。**机制与策略，就此解耦。**

## 1. 你要实现的（两处 `// TODO`）

1. **`kernel/ioe.c` 的 `ioe_read(AM_TIMER_UPTIME, …)`**：把 `t->us` 填成开机至今微秒数。
   virt 机 `mtime` 约 10MHz → `us = rdtime()/10`（`riscv.h` 的 `r_time()` 即 `rdtime`）。
2. **`kernel/cte.c` 的 `yield()` 与 `__am_irq_handle()`**：
   - `yield()`：执行一条断点指令**自陷**（`.word 0x00100073`，未压缩 `ebreak`）。
   - `__am_irq_handle()`：按 `scause` 把陷入翻成 `Event`（断点 `scause==3` → `EVENT_YIELD`，且
     `sepc += 4` 跨过自陷指令），调用上层 `handler`，**返回它给出的“下一个 `Context*`”**。

给定（勿改）：`trap.S`（S 态陷入入口）、`trm.c`（TRM）、`main.c`（harness + yield-os 风格 2 任务）。

```
cd kernel && make kernel.elf
make run     # OpenSBI banner 后见内核输出；kmain 返回后 k_shutdown 退出 qemu
```

判据：输出含 `TRM_PASS` / `IOE_PASS` / `CTE_PASS` / `ALL_PASS`；运行序为 `ABAB`；
不得出现 `FAIL` / `panic` / `UNEXPECTED`。失败诊断为 `*_MISS`。

## 2. 完成标准 (DoD)

- [ ] `IOE_PASS`：连读 `TIMER_UPTIME` 单调递增。
- [ ] `CTE_PASS`：2 任务互相 `yield`，调度器轮转，运行序 `ABAB`。
- [ ] 能说清 `yield()` 自陷 → `trap.S` 存现场 → `__am_irq_handle` 派事件 → `handler` 选下一个 →
      `trap.S` `mv sp,a0` 切栈 `sret` 这条完整链路。
- [ ] 能讲清为何 S 态自陷用 `ebreak` 而非 `ecall`（后者被 OpenSBI 当 SBI 调用截走）。

## 3. 为什么是“可移植”的

`main.c` 里没有一条 `csrr/csrw`、没有一个裸地址：它只调 `putch/ioe_*/yield/kcontext`。
把 `trm.c/ioe.c/cte.c/trap.S` 换成 x86 或另一块 RISC-V 板子的实现，`main.c` 原样就能跑。
这正是 nanos-lite / Nanos 等系统建在 AM 之上、却能跨 NEMU / qemu / 真机运行的原因。

## 4. 引申

- 补 **VME（虚存）**：`map/protect/ucontext`，把任务送进 U 态独立地址空间（接 S5c 分页）。
- 补 **MPE（多核）**：`cpu_count/atomic_xchg`，多核各跑一份调度（接 S13/S15）。
- 换一个 **arch** 实现同一套 `am.h`（如改用 `ecall`+`medeleg`、或 M 态版本），`main.c` 不变。
- 把已有 **S2/S5** 重构到 CTE 之上：trap 与调度都只跟 `Event/Context` 打交道。
