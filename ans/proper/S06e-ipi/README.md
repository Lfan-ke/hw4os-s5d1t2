# 正经·S06e · 真实 IPI（CLINT MSIP/SBI）+ 多核 PLIC claim 仲裁（参考解）

> 这是 improper 心智模型 **16c-clint（核内中断/IPI）** 与 **16e-intc-agg（中断聚合 + 多核仲裁）** 的「正经后续」：把那两节用最小模型讲过的「软件中断/IPI」「跨核 claim 谁处理」搬到 qemu-virt `-smp 4` 的**真内核**上跑。承接 S02（trap 框架）、S06c（PLIC claim/complete）、S13/S15（SBI HSM 启核）。

## 0. 这节课在讲什么

两条核内/核外中断的「跨核」主线：

- **软件中断 / IPI（核内，CLINT）**：一个 hart 如何中断另一个 hart？硬件上是 CLINT 的 **MSIP**（每 hart 一个，`0x0200_0000 + 4*hart`），写 1 即在目标 hart 触发机器软件中断。但 **S 态内核没权碰 MSIP**（那段属 M 态、OpenSBI 占着）。S 态做 IPI 的正路是 **SBI 的 IPI 扩展**：`sbi_send_ipi`（EID=`0x735049`"sPI"，fid=0）。OpenSBI 收到后，正是去写目标 hart 的 **CLINT MSIP** → 目标进 M 态软件中断 → OpenSBI 把 `mip.SSIP` 反射给 S 态 → 目标 hart 取到 **`scause=1`（S 态软件中断）**。复用 S02 的 trap 框架处理它，ack 就是清 `sip.SSIP`。
- **外设中断的多核仲裁（核外，PLIC）**：同一个外设源（UART，src=10）可以被**多个 hart 的 context** 同时使能。当它 pending 时，**谁来处理？** PLIC 的 `claim` 是「谁先读谁得到」的**原子领取**：第一个读 claim 的核拿到非零 irq 并把该源移出 pending，其余核读到 0。于是**恰一个 hart** 处理该中断 - 这正是 16e「同一 IRQ 对多 hart-context，claim/complete 保证只一个处理」的真身。

## 1. 如何做到确定性、不依赖时序

多核最易 flaky。本实验用两种手段把它做成**确定性**的：

1. **引导核不写死**：`-smp 4` 下 OpenSBI 选哪个 hart 引导是不确定的（引导核常是 hart 1/2/3）。内核用 `a0` 拿到引导 hartid，唤醒一个**确定不同**的 target（`(boot==0)?1:0`），经 SBI HSM `hart_start` 落到 `secondary_entry`（自备 per-hart 栈，同 S13/S15）。
2. **claim 仲裁用「屏障 + 轮询」而非异步抢中断**：两核都把 UART 源使能到自己的 S-context 后，回环自激把 src10 置 pending；两核在一个 `barrier`（原子计数到 2）处**同时**读各自的 `PLIC_CLAIM(ctx)`。因为两核**都确定读到了** claim，PLIC 必然仲裁出**一个赢家（拿到 irq=10）+ 一个输家（读到 0）** - `claim_nonzero==1 && claim_zero==1` 是硬性结果，不依赖中断到达时序。这把「多核谁处理」从「依赖时序偶然的中断风暴」收敛成「可复现的寄存器竞争」。

> 注意（同 S06c）：UART 既做回环自激、又是 SBI 控制台。一旦 `MCR.LOOP` 置上，发送被内部环回、终端看不到输出，所以 CLAIM 段只**记录**结果，等关回环、恢复控制台后再统一打印。打印**只由引导核**做 - 两核同时向 console 输出会打散 Pass 子串。

## 2. 你要实现什么

| 留白 | 文件 | 做什么 |
|------|------|--------|
| 发 IPI | `kernel/main.c`（`kmain` 的 IPI 段一行）| `sbi_send_ipi(1UL, target)`：给 target 发 S 态软件中断 |
| claim 仲裁 | `kernel/plic.c`（`plic_claim_one`）| `claim` 读 irq → 赢家读 UART RBR 清源 + 记账 + `complete` → 输家记 `zero` |

