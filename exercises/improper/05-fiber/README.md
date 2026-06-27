# 05 · 纤程（有栈协程）：绿色线程与用户态上下文切换

> 不正经赛道 · 第 5 课 —— 纯用户态机制。Rust 走 host(x86_64)，C 走 qemu-user(RV64 真汇编)。
> 一句话母题：**切换 = 换一组 callee-saved 寄存器 + 换一根栈**；不穿内核、不重载 TLS，所以「绿」得便宜。

## 0. 这节课在讲什么

操作系统切线程要「上朝面圣」——陷入内核、存满一柜子寄存器、重载 TLS。纤程是「民间私了」：两个执行流在用户态互相递接力棒（`yield`），只把对方真正在乎的几个寄存器换一下就跑。学完你会明白绿色线程「绿」在哪、为什么便宜，以及一个反直觉的事实——**一堆「永不让出」的异步任务在单线程上其实就是批处理：做完一个才做下一个，毫无并行**。

承接线程管理 lab 的「上下文 = GPRs(+CSR)」：协作式切换只需保存 RISC-V **callee-saved 子集**（`ra/sp/s0–s11`），因为 caller-saved 早被编译器在每个 `call` 处替你溢出了——这正对应 rcore `__switch` 保存 `ra+sp+s0~s11` 的 `TaskContext`。

## 1. 程序五阶段（一个二进制跑完，逐段打印 PASS）

| 阶段 | 现象 | 判据 |
| :-- | :-- | :-- |
| ① 手写上下文切换 | 两纤程交替 `PING`/`PONG` | `CTXSW_PASS` |
| ② 极简运行时 spawn+yield | 3 纤程 round-robin → `A1 B1 C1 A2 B2 C2` | `SCHED_PASS` |
| ③ 无让出 → 顺序批处理 | `task0_done → task1_done → task2_done` | `SEQ_PASS` |
| ④ 插入让出点 → 交错 | `0a 1a 2a 0b 1b 2b` | `INTERLEAVE_PASS` |
| ⑤ 类库设施复现 | Rust std(线程+channel) / C ucontext → 同序列 | `LIB_PASS` |

五段皆过再打印 `ALL_PASS`。

## 2. 你要填的地方（`sw/rust/src/main.rs` 或 `sw/c/fiber.c`）

1. **`switch_ctx` 汇编体**（核心）：把 callee-saved 子集存进 `*old`、从 `*new` 读回、`ret`。
   - C/RV64（`// TODO[a] global_asm` 风格）：`sd ra,0(a0) … sd s11,104(a0); ld …; ret`。
   - Rust/x86_64：`push rbp/rbx/r12–r15; mov [rdi],rsp; mov rsp,[rsi]; pop …; ret`。
   - 引申 `// ELSE[b]`：Rust 可改用 `#[naked]` + `asm!` 内联（这里用 `global_asm!`）。
2. **新纤程首次跳板**（`spawn`）：设 `ctx.ra`(RV64) 指向 `trampoline`、`ctx.sp` 指向新栈顶；x86_64 在新栈顶预埋返回地址与寄存器槽。
3. **`yield_now()`**：标记自己 READY，再 `switch_ctx(自己, 调度器)`。
4. **批处理的让出点**（`body_inter`）：`// TODO[a]` 不插 yield（退化为顺序）/ `// ELSE[b]` 在两段输出间插一句 `yield_now()`（交错）。
5. **类库阶段**：填库的 `resume` 调用序列，收集结果。

```
labctl run improper/05-fiber      # 跑 C(RV64/qemu) + Rust(host) 两条路径
labctl watch                      # 边改边自动判定
labctl hint improper/05-fiber     # 卡住看提示
```

## 3. 完成标准 (DoD)

- [ ] `CTXSW_PASS`：两纤程 ping-pong，确认只保存了 callee-saved 子集。
- [ ] `SCHED_PASS`：运行时能 spawn 多纤程并 round-robin 协作。
- [ ] `SEQ_PASS` + `INTERLEAVE_PASS`：演示「无让出 → 顺序」、插 yield 后交错。
- [ ] `LIB_PASS`：用现成有栈设施（std 线程 / ucontext）复现同序列。
- [ ] C/Rust 任一全过（必修）；另一条也过计辅助分。
- [ ] `essay/THINKING.md` 说清「绿色线程为何便宜」。

## 4. 思考题（`essay/THINKING.md` 作答即可通过）

1. 为什么协作式纤程切换只需保存 callee-saved 寄存器而非全部 GPRs？编译器在每个 `call` 处替你做了什么？（RISC-V 调用约定与 caller-saved 溢出）
2. 把内核线程换成纤程，省掉了哪些内核态开销（特权级穿越、TLS 重载、内核栈切换、调度器陷入）？反过来，哪些场景纤程**不划算**（某任务长时间不让出会饿死同线程其他纤程；阻塞式系统调用会卡住整条载体线程）？
3. 单线程上跑一堆「永不让出/不阻塞」的异步任务，为什么等价于顺序批处理？要让它们真正交错，最少需要引入什么？这与下一课「无栈协程的 `poll`」在让出方式上有何异同？
