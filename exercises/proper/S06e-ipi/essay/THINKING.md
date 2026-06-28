# S06e 思考题（学生作答）

请结合本实验代码与 qemu 运行结果作答（每题 3-6 句）。

## Q1. S 态内核为什么不能直接写 CLINT 的 MSIP 发 IPI？经 SBI 发 IPI 的完整链路是什么？（提示：MSIP 触发的是 M 态软件中断；目标最终取到 scause=?）

（在此作答）

## Q2. 「多核 PLIC claim 仲裁」具体仲裁了什么？为什么本实验用 barrier+轮询而不是异步抢中断？（对照 S06c 的单核 claim）

（在此作答）

## Q3. claim/complete 缺了会怎样？赢家读 UART RBR 这一步在多核场景额外重要在哪？

（在此作答）

## Q4. IPI 的内存序为什么关键？发送侧 / 接收侧各要怎么排（与 fence 的关系）？（对照本实验邮箱握手 + 16e essay 的 barrier）

（在此作答）

## Q5（Rust / tock-registers 视角）. 若用 Rust no_std 把 CLINT/PLIC 写成类型化寄存器图（register_structs!/register_bitfields!），会怎么抄？为何本套件 runnable 变体仍以 C 为准？（对照 16b）

（在此作答）

<!-- 完成后删除下面这行：LABCTL_ESSAY_TODO 本文件仍是待办占位，未作答。 -->