`plic_ctx_init`（配 S-context）、`trap_handler`（清 SSIP + 计数）、HSM 启核、屏障、自激全部已给。

## 3. DoD（判据）

| 输出 | 含义 |
|------|------|
| `IPI_PASS` | target 真的收到了 IPI：`ipi_count>=1` 且软件中断 `scause==1` |
| `CLAIM_PASS` | 两核同读 claim，PLIC 仲裁出恰一个赢家（irq=10、字节对）+ 一个输家（读 0） |
| `SMP_PASS` | target 上线 + IPI + 仲裁三者皆成（伞形总判据） |

判题 `expect=["IPI_PASS","SMP_PASS"]`（`SMP_PASS` 已蕴含 claim 通过），`forbid=["FAIL","panic","UNEXPECTED"]`。失败诊断：`IPI_MISS`/`CLAIM_MISS`/`SMP_MISS`/`ARM_MISS`。

## 4. 跑

```
make -C kernel kernel.elf && \
  timeout 20 qemu-system-riscv64 -machine virt -smp 4 -nographic -bios default -kernel kernel/kernel.elf
```

OpenSBI banner 后应见（引导核号每次可能不同）：

```
[S06e] real IPI (CLINT MSIP via SBI) + multi-core PLIC claim arbitration
boot hart=3, target hart=0
sbi_hart_start ret=0
target hart armed (stvec+PLIC ctx+SSIE ready)
IPI delivered to target, scause=0x8000000000000001
IPI_PASS
claim race: winner hart=3 got irq=10 byte=0x41, loser read claim=0
CLAIM_PASS
SMP_PASS
```

赢家可能是引导核也可能是 target（仲裁的两种结果都正常），但**永远恰一个**。

## 5. 文件

| 文件 | 作用 |
|------|------|
| `kernel/ipi.h` | PLIC/UART/CLINT 寄存器布局、SBI HSM/IPI 封装、scause/sie/sip 位、共享邮箱 |
| `kernel/main.c` | 引导核协调者（**留白：发 IPI**）+ target 主体（脚手架）+ 三项判据 |
| `kernel/plic.c` | `plic_ctx_init`（给定）+ **留白：`plic_claim_one` 仲裁** |
| `kernel/trap.c` | 给定：复用 S02/S06c 框架，处理 `scause=1` 软件中断（清 SSIP + 计数 + 风暴守卫）|
| `kernel/uart.c` | 给定：UART 寄存器读写 + 开 RX 中断/回环（同 S06c）|
| `kernel/secondary.S` | 给定：副 hart 裸入口（自备 per-hart 栈 → `secondary_main`，同 S13/S15）|

## 6. 引申

- **IPI 的真实用途**：TLB shootdown（改了页表，得让别的核刷 TLB）、跨核唤醒调度、停核/上下线、远程函数调用（`smp_call_function`）。本实验只发一发、计一数，骨架同真内核。
- **IPI 的内存序**：发 IPI 前对共享数据的写，必须在目标核处理 IPI 前可见 - 所以「先写数据、`fence` 再发 IPI」「收到 IPI 先 `fence` 再读数据」。本实验邮箱握手处处 `smp_fence()` 正是此理（呼应 16e essay 的 `barrier` 关键字）。
- **claim/complete 防重防丢**：claim 原子移出 pending → 防多核重复领取（防重）；complete 写回 → gateway 才再转发（防丢）。漏 complete：该源以后永不再来；漏读设备清源：complete 一瞬又 pending → 风暴。
- **对照 CLINT vs PLIC**：软件中断/timer 是**核内**（CLINT，per-hart，`scause=1/5`，无需仲裁）；外设中断是**核外**（PLIC，可路由多源到多 context，`scause=9`，claim/complete 仲裁）。S02 走 timer、S06c 走单核 PLIC、本课把两者都升到「跨核」。
- **现代化**：线式 PLIC 的 claim/complete 往返在虚拟化下每次陷出；RISC-V AIA 用 IMSIC 把中断做成「往 CSR 收件」，IPI 也可由 IMSIC 直接投递，省掉 SBI 往返。理解了本课的 MSIP/claim 语义，看 AIA 就是把同一套搬到消息与 CSR 上。
