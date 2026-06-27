# 正经·S5 · 协作式多任务与调度器

> 承接 S2（trap/timer）。本课造出“单核 OS 的心脏”：**任务上下文 + `__switch` + 任务表 + 调度器**。
> 这是 rcore ch3 的核心——多个任务在一个核上轮流上 CPU，靠协作式 `yield` 让出。

## 0. 这节课在讲什么

一个执行流的“身份”就是一组寄存器。要在两个任务间切换，只需把当前任务的寄存器存好、把下一个任务的寄存器装回。协作式切换（任务主动让出、无时钟抢占）甚至只需保存 **callee-saved 子集**（`ra/sp/s0-s11`），因为 caller-saved 寄存器早被编译器在每个 `call` 处替你溢出到栈上了。

```
struct TaskContext { uint64_t ra; uint64_t sp; uint64_t s[12]; };  // 14 个 u64
__switch(cur, next): 把 callee-saved 存进 *cur，从 *next 装回，ret 到 next.ra
```

调度器 `schedule()` 维护一张任务表（就绪/运行/退出），用 **round-robin** 轮流挑下一个就绪任务并 `__switch` 过去。3 个任务各 `yield` 数次，运行序应是 `0 1 2 0 1 2 0 1 2`。

## 1. 你要实现的

- **`kernel/switch.S` 的 `__switch` 体**：保存 `cur(a0)` 的 callee-saved，恢复 `next(a1)` 的，`ret`。
  偏移：`ra=0, sp=8, s0=16, s1=24, ..., s11=104`（各 8 字节）。
- **`kernel/sched.c` 的 `schedule()`**：从 `(current+1)%NTASK` 起环形扫描，选第一个 `READY` 任务切过去；若全部退出，`__switch` 回 `boot_cx` 结束调度循环。

`main.c`（测试驱动）、任务体、`run_tasks`、上下文切换自检均已给定。

## 2. 判据

输出含三段：
- `SWITCH_PASS`：上下文切换自检——来回切一趟回到原点，`probe` 设置的魔数被正确带回（“回来值对”）。
- `SCHED_PASS`：3 任务轮转，记录到的运行序 == `0 1 2 0 1 2 0 1 2`。
- `ALL_PASS`：全部通过。

不得出现 `FAIL` / `panic` / `UNEXPECTED`。

```
cd kernel && make kernel.elf
make run      # OpenSBI banner 后见内核输出；跑完内核 k_shutdown 退出 qemu
```

## 3. 完成标准 (DoD)

- [ ] `__switch` 正确整存整取 callee-saved，切换后控制流落到 `next.ra`、用 `next.sp`。
- [ ] 新任务首次被调度能落进任务体（`ra=task_body`、`sp=` 任务栈顶）。
- [ ] `schedule()` 轮转正确，运行序符合 round-robin。
- [ ] 能说清“为什么协作式只需保存 callee-saved”“`__switch` 返回到哪里”。

## 4. 引申

- 把协作式换成**抢占式**：接 S2 的时钟中断，在 `trap_handler` 里调 `schedule()`，任务无需主动 `yield`。
- 把 round-robin 换成**优先级/优先队列**调度（高优先先出队），平级 FIFO 兜底。
- 给每个任务一份完整 `TrapContext`（进 U 态运行），即 rcore ch3→ch4 的演进。
