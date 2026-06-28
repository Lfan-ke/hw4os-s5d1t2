# 正经·S08 · 用户态与系统调用（rcore ch2 批处理风）

> 承接 S02（trap 入口/分发已就绪）。本课让内核第一次**跌入 U 态**跑一个用户程序，
> 用户经 `ecall` 求内核办事（系统调用），办完 `exit`，内核回收。这是「批处理内核」的最小骨架。

## 0. 这节课在讲什么

特权级不是玄学：`sstatus.SPP` 这一个 bit 决定 `sret` 落到 S 态还是 U 态。本实验：

1. `kmain`（给定）设好 `stvec`，调 `run_user(entry, ustack)`（`uentry.S` 给定）：
   清 `sstatus.SPP` → `sret` 进 **U 态**，在独立用户栈上跑 `user_main`。
2. `user_main`（给定，U 态）**只**通过 `ecall` + 寄存器约定求服务：
   `a7`=调用号，`a0..a2`=参数，返回值在 `a0`。它 `sys_write` 打印 `SYSCALL_PASS`，再 `sys_exit(0)`。
3. U 态 `ecall` 触发异常（`scause` code = **8**，U 环境调用），跳共享 `__alltraps` 存上下文、调你的 `trap_handler`。
4. 你在 `trap_handler` 里**分发 syscall** 并 **`sepc += 4`**；`sys_exit` 经 `return_to_kernel()`
   直接 longjmp 回内核主线，内核打印 `PROC_PASS` 后 `ALL_PASS`、关机。

> 无分页：用户程序与内核同地址空间，仅靠**特权级**隔离（PMP 由 OpenSBI 配好给 S/U 全权）。
> 这正对应 rcore ch2「批处理」——还没有 ch4 的地址空间隔离。

## 1. 你要实现的（`kernel/syscall.c` 的 `trap_handler`）

```
若 (scause 最高位==0) 且 (scause == 8 即 U 态 ecall)：
    n  = ctx->x[17]            // a7：系统调用号
    a0 = ctx->x[10] ...        // a0..a2：参数
    ret = do_syscall(n, a0, a1, a2)   // 分发表已给：64=write, 93=exit
    ctx->x[10] = ret           // 返回值写回 a0
    ctx->sepc += 4             // 跳过 ecall，否则 sret 后又陷入 → 死循环
否则：打印 scause，sepc += 4 跳过
```

`do_syscall` / `sys_write` / `sys_exit` 与进/出 U 态的 `run_user` / `return_to_kernel` 已给。

```
labctl run proper/S08-syscall-process
make -C kernel run     # 手动跑（OpenSBI banner 后见内核输出）
```

判据：输出含 `SYSCALL_PASS`（用户经 write 打印）/ `PROC_PASS`（内核回收）/ `ALL_PASS`，
不出现 `UNEXPECTED_*` / `FAIL`。

## 2. 完成标准 (DoD)

- [ ] 内核 `sret` 成功进 U 态、用户程序运行。
- [ ] U 态 `ecall` 被正确识别（`scause==8`）并按 `a7` 分发，返回值回 `a0`、`sepc+=4`。
- [ ] `sys_exit` 后内核回收并打印 `PROC_PASS` / `ALL_PASS`，qemu 正常关机。
- [ ] 能说清：为什么 syscall 用「号+寄存器约定」而非让用户跳进内核地址（隔离 / ABI 稳定 / 安检门）。

## 3. 引申

- **地址空间隔离**（rcore ch4）：每进程独立 SV39 页表，用户访问内核地址直接缺页——不再靠「同地址空间+特权级」裸奔。
- **进程模型**：`fork` / `exec`（ELF 加载）/ `wait`，多道程序与就绪队列（接 S05 调度器）。
- **文件类 syscall**：`open/read/write/close/dup` 接 VFS（S07）。
- U/S 切换用 `sscratch` 换内核栈（本实验为简化让 trap 跑在用户栈上，靠特权级而非换栈）。
