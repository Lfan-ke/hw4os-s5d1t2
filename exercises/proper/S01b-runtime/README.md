# 正经·S01b · 裸机最小标准库（刚流片的板子如何速起 std 开发环境）

> 工程落地课。场景：一块**刚流片**的 RISC-V 板子，串口刚通（S01 SBI 能打印了）。
> 你要在它上面**尽快**搭起一个能用 `Vec`/`String`/`malloc`/`printf` 写业务逻辑的开发环境——
> 而不是一直在裸指针里手搓。本课讲清这条「速起标准库」的最短路径。

## 0. 为什么裸机上「标准库」不是白来的

编译器只保证最底层的语言核心可用；要用到「需要内存分配 / 需要 I/O」的标准库设施，
必须由你把**运行时挂钩 / OS 适配桩**补上。补齐了，标准库就「活」了。

```
Rust:   core(白送, 无需分配器)  ──加 #[global_allocator] + #[panic_handler]──▶  alloc(Vec/Box/String)
                                                                    ──加 OS syscall 适配层──▶  std
C  :    freestanding(只有语言)  ──加 newlib/picolibc 的 _sbrk/_write/... 桩──▶  完整 libc(malloc/printf)
```

## 1. 你要实现的

### Rust 路径（`kernel-rust/src/main.rs`）
- 实现 `#[global_allocator]` 的 `Bump::alloc`（对齐 + 从静态 `HEAP` 切一段；越界返回 null）。
- `#[panic_handler]` 已给（no_std 必备）。填好后 `Vec`/`format!` 即可用。
- 判据：`CORE_PASS`（core 切片/迭代器本就能用）→ `ALLOC_PASS`（Vec/String 活了）→ `ALL_PASS`。

### C 路径（`kernel-c/syscalls.c`）
- 实现 newlib 式两个关键桩：`_sbrk`（malloc 的堆来源）、`_write`（printf/puts 的出口）。
- 其余桩（`_exit`/`_read`/`_close`/...）与 `minlibc`（malloc/printf 本体）已给。
- 判据：`WRITE_PASS` → `SBRK_PASS`/`MALLOC_PASS` → `PRINTF_PASS` → `ALL_PASS`。

```
labctl run proper/S01b-runtime           # 两个变体都跑（make → qemu）
make -C kernel-rust run                  # 单独手动跑 Rust
make -C kernel-c run                     # 单独手动跑 C
```

## 2. 完成标准 (DoD)

- [ ] Rust：补 `global_allocator` 后 `Vec`/`String`/`format!` 可用，跑出 `ALL_PASS`。
- [ ] C：补 `_sbrk`/`_write` 后 `malloc`/`printf` 可用，跑出 `ALL_PASS`。
- [ ] 能说清：为什么 `core` 不需要分配器而 `alloc` 需要；newlib 的 libc 为什么离不开 `_sbrk`/`_write`。

## 3. 引申（真实工程）

- 再往上一层就是 `std`：给 Rust 加一个最小 OS 适配层（线程/文件/时间的 syscall 垫片）即可逐步点亮 `std`；
  C 侧把这套桩接到真 **newlib / picolibc**（`material/libc/{newlib,picolibc,baselibc}`）就是工业做法。
- 真实 `_sbrk` 通常从链接符号 `_end` 向栈方向生长，而非静态数组；malloc 之上还有 free-list / buddy。
- 板厂拿到新片，最先做的就是「串口 + 这套运行时桩」——之后所有上层开发才谈得上效率。
