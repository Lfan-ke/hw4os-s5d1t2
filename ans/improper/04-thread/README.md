# 04 · 线程管理：进程是线程的资源容器

> 不正经赛道 · 第 4 课 —— 纯软件 / 软件模拟硬件，host 直接跑。
> 一句话母题：**进程不干活，线程才干活**。进程是「皮包公司」（租地址空间、办 fd 门禁卡、备信号量公章），线程是雇来真正敲代码的员工。
> 本课把「又当老板又当员工」的进程，拆成「老板 PCB＝资源容器」＋「员工 TCB＝一把寄存器」。

## 0. 这节课在讲什么

对标 rcore ch7→ch8 的关键重构：ch7 的 `Process` 既装资源又当执行单元；ch8 拆成
`Process`（`address_space`/`fd_table`/`semaphore_list`，全线程共享）＋ `Thread`（仅 `tid` ＋ `context`）。
这就是本课的 **PCB→TCB**。对照物：xv6 `struct proc` 的执行身份＝`trapframe`＋`swtch.S` 的 callee-saved；
rcore 的 `__switch`/`TaskContext` 把「上下文＝一组寄存器」讲得最直白；RISC-V 里 **CSR＋GPRs 就是一个上下文**；
硬件超线程（Intel HT / SMT）＝复制 architectural state、共享执行后端。

三个子实验逐题递进：

1. **04-1 上下文 = CSR + GPRs**：`ctx_save` / `ctx_restore` / 协作式 `ctx_switch`，把「当前虚拟 CPU」的整套寄存器整存整取。
2. **04-2 PCB→TCB**：抽出共享资源 `Process`，`Tcb` 只留 `ctx ＋ 指向 Process 的指针`；同进程两线程共享同一 fd/内存；调度器轮流切换多个 TCB。
3. **04-3 超线程（思考题）**：硬件线程共享什么、软件线程共享什么，为何都遵循「复制最少的执行身份，共享最贵的资源」。

## 1. 数据模型

```
Context  { gpr[31], sepc, sstatus, sp }          // 一个上下文 = 一把寄存器 = 一个执行身份
SharedRes{ mem[8], fd[8], sem }                  // 进程资源：地址空间 / fd 表 / 信号量（占位）
Process  { pid, res: SharedRes }                 // PCB = 资源容器（全线程共享）
Tcb      { tid, ctx: Context, proc: →Process }   // TCB = 上下文 + 指向 PCB 的指针
```

## 2. 你要填的函数

软件变体（`sw/rust/src/main.rs` 或 `sw/c/thread.c`）：

| 函数 | 要求 | 判据 |
| :-- | :-- | :-- |
| `ctx_save` / `ctx_restore` / `ctx_switch` | 整存整取一套寄存器；switch=save 后 restore | 切换后现场与 `sepc` 随上下文整组迁移 → `CTX_SWAP_PASS` |
| `spawn_shared_pair` | 两个 TCB 指向**同一个** Process | 线程1 写 fd/mem，线程2 读到同一份 → `SHARE_PASS` |
| （复用 `ctx_switch`）| 调度器轮流切换 TCB | 轮转执行序正确、各 TCB 上下文独立 → `SCHED_PASS` |

`ctx_save` 可二选一：`// TODO[a]` 具名字段逐个搬 / `// ELSE[b]` 整体赋值（`*cur = vcpu.regs`）。
`spawn_shared_pair` 可二选一：`// TODO[a]` `Rc<RefCell<_>>` / `// ELSE[b]` `Arc<Mutex<_>>`（多线程版）。

硬件变体（`hw/v` / `hw/bsv`，辅助分，0 warning 才计）：填 `ctx_rf` 的读口与共享加法器 /
`vcpu_read` `vcpu_sum`。`bank0/bank1` 是复制的影子寄存器组，读口与加法器是共享后端，
由 `active` 一拍选中一套上下文 —— 直接预演「超线程＝多一套寄存器、共享执行单元」。
软硬两路输出一致：`CTX_SWAP_PASS` / `SHARE_PASS` / `SCHED_PASS` / `ALL_PASS`。

思考题（`essay/THINKING.md`）：见 §4。

```
labctl run improper/04-thread     # 跑全部变体
labctl watch                      # 边改边自动判定
labctl hint improper/04-thread    # 卡住看提示
```

## 3. 完成标准 (DoD)

- [ ] `CTX_SWAP_PASS`：能整存整取一个 `Context`（GPRs＋少量 CSR），切换后执行流与全部寄存器随上下文整体迁移。
- [ ] `SHARE_PASS`：同进程两线程共享同一 fd/内存可见。
- [ ] `SCHED_PASS` + `ALL_PASS`：调度器轮流切换多个 TCB 且各自上下文独立。
- [ ] 重构后 TCB 仅含「上下文 ＋ 指向 Process 的指针」，进程资源（mem/fd/sem）抽到独立共享结构，无重复存储。
- [ ] （辅助）`hw-v`/`hw-bsv` 双上下文寄存器文件 0 warning 通过，输出与软件一致。
- [ ] 思考题说清「硬件线程共享什么、软件线程共享什么」及各自省下的开销。

## 4. 思考题（`essay/THINKING.md` 作答）

1. **硬件物理线程（超线程）**：一个物理核同时跑 2 个硬件线程，必须**每线程各复制一套**的是什么
   （PC/GPRs/CSR 等 architectural state）？被两线程**共享**的又是什么（取指译码后端、ALU/FPU、L1/L2、TLB、分支预测器）？
   为什么共享能加速（一个线程访存 stall 时让另一线程顶上，掩盖延迟、抬吞吐）？省下了哪些开销？
2. **软件进程/线程**：同进程多线程共享地址空间/fd/堆，独占栈与寄存器上下文。
   为什么「切线程」远比「切进程」便宜（省切 `satp` 等 CSR、免 TLB flush、共享数据免拷贝/免 IPC）？
   再给一个「共享反而成负担」的场景（数据竞争要加锁 / 缓存行伪共享）。
3. **（可选）对照两套「共享」**：OS 软件线程共享 fd，对应硬件线程共享的是什么（TLB/cache/执行单元）？
   为什么两者都遵循同一条省钱原则——「复制最少的执行身份，共享最贵的资源」？

## 5. 简化取舍（简化的是学生负担，非功能完整性）

- 用单线程「虚拟 CPU」＋协作式 `ctx_switch()` 模拟切换，不跑裸机、不触发 trap；真实 `__switch.S` 保存
  `ra/sp/s0-s11`＋`sstatus/sepc`、时钟中断驱动抢占，留作引申。
- TCB 只存上下文＋指针，资源用 `Rc/Arc`（C 用指针）共享；不实现真正地址空间隔离与页表，
  「内存」用共享数组、fd 表用数组、信号量用计数器占位。
- 「并发」靠顺序 `switch` 的交错体现，不引入多核真并行。
