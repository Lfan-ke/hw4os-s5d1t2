# 正经·S4 · embassy 式异步运行时（内核内）

> 承接 S2（trap/timer 已通）。本课在 S 态内核里手搓一个 **embassy 风格的无栈异步运行时**：
> `Future = 带状态的结构体 + poll()`、一个轮询任务队列的最小 executor、以及一个靠时钟中断
> 推进的「延时 Future」（用 waker/重新入队完成）。

## 0. 这节课在讲什么

无栈协程的肉身就是一台**被反复 poll 的状态机**：
- `poll(self)` 返回 `Pending`（还没好，让出）或 `Ready`（好了，带产出值）。
- 凡是「跨让出点还要活着」的局部，都塞进任务结构体（`struct Task` 的 `state/n/wake_tick`），
  **不放在函数栈上**——这就是「无栈」：有栈协程换的是栈指针，无栈协程换的是**状态号**。
- **executor** 轮询就绪队列：`Pending` 的任务等唤醒后重新入队，`Ready` 的出队丢弃。
- **延时 Future**：未到点就返回 `Pending` 并登记到 timer reactor；时钟中断（复用 S2 的
  `trap_handler`）推进 `g_ticks`，reactor 到期把任务**重新入就绪队列**——这正是 `waker` 的本质。

> **退化顿悟**：如果所有 poll 都「一次就 Ready / 从不让出」，executor 就退化为**顺序批处理**
> （取一个跑完才轮下一个，输出 `AABBCC` 而非交错的 `ABCABC`）。要交错，poll 必须在让出点返回
> `Pending`。这与纤程课「无让出 = 顺序执行」是同一条道理，只是这里「让出 = 返回 Pending」。

## 1. 你要实现的（`kernel/async.c` 三处 `// TODO`）

1. **`count_poll`**（状态机推进）：
   ```
   若 state < n：state++; 打标签(trace_emit(label)); want = WAKE_NOW; 返回 PENDING
   否则：out = magic; 返回 READY
   ```
2. **`delay_poll`**（延时 future）：
   ```
   若 g_ticks >= wake_tick：out = g_ticks; 返回 READY
   否则：want = WAKE_TIMER; 返回 PENDING
   ```
3. **`exec_run`**（executor 调度循环）：
   ```
   while 就绪队列非空 或 reactor 非空：
       while 就绪队列非空：
           t = ready_pop; r = t->poll(t);
           若 r==PENDING：want==WAKE_NOW → ready_push 重新入队；否则 → timer_register 挂 reactor
           // r==READY：任务完成，丢弃
       若 reactor 非空：asm volatile("wfi")(等时钟中断); reactor_wake_due(唤回到期任务)
   ```
   就绪队列原语（`ready_push/ready_pop`、`timer_register`、`reactor_wake_due`）已给定，直接调用即可。

`kernel/main.c`（harness，勿改）跑三个场景并判定；`kernel/trap.c`（给定，复用 S2）让时钟中断累加 `g_ticks`。

```
labctl run proper/S4-async
make -C kernel run     # 手动跑（OpenSBI banner 后见内核输出）
```

判据：输出含 `POLL_PASS` / `EXEC_PASS` / `TIMER_FUTURE_PASS` / `ALL_PASS`，不出现 `FAIL`/`UNEXPECTED`。

⚠️ `exec_run` 里就绪队列空且 reactor 非空时**必须** `wfi` 等中断再 `reactor_wake_due`，否则要么
busy-poll 空转、要么没人推进 `g_ticks` 而死循环（超时判 FAIL）。

## 2. 三个判据

- **POLL_PASS**：单个计数 future 直接手动 poll，Pending 推进 3 次后 Ready，产出值正确。
- **EXEC_PASS**：3 个让出任务进 executor，round-robin 交错输出 `ABCABC`（而非批处理 `AABBCC`）。
- **TIMER_FUTURE_PASS**：延时 future 等 3 拍时钟，executor 内部 `wfi` 等中断、reactor 到期唤醒后完成。

## 3. 完成标准 (DoD)

- [ ] `poll()` 把「进度」存进结构体而非栈，能 Pending/Ready 推进状态机。
- [ ] executor 轮询多任务、让出点交错调度；Ready 出队。
- [ ] 延时 future 靠时钟中断 + waker 重新入队完成，不忙等空转。
- [ ] 能说清「无让出 → 退化为顺序批处理」，以及 embassy 式 reactor/waker 的角色（essay）。

## 4. 引申

- 真实 `Pin`/自引用 Future、`RawWaker` vtable、跨 `await` 借用；多个外设各自的 reactor。
- 把本 executor 接入 S5 调度器（协程任务参与统一调度）；为 S14 异步 IPC 提供运行时底座。
