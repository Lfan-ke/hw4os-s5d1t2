# 20 · 异步事件：信号 —— handler 表、pending 位集、mask 与「补投递」

> 不正经赛道 · 第 20 课 —— 纯软件建模一个「进程」的信号机制，host 直接跑。
> 一句话母题：**信号就是「软件版的中断」——有人随时拍你肩膀。你事先登记好「拍到该干嘛」
> (handler)；正忙时拍来的先记一笔挂着 (pending)；你也能主动说「这会儿别烦我」(mask)，
> 等忙完再把攒下的补送过来 (补投递)。**

## 0. 这节课在讲什么

「异步事件」是 OS 里一个绕不开的概念：事件**与当前执行流无关、随时插进来**。信号是它在
**用户态**最朴素的化身——本质就是内核搭在硬件中断之上的一层软件抽象。本课不追求严谨，
只让你亲手搭出那套人人都该有的**心智模型**：

```
一个「进程」 = handlers[NSIG]  (信号表：每号登记一个回调，None=默认忽略)
             + pending (u32)   (挂起位集：投递不出去、攒着的信号)
             + mask    (u32)   (屏蔽位集：被你主动挡住的信号)
```

两个动作把它们盘活：
- `raise(sig)`：**被屏蔽就入 pending；否则立刻跑 handler（投递）。**
- `unmask(sig)`：**解屏蔽，并把这期间攒下的 pending「补投递」出去。**

对应真实系统：`handlers` ↔ `sigaction` 注册表；`pending`/`mask` ↔ 内核 PCB 里的
`sigpending`/`sigmask`，也 ↔ 硬件 `mip`/`mie`；「运行期间自动屏蔽本信号、不重入」↔
`sigaction` 默认语义；「pending 是位、合并不排队」↔ 标准信号 vs 实时信号。

## 1. 你要填的 2 个函数

软件在 `sw/rust/src/main.rs` 或 `sw/c/signal.c`。`run_handler`（真正把 handler 跑起来、
含可重入守卫与返回补投递）与测试 harness 都已给定，**你只填 `raise` 与 `unmask`**：

| 子实验 | 你填的逻辑 | 要求 | 判据 |
| :-- | :-- | :-- | :-- |
| 1 投递 | `raise` 的「未屏蔽分支」 | 注册 handler 后 raise → handler 真的跑、改了标志 | `DELIVER_PASS` |
| 2 屏蔽 | `raise` 的「屏蔽分支」 | 先 mask 再 raise → handler 不跑、信号进 pending | `MASK_PASS` |
| 3 补投递 | `unmask` 全部 | unmask → 把攒下的 pending 补投递出来 | `PENDING_PASS` |
| 4 可重入 | 上面两者协同 | handler 执行中再来同号 → 合并/不丢、绝不递归重入崩 | `REENTRANT_PASS` |

四段皆过再打印 `ALL_PASS`。失败会打印**不含 `FAIL`** 的诊断行：`*_MISS`（该发生没发生，
如 `DELIVER_MISS` raise 后 handler 没跑）/`*_BAD`（不该发生却发生，如 `MASK_BAD` 屏蔽期间
没进 pending、`REENTRANT_BAD` 递归重入了）。

```
labctl run improper/20-signal      # 跑 rust/c 两条路径
labctl watch                       # 边改边自动判定
labctl hint improper/20-signal     # 卡住看提示
```

### 关键约定（判题用）

- `raise(sig)`：`is_masked(sig)` ⇒ `pending |= bit(sig)`（入队）；否则 `run_handler(sig)`（投递）。
- `unmask(sig)`：清 `mask` 位；若 `is_pending(sig)` 则清 `pending` 位并 `run_handler(sig)`（补投递）。
- **pending 是「位」不是「计数」**：屏蔽期间同号 raise 多次，只留一个 pending（合并、丢重复）。
- 可重入由 given 的 `run_handler` 守卫兜底：handler 运行中再来同号 → 经 `raise→run_handler`
  被 `in_handler` 拦下入 pending，**绝不递归重入**（`max_depth` 恒为 1），返回后补投递一次。

## 2. 完成标准 (DoD)

- [ ] 至少一条变体（rust 或 c）跑出 `DELIVER_PASS`/`MASK_PASS`/`PENDING_PASS`/`REENTRANT_PASS`/`ALL_PASS`，无任何 `*_MISS`/`*_BAD`（必修）。
- [ ] rust `cargo run -q` 0 warning；c `gcc -Wall -Wextra -O2` 0 warning。
- [ ] 另一条语言路径也过计辅助分（双语言都过有奖励）。
- [ ] essay 子题答出「异步/async-signal-safe/signal vs sigaction/实时信号排队/与中断类比」的要点。
- [ ] 能口述 `pending`/`mask` ↔ 内核 `sigpending`/`sigmask` ↔ 硬件 `mip`/`mie` 的对应。

## 3. 思考题（`essay/THINKING.md` 作答即可通过）

1. 什么是「异步事件」？信号为什么需要 handler 表 + pending + mask 这套机制？
2. 什么是 async-signal-safe？为什么 handler 里只能干极少的事（不能 `malloc`/`printf`）？
3. `signal()` 与 `sigaction()` 有何区别？为什么现代代码该用后者（`sa_mask`/`SA_RESTART`/`SA_SIGINFO`）？
4. 「标准信号」与「实时信号」在排队上有何本质区别？本实验建模了哪一种？怎么升级成另一种？
5. 信号与硬件中断有哪些类比、又有哪些关键不同（触发主体/特权态/投递时机/排队粒度）？

## 4. 引申

- **self-pipe trick**：handler 只 `write` 一个字节到管道，主循环用 `select`/`epoll` 等它——
  把「异步信号」转成「同步可等待的 fd 事件」，绕开 async-signal-safe 的雷区。
- **signalfd / pidfd**：Linux 让你把信号当文件描述符来读，进一步把信号纳入统一的事件循环。
- **真实内核路径**：信号在进程**从内核返回用户态**前被投递（检查 `sigpending & ~sigmask`），
  内核在用户栈上伪造一个帧跳到 handler，handler 结束经 `sigreturn` 回到被打断处——
  与本模型「择机 run_handler、跑完回到原流程」一一对应。
