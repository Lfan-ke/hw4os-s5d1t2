# 18 · 系统调用：从 MCU 中断向量表到 MPU 系统调用

> 不正经赛道 · 第 18 课 —— 软硬同构（S1）+ 软件演化链（S2→S3→S4）。
> 一句话母题：**中断 = 硬件驱动的间接跳转；syscall = 被 ABI 驯化、过特权级的受控陷入。**

## 0. 这节课在讲什么

单片机（MCU）裸机时代，「中断」就是 CPU 硬件按异常号在一张表里查个地址、直接跳过去——
简单粗暴，没有门卫。当芯片长出特权级、跑起 OS（MPU/通用处理器），用户态想求内核办事，
就不能再「跳进内核地址」了，得排队、报号、过安检——这道安检门就是 `ecall` 触发的系统调用。

你将沿这条演化链亲手补全四段：

1. **S1 向量分发器**（软硬同构）：硬件按号查表的间接跳转 `pc = base + 4*cause`。
2. **S2 Trap 上下文**：陷入时整存寄存器现场，返回时 `sepc+=4`、`a0`=返回值。
3. **S3 Syscall ABI**：`a7`=号、`a0..a5`=参、`a0`=返；分发表 + 三个 handler（真实 RV64 号）。
4. **S4 真实 ecall 往返**：内联汇编真的陷入内核，把 `HELLO_SYSCALL` 打到 stdout。

对应真实系统：RISC-V `mtvec/stvec`(BASE+MODE)、Cortex-M `VTOR`+NVIC 向量表、
rcore `TrapContext`、xv6 `trapframe`、`scause=8` 的 U 态 `ecall`、Linux/RV64 号（write=64/exit=93/getpid=172）。

## 1. 变体矩阵

| 子实验 | sw-rust | sw-c | hw-v | hw-bsv | essay |
| :-- | :--: | :--: | :--: | :--: | :--: |
| S1 向量分发 | ✓ | ✓ | ✓ | ✓ | |
| S2 trap 上下文 | ✓ | ✓ | | | |
| S3 syscall ABI | ✓ | ✓ | | | |
| S4 真实 ecall | ✓ | ✓ | | | |
| S5 思考题 | | | | | ✓ |

- `sw-rust` 走 **host**：S4 用 x86-64 `syscall` 指令（与 RV64 `ecall` 概念同构）。
- `sw-c` 走 **gcc-rv64 / qemu-user**：S4 是**真实 RV64 `ecall`**，命中真实 GNU/Linux ABI。
- `hw-v` / `hw-bsv` 是 S1 的「软硬同构」锚点——同一 dispatch 公式，硬件就是移位+加法+二选一。
- `require = 1`：任一变体过即必修达成；多过的路径进辅助分账本（尤其 S1 软↔硬都跑一遍）。

## 2. 你要填什么（【STUDENT】标注处）

软件（`sw/rust/src/main.rs` 与 `sw/c/syscall.c`）：

| 函数 | 要求 | 判据 |
| :-- | :-- | :-- |
| `dispatch` | `mode?base+4*cause:base`；`accept=trap_req` | DIRECT / VECTORED / DISPATCH → `S1_PASS` |
| `ctx_save` | 32 GPR + sepc + sstatus 整存 | `SAVE_PASS` |
| `ctx_advance` | `a0=retval`；`sepc+=4` | RETVAL / RESTORE → `S2_PASS` |
| `sys_write/getpid/exit` + `syscall` | 按 a7 分发；未知号 -ENOSYS | NR_* / ENOSYS → `S3_PASS` |
| `raw_syscall3` | 内联汇编真的 `syscall`/`ecall` | ECALL / SYSRET → `S4_PASS` |

硬件（`hw/v/vec_dispatch.v`、`hw/bsv/VecDispatch.bsv`）：只填 `vec_dispatch` 的组合逻辑：
`handler_pc = mode ? base + (cause<<2) : base; accept = trap_req;`（0-warning 门）。

`S1` 公式可二选一深入：`// TODO[a]` 只实现向量化公式即可过；`// ELSE[b]` 额外把 direct 模式也独立接好
（本参考骨架已把两态都写进同一表达式）。

```
labctl run improper/18-syscall      # 跑全部五个变体
labctl watch                        # 边改边自动判定
labctl hint improper/18-syscall     # 卡住看提示
labctl wave improper/18-syscall     # 看 S1 硬件波形
```

## 3. 判题口径

五变体都打印 S1 组 + `ALL_PASS`；软件再额外打 S2/S3/S4 组，`ALL_PASS` 受四段全过门控。

- `expect = [DIRECT_PASS, VECTORED_PASS, DISPATCH_PASS, S1_PASS, ALL_PASS]`
- `forbid = [FAIL, panic, ERROR]` —— 任一子实验失败会印 `*_FAIL`，直接挂。
- 硬件变体额外要 **0 warning**（`warn_gate`）。

## 4. 完成标准 (DoD)

- [ ] S1 至少一条路径（软或硬）打印 `S1_PASS`，能说清「向量化 = `base+4*cause` 的硬件间接跳转」。
- [ ] S2：返回值正确、其余 31 个寄存器逐位不变、`sepc` 正好 +4（`S2_PASS`）。
- [ ] S3：三个号正确分发、未知号 `-ENOSYS`（`S3_PASS`）。
- [ ] S4：真实 `ecall`/`syscall` 往返打出 `HELLO_SYSCALL` 并 `S4_PASS`。
- [ ] 硬件变体 0 warning；任一变体跑出 `ALL_PASS`（必修）。
- [ ] 能讲出「MCU 裸中断 → MPU 受控 syscall」这条演化线（思考题 `essay/THINKING.md`）。

## 5. 思考题（`essay/THINKING.md` 作答即可通过）

1. MCU 直接按向量表跳进 handler 地址 vs MPU 必须走 `ecall`+调用号：OS 为什么不让用户态直接跳到内核函数地址？（隔离 / ABI 稳定 / 安检门——把「地址」换成「号」换来了什么？）
2. 向量化 vs 非向量化分发：硬件多连几根线、省掉哪段软件开销？接回 `01-hw-vlan` 的软硬成本核算，各举一个该选向量化、该选 direct 的场景。
3. 为什么 RISC-V 用「`ecall`+寄存器约定」而某些 CISC 用专门的 `syscall`/`int 0x80`？把「分发/取号」放硬件还是软件，各自代价是什么？
4. GNU/Linux 把 `a7`=号、`a0..a5`=参、`a0`=返固定成 ABI 的意义：若每个内核版本都改寄存器约定，会对 libc、对已编译的二进制造成什么后果？

## 6. 简化取舍（简化的是学生负担，非功能完整性）

- cause 压成 4-bit、向量表 16 项；只做 direct/vectored 两态，**不做**中断优先级/嵌套/抢占（NVIC priority）。
- S2/S3 在 host 把寄存器/内核态建模成结构体，免去真实 CSR 机器；S4 的 `sw-c` 走 qemu-user 命中真实 RV64 ecall。
- syscall 表只取 write/exit/getpid 三号代表，号值用真实 RV64 ABI 以保 GNU 规范味。
- 完整版引申：可剥夺嵌套中断 + 优先级控制器（PLIC/CLINT/NVIC）、完整 Linux 号与 errno、`ERESTARTSYS`、vDSO、`sigreturn`。
