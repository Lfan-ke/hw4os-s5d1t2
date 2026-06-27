# 正经·S3 · 内核形态认知（unikernel / RTOS / exokernel）

> 承接 S1（已能引导进 S 态）。本课不碰新硬件，而是**在同一个 S 态内核里**演示三种经典「内核形态」的心智模型，并亲手补完一个最小协作式 RTOS。

## 0. 这节课在讲什么

「内核」并不是只有 Linux 那一种宏内核样子。同一段 CPU 代码，按**应用与内核如何划界**，可以长成很不一样的形态。本实验在一个镜像里依次跑三个最小 demo：

1. **库核心 / unikernel**：应用被链接进内核镜像、与内核**同地址空间**，没有特权边界。应用要用内核服务时不发 `syscall`、不触发 trap，**直接函数调用**即可。打印 `UNIKERNEL_PASS`。
2. **极简 RTOS**：一组**静态任务**共享单 CPU，靠**协作式 `yield`** 轮转（无定时器、无抢占）。一个对静态任务表做轮询的调度器，就是微型 RTOS 的内核。打印 `RTOS_PASS`。
3. **外核 / exokernel**：内核**只做安全多路复用**——经过边界检查把资源**句柄**发给上层；真正的「抽象」（这里是一个字符串写入器）放在 **libOS** 函数里。打印 `EXOKERNEL_PASS`。

三个都过 → `ALL_PASS`。

## 1. 你要实现的（`kernel/rtos.c`，两处 TODO）

unikernel 与 exokernel 已在 `kernel/main.c` 给好；RTOS 调度器由你补完。

- **TODO(1) `rtos_yield()`**：把 CPU 交还调度器。
  ```
  __switch(&task_ctx[current], &sched_ctx);
  ```
- **TODO(2) `rtos_run()`**：对静态任务表做轮询，循环把 CPU `__switch` 给每个 `READY` 任务，直到全部 `EXITED`。
  ```
  for (;;) {
      int alive = 0;
      for (i = 0..NTASK-1) if (task_state[i]==READY) { alive++; current=i; __switch(&sched_ctx, &task_ctx[i]); }
      if (alive == 0) break;
  }
  ```

`__switch`（`kernel/switch.S`，已给）只保存/恢复被调用者保存寄存器 `ra/sp/s0-s11`——这刚好完整描述一个栈帧，于是控制流能回到对方上次 `__switch` 的位置；新任务的 `ctx.ra=task_bootstrap`、`ctx.sp=该任务栈顶`（`task_create` 已给）。

3 个任务各 `yield` 3 轮，正确轮转的运行顺序应为 `0 1 2 0 1 2 0 1 2`，校验通过即打印 `RTOS_PASS`。

```
labctl run proper/S3-kernel-forms
make -C kernel run     # 手动跑（OpenSBI banner 后见内核输出）
```

判据：输出含 `UNIKERNEL_PASS` / `RTOS_PASS` / `EXOKERNEL_PASS` / `ALL_PASS`，不出现 `FAIL` / `UNEXPECTED`。未补完时（RTOS 未实现）只出 `UNIKERNEL_PASS` 与 `EXOKERNEL_PASS`（缺中间的 `RTOS_PASS`），无 `ALL_PASS`。

## 2. 完成标准 (DoD)

- [ ] 补完 `rtos_yield` / `rtos_run`，任务轮转顺序正确、调度器在全部任务退出后返回、内核正常关机。
- [ ] 能说清三种形态的边界差异：unikernel（无边界，调用替代 syscall）/ RTOS（协作式让出，无抢占）/ exokernel（内核只安全多路复用，抽象上移 libOS）。
- [ ] 理解 `__switch` 为何只存 callee-saved 即可完成上下文切换，以及新任务 `ctx` 如何「伪造」首帧。

## 3. 引申

- 协作式 → 抢占式：接上 S2 的时钟中断，在 trap 里触发 `__switch` 即得分时（见 S5/S8）。
- exokernel 的「安全多路复用」推到极致就是 unikernel-on-exokernel：libOS 把 OS 抽象全搬到用户态。
- 把 `yield` 换成 `await` + 状态机，就是 S4 的无栈协程 / async runtime。
