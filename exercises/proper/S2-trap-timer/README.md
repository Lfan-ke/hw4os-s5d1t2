# 正经·S2 · trap 与时钟中断

> 承接 S1（已能引导进 S 态）。本课让内核能**响应中断/异常**——多任务与抢占的地基。

## 0. 这节课在讲什么

CPU 遇到中断或异常时，跳到 `stvec` 指向的入口、进入 trap。共享的 `__alltraps`（`common/kernel/trap.S`）已把 32 个通用寄存器 + `sstatus`/`sepc` 存成一份 `TrapContext` 压栈，再调你的 `trap_handler(ctx)`；返回后 `__restore` 恢复现场 `sret`。

你要让内核**周期性响应时钟中断**：`kmain`（给定）开启 `sie.STIE` + `sstatus.SIE` 并排第一次时钟，然后等够 5 拍退出。每拍由你的 `trap_handler` 累加。

## 1. 你要实现的（`kernel/trap.c` 的 `trap_handler`）

```
若 中断(scause 最高位) 且 时钟(低 8 位==5)：g_ticks++; set_next_trigger();
否则(异常)：打印 scause；ctx->sepc += 4 跳过出错指令
```

⚠️ 时钟中断**必须** `set_next_trigger()` 重置比较器，否则中断风暴、内核卡死（超时判 FAIL）。

```
labctl run proper/S2-trap-timer
make -C kernel run     # 手动跑（OpenSBI banner 后见内核输出）
```

判据：输出含 `TIMER_PASS` / `TRAP_PASS` / `ALL_PASS`，不出现 `UNEXPECTED_TRAP`。

## 2. 完成标准 (DoD)

- [ ] 时钟中断正确分发、`g_ticks` 累加到 5、内核正常退出。
- [ ] 能说清 trap 的保存/恢复现场、`stvec`/`scause`/`sepc` 各自作用。
- [ ] 理解为何时钟中断要重置比较器。

## 3. 引申

- 处理更多异常类型（非法指令、缺页）；U/S 切换时用 `sscratch` 换栈（S8 进程会用到）。
