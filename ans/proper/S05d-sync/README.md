# 正经·S05d · 内核阻塞同步原语：互斥锁 / 信号量 / 条件变量

> 承接 S05（协作式 `__switch` 任务）。本课在「协作任务运行时」之上，
> 加一个 **BLOCKED 态 + 阻塞/唤醒**，造出 rcore ch8 的三种**阻塞式**同步原语。
> 核心对照：**阻塞 vs 自旋**（自旋见 S15）——被阻塞的任务**零 CPU 占用**。

## 0. 这节课在讲什么

多个执行流共享数据，就要协调“谁能进临界区 / 谁该等”。协调有两条路：

- **自旋（spin）**：抢不到就原地打转反复试（`while (locked) {}`）。简单、无切换开销，但**空转烧 CPU**——临界区一长，等待者就把一个核白白耗光。适合多核、临界区极短（S15 自旋锁）。
- **阻塞（block）**：抢不到就**把自己挂到等待队列、让出 CPU、睡过去**；持有者释放时**唤醒**一个等待者。等待者在睡着期间**完全不被调度、零 CPU 占用**。适合单核、或临界区较长。这正是 rcore ch8 内核同步原语的做法。

本课用 S05 的 `__switch` 搭一个最小协作运行时，给每个任务加 `READY / RUNNING / BLOCKED / EXITED` 态，并提供两个调度原语：

```
sched_block()  ：当前任务 → BLOCKED，__switch 走人；被唤醒前调度器绝不选中它（零 CPU）
sched_wake(id) ：把 BLOCKED 的任务 → READY（重新可被调度，但不立即切过去）
```

在它们之上，三种原语都用同一对**阻塞核心 / 唤醒核心**实现：

```
wq_block(wq)    ：把当前任务入等待队列 wq + sched_block()   —— 抢不到资源时调用
wq_wake_one(wq) ：从 wq 出队一个等待者 + sched_wake(id)     —— 释放资源时调用
```

- **Mutex**：`locked` 标志 + 等待队列。`lock` 若被占就 `wq_block`，`unlock` 置空并 `wq_wake_one`。
- **Semaphore**：`count` 计数 + 等待队列。`down` 若 `count==0` 就 `wq_block`，否则 `count--`；`up` 让 `count++` 并 `wq_wake_one`。
- **Condvar**（配一把 mutex）：`wait` = 放锁 → `wq_block` → 醒来重新拿锁；`signal` = `wq_wake_one`。

## 1. 你要实现的

`kernel/sync.c` 里的两个核心（其余外壳、三个测试、运行时全给定）：

- **阻塞核心 `wq_block`**：把 `cur_task()` 入等待队列（`ids[tail]`、`tail` 取模推进、`count++`），再 `sched_block()`（置 BLOCKED + 让出 CPU）。被唤醒后从这里返回，回到调用方的 `while` 重检条件。
- **唤醒核心 `wq_wake_one`**：若队列非空，出队队头 `id`（`head` 取模推进、`count--`），`sched_wake(id)` 置回 READY，返回 `id`；空则返回 `-1`。

`mutex_lock/unlock`、`sem_down/up`、`condvar_wait/signal` 的外壳都已写好，它们调用上面两个核心。

## 2. 判据

输出含四段：

- `MUTEX_PASS`：两任务在锁内对共享计数器各加 `N` 次，结果精确等于 `2N`（无丢更新），**且确有任务真阻塞**（`g_block_events>0`，不是自旋）。
- `SEM_PASS`：`empty/full` 两信号量 + 一把 mutex 的有界缓冲，16 件产品穿过容量 4 的环，内容与顺序完全对上。
- `CONDVAR_PASS`：mutex + 条件变量再做一遍生产者-消费者，消费者条件不满足则 `wait`、生产者 `signal` 唤醒。
- `ALL_PASS`：三项全过。

不得出现 `FAIL` / `panic` / `UNEXPECTED`。诊断用 `*_MISS`。

> 为什么判据要看 `g_block_events>0`？因为「结果对」还不够——把阻塞换成自旋让出（`sync_yield`）数据也能搬对，但那不是阻塞同步。`g_block_events` 统计「任务进入 BLOCKED 态」的次数，自旋恒为 0。这就是「阻塞 vs 自旋」的可观测分界。

```
cd kernel && make kernel.elf
make run      # OpenSBI banner 后见内核输出；跑完内核 k_shutdown 退出 qemu
```

## 3. 完成标准 (DoD)

- [ ] `wq_block` 正确入队 + 置 BLOCKED + 让出；被唤醒后能从原处继续。
- [ ] `wq_wake_one` 正确出队 + 置 READY；空队列返回 -1。
- [ ] 三种原语全部 `*_PASS`，且 `g_block_events>0`（真阻塞，零 CPU 占用）。
- [ ] 能说清「阻塞 vs 自旋的取舍」「条件变量为何要配锁、为何用 `while` 重检」「信号量/互斥锁/条件变量语义区别」。

## 4. 引申（怎么扩成完整版）

- **可中断 / 带超时的等待**：给 `wq_block` 加超时（到点由时钟中断唤醒、`down` 返回失败），或让信号打断等待——即 `mutex_timedlock` / 可被 `EINTR` 的阻塞。
- **优先级等待队列**：等待队列从 FIFO 换成按优先级出队（高优先先醒），平级 FIFO 兜底；并直面**优先级反转**——低优先持锁、高优先阻塞等它，中优先却抢跑，需要**优先级继承**。
- **读写信号量 / 读写锁**：区分读者（可并发）与写者（独占），用计数 + 两个队列实现。
- **对照 S15 自旋锁与 futex**：单核协作下本课无需原子指令；多核抢占下，`locked/count` 的读改写本身要用 `amo`/CAS 保护（自旋锁），而真实 OS 的 `futex` 正是“先自旋几下、抢不到再陷入内核阻塞”的二者合一。
- **抢占式**：接 S02 时钟中断，让阻塞/唤醒在任意指令处也安全——需要在临界区关中断或用原子操作保护原语自身的元数据。
