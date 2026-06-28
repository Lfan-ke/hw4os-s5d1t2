# S06d 思考题（学生作答）

请结合本实验代码与 qemu 运行结果作答（每题 3-6 句）。

## Q1. 「同一份 probe，软件模型 vs 真硬件」 - improper/16（16-driver / 16b-register-model）与本实验是什么关系？

（在此作答）

## Q2. 类型化寄存器图换掉了 `#define` + 裸指针，那 `volatile` 和内存序还需要吗？为什么？

（在此作答）

## Q3. PLIC 这种稀疏大窗口为什么不写成一个大 struct？怎么建模才对（结合本实验的三个 typed 子结构）？

（在此作答）

## Q4. 把同一张表写成 Rust `tock-registers`：给出 NS16550 的 `register_structs!`/`register_bitfields!` 草稿，并说明 probe 逻辑为何能一字不改复用（对标 16b 的 Rust 第③级）。

（在此作答）

<!-- 完成后删除下面这行：LABCTL_ESSAY_TODO 本文件仍是待办占位，未作答。 -->
