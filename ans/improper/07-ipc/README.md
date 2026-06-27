# 07 · 进程通信：原子操作、锁与「A 等 B 置位」的完成握手

> 不正经赛道 · 第 7 课 —— 软/硬同构，host 直接跑（硬件走 iverilog/bsc）。
> 一句话母题：**两个进程怎么对话？最朴素的方式不是发消息，而是共用一块小黑板——
> B 干完活画个勾(置 `DONE`)，A 死盯黑板，看见勾了才接着干。**

## 0. 这节课在讲什么

进程通信的地基不是「消息队列」，而是**共享状态 + 原子位**。本课让你亲手发现：
这块「黑板」如果不是原子地涂改，两个人就会打架（竞态）；而硬件里一个 `DONE` 位，
不过就是一根 B 拉高、A 采样的线。你用**软/硬四种写法**实现同一套「涂黑板」协议。

沿用 VLAN 的「控制字」同构思路，32-bit：

```
[31]BUSY  [30]DONE  [29]LOCK  [28]START  [15:0]RESULT
```

对应真实系统：`DONE` ↔ xv6 `sleep/wakeup`、rcore `Condvar`、MMIO 设备 done bit、
virtio notify、父进程 `wait` 收割 `ZOMBIE` 子进程；`test_and_set` ↔ RISC-V
`amoswap`/`lr.sc`、x86 `xchg`、xv6 `acquire`；计数信号量 ↔ Dijkstra P/V、rcore
`Semaphore`。

## 1. 你要填的 8 个纯函数

软件在 `sw/rust/src/main.rs` 或 `sw/c/ipc.c`；硬件是同一逻辑的组合块，
在 `hw/v/ipc_proc.v`（填 `always` 块）或 `hw/bsv/IpcProc.bsv`（填 `ipc_op` 函数）。
硬件把 8 个原语合成一个纯组合「IPC ALU」：`op(0..7)` 选原语，`(a,b)→y`。

