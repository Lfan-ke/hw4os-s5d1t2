# 正经·S9 · 极简 libc（给 U 态用户程序用）

> 承接 S8：内核已能 `sret` 跌入 U 态、用户经 `ecall` 求服务、`exit` 后被回收。
> 但 S8 的用户程序是「裸手写 ecall」。本课在 U 态之上盖一层最小 **libc**——
> 让用户程序像普通 C 程序一样写 `main()` / `printf()` / `malloc()`，
> 由 libc 把它们翻译成 `write`/`exit` 系统调用。这就是「用户态运行时」的最小骨架。

## 0. 这节课在讲什么

一个普通进程跑起来，靠的不只是内核，还有一层**用户态运行时（libc + crt0）**：

- **crt0**：程序真正的入口（`_start`）。内核把控制权交给它，它准备好 C 运行环境、
  调 `main()`，再把 `main` 的返回值当退出码交给 `exit()`。`main` 不是第一行代码。
- **syscall 封装**：`write()` / `exit()` 这些函数体里就是一条 `ecall` + 寄存器约定——
  libc 把「报号、传参、取返回值」的脏活封装成普通函数调用。
- **`malloc`**：内核只给「整块内存」（本实验是一段静态堆），细粒度分配是 libc 的事。
  最简单的实现是 **bump（指针碰撞）**：一个游标顺序往后切，从不回收。
- **`printf`**：格式化（`%d/%x/%s`）是纯用户态计算，算完整串再一次 `write(1, buf, n)`。

本实验把这四样做成能跑的最小版，用户程序 `user.c` 用它们打印三枚通行证：
`CRT0_PASS` / `MALLOC_PASS` / `PRINTF_PASS`；全过则 `main` 返回 0，crt0 `exit(0)`，
内核盖章 `ALL_PASS`。

## 1. 数据流（一次 `printf` 的旅程）

```
user main()  --call-->  printf(fmt,...)         [U 态, libc]
   printf: vfmt(buf,fmt,ap) 把 %d/%x/%s 算成纯文本   [U 态纯计算]
   printf: write(1, buf, n)
       write: usys(SYS_WRITE, 1, buf, n) -> ecall   [U 态 -> 陷入]
           __alltraps 存上下文 -> trap_handler       [S 态]
               do_syscall(64,...) -> sys_write -> console_putchar(SBI)
           sepc += 4; sret                            [回 U 态]
   write 返回 n -> printf 返回
```

`exit` 同理走 `SYS_EXIT`，但内核 `sys_exit` 用 `return_to_kernel()` longjmp 回 `kmain`，
不回 U 态——进程就此结束。

## 2. 你要实现的（`kernel/libc.c` 两处 TODO）

1. **`malloc`（bump）**：
   - 把游标 `heap_off` 向上 8 字节对齐：`(heap_off + 7) & ~7`；
   - 若 `off + n > sizeof(heap)` → 堆耗尽，返回 `0`；
   - 否则取 `&heap[off]`，`heap_off = off + n`，返回该指针。永不 `free`。
2. **`vfmt` 里 `%d` / `%x` 的数字格式化**：
   - `%d`：先处理负号（`INT_MIN` 用 `-(long)v` 再转 `unsigned` 防溢出），
     反复 `% 10` 把各位倒着塞临时数组，再**逆序**倒出。
   - `%x`：反复取低 4 位 `& 0xF`（`0..9`→`'0'..'9'`，`10..15`→`'a'..'f'`），逆序倒出。

`%s`、`crt0`、`write`/`exit` 封装、`printf`/`sprintf` 框架均已给定。

```
labctl run proper/S9-libc
make -C kernel run     # 手动跑（OpenSBI banner 后见内核 + 用户输出）
```

判据：输出含 `CRT0_PASS` / `MALLOC_PASS` / `PRINTF_PASS`（用户经 printf 打印）与
`ALL_PASS`（内核回收并确认 exit 0 后盖章），不出现 `FAIL` / `panic` / `UNEXPECTED_*`。

## 3. 完成标准 (DoD)

- [ ] `malloc` 给出 8 字节对齐、互不重叠、可写的内存，堆耗尽时返回 `NULL`。
- [ ] `printf("%d %x %s", ...)` 输出与标准库一致（含负数、`0`、十六进制小写）。
- [ ] 三枚 `*_PASS` 齐出，`main` 返回 0，内核盖 `ALL_PASS`，qemu 正常关机。
- [ ] 能说清：crt0 为什么必须存在？`printf` 为什么不直接 `console_putchar` 一个字符一个字符地打？

## 4. 引申

- **真 `malloc`**：free-list / 分级（bins）/ 合并相邻空闲块；`brk`/`mmap` 向内核要内存而非静态堆。
- **真 crt0**：传 `argc/argv/envp`、跑全局构造器（`.init_array`）、设 TLS、对齐栈。
- **`printf` 全集**：宽度/精度/`%ld`/`%p`/`%c`/`%u`/填零，带缓冲的 `FILE` 流与行缓冲。
- **真正的进程隔离**：本实验用户与内核同地址空间（同 S8），libc 与内核链在同一个 ELF；
  真实系统里 libc 是独立加载、用户有独立 SV39 地址空间（rcore ch4）。
