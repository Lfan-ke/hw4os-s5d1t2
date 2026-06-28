# S09 思考题

请结合本实验代码（crt0.S / libc.c / user.c）与 S08 的 U 态 + syscall 骨架作答。

## Q1. crt0 到底有什么用？为什么不让内核直接 `sret` 到用户的 `main`？

（提示：`main` 用 `return` 结束时 CPU 该往哪去？退出码怎么报给内核？谁来传 argv / 跑全局构造器？）

## Q2. `printf` 为什么先格式化到缓冲再一次 `write`，而不是边算边一个字符一个字符 syscall？

（提示：一次 `ecall` 陷入的固定开销有多大？格式化是不是纯用户态计算？真实 libc 的「带缓冲流」在省什么？）

## Q3. bump（指针碰撞）malloc 简单在哪、它放弃了什么？真 `malloc` 多做了什么？

（提示：bump 能 `free` 单独一块吗？真 `malloc` 的空闲链表 / 合并相邻块 / 向内核要内存各解决什么？）

<!-- LABCTL_ESSAY_TODO: 在此填写你的作答（替换本行）。 -->