| 子实验 | 函数 | 要求 | 判据 |
| :-- | :-- | :-- | :-- |
| 1 握手 | `b_finish` / `a_poll` | B 置 DONE+RESULT；A 仅 DONE=1 才 ready 取数 | `HANDSHAKE_PASS` |
| 2 锁   | `tas` / `unlock` / `try_lock` | 原子读旧写新；占用中不可双进入 | `TAS_PASS`+`MUTEX_PASS` |
| 3 信号量 | `down` / `up` | down 减一、ok=(count'>=0)；up 加一 | `SEM_PASS` |
| 4 编排 | `a_step` / `b_step` | A 门铃→等 DONE→post=result*2；B 见门铃算并置位 | `ORCH_PASS` |

四段皆过再打印 `ALL_PASS`。失败会打印含 `FAIL` 的行（如 `EARLY_FAIL` A 提前就绪、
`DOUBLE_ENTER_FAIL` 锁被双进入）。`NAIVE_RACE` 是给定的「非原子读改写丢更新」对照，
仅作演示，不计判据。

`// TODO[a]` / `// ELSE[b]` 是择一分支（如 `down` 的阻塞式/自旋式、`a_step` 的
先用后清 DONE / 先清后用），两条都能过同一断言。

```
labctl run improper/07-ipc      # 跑 rust/c/v/bsv 四条路径
labctl watch                    # 边改边自动判定
labctl hint improper/07-ipc     # 卡住看提示
```

## 2. 完成标准 (DoD)

- [ ] 至少一条变体跑出 `HANDSHAKE_PASS`/`TAS_PASS`/`MUTEX_PASS`/`SEM_PASS`/`ORCH_PASS`/`ALL_PASS`，无任何 `*_FAIL`（必修）。
- [ ] 软/硬实现对同一向量逐位一致；其余路径也过计辅助分（跨轴：软+硬都过有奖励）。
- [ ] 硬件路径 0 warning（`iverilog -g2012 -Wall` / `verilator --lint-only -Wall`）。
- [ ] essay 子题答出「内存序、自旋 vs 阻塞、A 为何等 B」的要点。
- [ ] 能口述 `DONE` 位 ↔ rcore `Condvar`/xv6 `wakeup`、`tas` ↔ `amoswap` 的对应关系。

## 3. 关键约定（判题用）

- B 端协议：**先写 `RESULT` 再置 `DONE`**（release）；A 端**见 `DONE` 才取数**（acquire）。
  顺序反了 A 就会读到垃圾 → `EARLY_FAIL`。
- `tas(lock)`：新值恒为 `1`，`got = (lock == 0)`；`try_lock` 调 `tas` 写回锁并返回 `got`。
- `down(count) = (count-1, count-1>=0)`；`up(count) = count+1`。harness 只在 `ok=true` 时
  更新 `count`，故阻塞式/自旋式两种写法等价。
- 并发用**确定性交错调度模拟**：harness 喂一条给定的 step 序列（而非真抢占），
  把竞态/原子性的心智模型保留下来，免去非确定性并发的调试地狱。软件握手另用
  `std::thread`+原子（Rust）/`pthread`+`stdatomic`（C）跑一遍真·两执行流。

## 4. 引申（可扩展性：从「涂黑板」原语到真实同步设施）

本课把 IPC 的地基压缩成 8 个纯函数 + 一块 32-bit 控制字，并用**确定性交错调度**模拟并发（而非真抢占），把竞态/原子性的心智模型留下、把非确定性并发的调试地狱省掉。想把它推向真实内核，按兴趣选：

1. **自旋锁升级为阻塞锁**：本课 `tas` 拿不到锁只能反复试（自旋）。实现一条「拿不到就挂到等待队列、`up` 时 `wakeup`」的阻塞路径，对照 rcore `MutexSpin` vs `MutexBlocking`、Linux **futex**（用户态快路径 + 内核态慢路径）——这正是思考题 2 的落地。
2. **从 `test_and_set` 到 `lr.sc`/CAS**：`tas` 在 SMP 下有「拿到锁前的总线争用」问题。换成 RISC-V `lr.sc`（load-reserved/store-conditional）或 `amoswap`，实现 **CAS** 并用它搭无锁栈/队列，体会 ABA 问题与 `compare_exchange`；再加 `MCS`/`ticket` 锁解决自旋锁的公平性与缓存行乒乓。
3. **补内存序**：本课「先写 RESULT 再置 DONE」是手工保证的顺序（思考题 1）。在软件侧真的用 `Ordering::Release/Acquire`（Rust）/ `atomic_thread_fence`（C），并把顺序故意写反观察 `EARLY_FAIL`，理解 release-acquire 与 `fence` 在弱内存模型（ARM/RISC-V）下的必要性。
4. **DONE 位升级为真设备/中断**：把 A 的「死盯黑板」轮询改成中断驱动——B（设备）置 `DONE` 后触发中断，A 在 ISR 里被唤醒。对照 virtio notify、MMIO done bit、xv6 `sleep/wakeup`，体会 busy-poll vs 中断的取舍。
5. **从共享内存到消息传递**：本课是「共享状态 + 原子位」的最朴素 IPC。在其上实现一个有界环形缓冲的 **pipe/channel**（生产者-消费者 + 计数信号量），再对照「共享内存 IPC」与「消息传递 IPC」（Mach port / seL4 IPC / Go channel）两种范式的安全性与性能差异。
6. **真·两执行流压测**：把确定性交错换成真 `std::thread`+原子 / `pthread`+`stdatomic` 跑大量迭代，用 `NAIVE_RACE`（非原子读改写）复现真实丢更新，再用你的原子版修复——把「为什么需要原子」从模拟变成可观测的数据竞争。
7. **硬件侧做成带握手的 FIFO**：把组合「IPC ALU」扩成时序电路，给 `DONE`/`BUSY` 配 valid-ready 握手，做一个跨时钟域的同步 FIFO，体会硬件里「原子置位」靠的是单 bit 寄存器不可能被读到中间态（思考题 3）。

## 5. 思考题（`essay/THINKING.md` 作答即可通过）

1. 为什么 B 必须「先写 `RESULT` 再置 `DONE`」？若反过来，A 可能读到什么垃圾值？（内存序/可见性、`fence`、release-acquire）
2. 自旋锁与阻塞锁各自浪费/节省了什么？临界区很短/很长、单核/多核分别该用哪个？（rcore `MutexSpin` vs `MutexBlocking`）
3. 单核可靠「关中断」得原子性，为什么 SMP 必须用 `lr.sc`/`amoswap`？硬件那一个 `DONE` 位的「原子置位」又靠什么不被读到中间态？
