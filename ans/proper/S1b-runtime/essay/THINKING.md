# S1b 思考题 · 裸机最小标准库（参考答案）

## 1. 为什么 Rust 的 `core` 不需要分配器，而 `alloc` 需要？
`core` 只含「不分配堆内存」的语言核心：基本类型、切片、迭代器、`Option/Result`、原子、
`fmt` 的格式化 trait 等。它们大小编译期已定，放栈或静态区即可。`alloc`（`Vec/Box/String/BTreeMap`）
要在运行时按需扩容，必须向某处「要一块堆」——这个「某处」就是 `#[global_allocator]`。
没有它编译器不知道堆从哪来，所以裸机用 `alloc` 必须先挂一个全局分配器（哪怕只是 bump）。
此外 `no_std` 二进制还必须有 `#[panic_handler]` 才能编译——panic 时跳哪去由你定。

## 2. newlib 的 `_sbrk` / `_write` 为什么是「移植」的关键？
newlib/picolibc 把「与 OS 无关」的 libc 逻辑（printf 格式化、malloc 的 free-list、字符串函数）
都写好了；凡是「要碰 OS」的地方都收敛成一组 syscall 桩：malloc 最终经 `_sbrk` 向系统要堆，
stdout 最终经 `_write` 把字节交给系统。把 libc 移植到新板子 = 实现这组桩。本实验只填最关键的
`_sbrk + _write` 就让 malloc/printf 活了；真 newlib 还需 `_read/_close/_lseek/_isatty/_fstat/_kill/_getpid`。

## 3. 从 `alloc` 到 `std` 还差什么？
`std` 在 `core+alloc` 之上又加了「依赖 OS」的设施：线程、文件、网络、时间、进程、同步原语。
要在自研 OS 上点亮 `std`，需提供一个平台适配层（Rust 里是 std 的 `sys` 后端 / 自定义 target），
把这些设施映射到你内核的 syscall。一句话：core 免费、alloc 加个分配器、std 加一整套 OS 适配。

## 4. 板厂 bring-up 的顺序
新片回来：先点串口（能 log/printf）→ 补这套运行时桩（malloc/printf 活）→ 才谈得上高效写驱动/业务。
所以「最小标准库环境」是新硬件软件开发效率的地基——这正是本课的工程意义。
