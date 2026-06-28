# 正经·S06c · PLIC 外部中断（参考解）

> 承接 S02（timer-only：本地时钟中断走 CLINT，scause=5）。S02 的中断是 CPU **本地**的、由 `sie.STIE` 直接管。本课补上另一条主线：**外设中断**。外设（这里是 UART）的中断线先汇到一个**外部中断控制器 PLIC**，PLIC 再按优先级/使能/阈值把它路由给某个 hart 的某个特权级 context，CPU 看到的是 `scause=9`（S 态外部中断）。处理时必须走 **claim → 服务设备 → complete** 这套握手——这正是 Linux/xv6 处理一切外设中断的骨架。

## 0. 这节课在讲什么

- **PLIC vs CLINT**：CLINT 管「本地」中断（每 hart 的 timer/software，scause=5/1）；PLIC 管「全局外设」中断（UART/磁盘/网卡…，scause=9）。前者是 CPU 自带的小定时器/IPI，后者是一块可路由多源多目标的中断仲裁器。
- **PLIC 三件套**：源 `priority`（0=禁用）、每 context 的 `enable` 位图、每 context 的 `threshold`（只放行 priority>threshold 的源）。context = (hart, 特权级)，qemu virt 上 hartN 的 S-context 号 = `2N+1`。
- **claim/complete 握手**：读 claim 寄存器 → 原子拿到最高优先级 irq 号并把它移出 pending（防止别的核重复领取，**防重**）；服务完设备后把 irq 号写回 → complete，gateway 才会再次转发该源（**防丢**）。

## 1. 你要实现什么

确定性地「自激」一次外设中断并正确处理它：

1. **PLIC 使能配置**（`plic.c::plic_init`，学生填一行）：在**当前 hart** 的 S-context 把 UART 源（IRQ=10）使能位置 1。优先级、阈值已给。
   - 为什么按当前 hart？`-smp 4` 下 OpenSBI 的启动 hart **每次可能不同**；PLIC 是 per-context 的，必须配自己这一份（hartid 由 SBI 经 `a0` 传入，harness 已取好）。
2. **外部中断处理**（`plic.c::plic_external_handler`，学生填）：`claim`（读 claim 寄存器得 irq）→ 读 `UART RBR` 取回字节并清设备中断线 → `complete`（把 irq 写回 claim 寄存器）。

自激手法：UART 开 `IER.ERBFI`(收到数据中断) + `MCR.LOOP`(回环)，然后向 `THR` 写一个字节 —— 字节在 UART 内部环回成「收到」，置 `LSR.DR`、拉高 UART→PLIC 中断线 → `scause=9`。

> 注意：本实验复用**同一个 UART** 做回环，而 SBI 控制台也用它；一旦 `MCR.LOOP` 置上，发送被内部环回、终端看不到输出，所以 harness 把所有打印都推迟到「关回环、恢复控制台」之后统一输出。

## 2. DoD（判据）

| 输出 | 含义 |
|------|------|
| `PLIC_SETUP_PASS` | priority/enable/threshold 与 UART 的 IER/MCR 都配对了 |
| `IRQ_PASS` | 外部中断**真的触发**，且 `scause==9` |
| `RX_PASS` | `claim` 得到 `irq==10`，且读回的字节 == 发出的字节 |
| `COMPLETE_PASS` | `complete` 后**不再重复触发**，外部中断计数恰为 1 |
| `ALL_PASS` | 以上全过 |

失败诊断：`*_MISS`（`ENABLE_MISS`/`IRQ_MISS`/`RX_MISS`/`COMPLETE_MISS`…）。全程有界：等待用计数自旋而非 `wfi` 死等；万一 complete 没写对导致中断风暴，`trap.c` 的计数守卫会兜底屏蔽 `sie.SEIE`，不会卡死。

## 3. 跑

```
make -C kernel kernel.elf && \
  timeout 20 qemu-system-riscv64 -machine virt -smp 4 -nographic -bios default -kernel kernel/kernel.elf
```

OpenSBI banner 后应见：`boot hartid=...` / `PLIC_SETUP_PASS` / `IRQ_PASS` / `RX_PASS` / `COMPLETE_PASS` / `ALL_PASS`，随后 `k_shutdown` 退出。

## 4. 文件

| 文件 | 作用 |
|------|------|
| `kernel/plic.h` | PLIC/UART 寄存器布局、context 宏、scause=9/SEIE 定义、统计量声明 |
| `kernel/plic.c` | **学生填**：PLIC 使能 + claim/读设备/complete |
| `kernel/uart.c` | 给定：UART 寄存器读写 + 开 RX 中断/回环 |
| `kernel/trap.c` | 给定：复用 S02 框架，扩 `scause=9` 分支 + 中断风暴守卫 |
| `kernel/main.c` | 给定：取 hartid、配置、自激、四项判据 |

## 5. 引申

- **优先级/阈值/抢占**：多源时 PLIC 按 priority 仲裁；threshold 可临时屏蔽低优先级；同优先级用 ID 仲裁。
- **中断路由**：同一源可被多个 context 使能，但 claim 是「谁先读谁得到」，天然防多核重复处理。
- **MSI / AIA（APLIC+IMSIC）**：新一代用「消息中断」（往内存地址写一个 word）取代线式 PLIC，去掉 claim/complete 轮询、更易虚拟化；ARM 对应 GIC。
- **对照 S02**：把本课的 PLIC 外部中断与 S02 的 CLINT 时钟中断并排看——一个全局可路由、一个 per-hart 本地，scause 9 vs 5，握手 claim/complete vs 重置比较器。
