# 06 · 无栈协程：被 poll 出来的「状态机」绿色线程

> 不正经赛道 · 第 6 课 —— 纯软件、host 直接跑。
> 一句话母题：**有栈协程换的是栈指针，无栈协程换的是状态号**。
> 一个无栈协程，本质就是一台**被反复 poll 的状态机**。

## 0. 这节课在讲什么

上一课（`05-fiber`）的纤程靠「每人发一根栈、换人就换栈指针」实现暂停/恢复；这一课我们把**栈也省了**。
你 `poll` 协程一下，它跑到下一个让出点就停住，把「现在停在第几步」记在自己结构体里；再 `poll` 又接着跑。
你将亲手把一段顺序代码「掰成」一台状态机，然后发现：这正是 Rust `async/await`、C++20 协程、C 的
protothreads 在背后偷偷做的事，也正是第 01 课那个「软件能做、硬件也能做」的 FSM。

主线：**顺序代码 → 状态机 → 谁来生成这台状态机**（你手写 / C 宏 / Rust 编译器 / 一块硬件 FSM）。

对应真实系统：Rust `Future`+`Waker`+`Executor`、tokio/embassy、C++20 `co_await`、Python `generator`、
C protothreads（Contiki）、nginx 事件循环。对照组：Go goroutine、Lua coroutine 是**有栈**的。

## 1. 数据模型

```
enum Step { Pending(u32), Ready(u32) }     // 让出一个值 / 结束并给终值
struct Stepper { seed, start, step, count,  // 配置
                 i, acc, done }             // ← 跨让出点存活的局部，塞进 struct（这就是「无栈」）
```

一台 `Stepper` 让出 `start, start+step, …` 共 `count` 次，终值 = `seed + 让出值之和`。
状态号就是 `i`：`poll` 一次推进一步。

## 2. 你要填的函数（`sw/rust/src/main.rs` 或 `sw/c/coroutine.c`）

| 子实验 | 函数 | 要求 | 判据 |
| :-- | :-- | :-- | :-- |
| 06.1 手写状态机 | `poll` / `co_poll` | i<count 让出并累加；i==count 收尾 | `YIELD_PASS` `STATEMACHINE_PASS` |
| 06.2 协作执行器 | `exec_run` | round-robin 轮询，记交错让出 + 完成终值 | `EXEC_PASS` `BATCH_PASS` |
| 06.3 就绪与唤醒 | `reactor_run` | 只重 poll 被唤醒者，统计 poll/wake | `WAKER_PASS` |
| 06.4 async（rust）| `stepper_async` | 用 `async/await` 重写 06.1，跑在自写 executor 上 | `ASYNC_PASS` `JOIN_PASS`（辅助分）|

C 没有 `async/await`（这正是要点）：06.4 在 C 侧只能停在函数指针「状态机库」`lib_run`（产出辅助分
`LIB_PASS`），「编译器替你生成状态机」的体验留给 Rust 版与 `essay/THINKING.md`。

```
labctl run improper/06-coroutine     # 跑 rust / c / essay 三条路径
labctl watch                         # 边改边自动判定
labctl hint improper/06-coroutine    # 卡住看提示
```

## 3. 完成标准 (DoD)

- [ ] `YIELD_PASS` / `STATEMACHINE_PASS`：手写状态机吐出的让出序列逐位一致，终值正确，poll 次数恰为 count+1。
- [ ] `EXEC_PASS`：多协程交错推进；`BATCH_PASS`：无让出 → 退化为顺序批处理（无交错）。
- [ ] `WAKER_PASS`：总 poll 次数 ≤ 唤醒数 + 任务数（无忙等）。
- [ ] （rust 辅助分）`ASYNC_PASS` / `JOIN_PASS`：`async/await` 版与手写版逐位一致，并能说出二者同构。
- [ ] 三者皆过再打印 `ALL_PASS`。rust / c 任一条全过即必修达标；另一条 + essay 计辅助分。
- [ ] 能用自己的话讲清「无栈 vs 有栈」省/付各在哪（`essay/THINKING.md`）。

## 4. 思考题（`essay/THINKING.md` 作答即可通过）

1. 为什么无栈协程也叫「绿色线程」？相比陷入内核的线程切换，它在用户态省掉了哪些开销
   （特权级切换 / 内核栈 / TLB·缓存抖动 / 调度器锁 / TLS）？又因「无栈」额外省掉了什么（每任务一根独立栈）？
2. 无栈 vs 有栈：无栈为什么**不能从任意深的调用栈中间** yield（联系函数染色 / red-blue function，
   `async` 会沿调用链传染）？有栈为什么内存更大、却编程模型更自然？各举一个更合适的场景。
3. 「无栈协程 = 状态机 = 硬件 FSM」——把 06.1 手写 enum 状态机、06.4 编译器生成的 Future、
   以及硬件 FSM 三者并排，它们是同一台机器吗？若让芯片「流片成一个协程」，每个时钟沿就是一次 `poll`，
   那 `Pending` / `Ready` 对应硬件的什么信号？
