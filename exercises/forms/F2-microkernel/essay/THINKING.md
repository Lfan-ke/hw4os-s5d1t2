# F2 微内核 · 思考题（essay 变体）

在每题下方写下你的理解即可（命中关键字即通过：能力/不可伪造/rights、IPC fastpath、
最小 TCB/形式化验证、故障隔离 等）。

## 1. capability 与 POSIX `fd` 都是「整数句柄」，本质区别在哪？

（联系：fd = 进程私有下标 + 环境权威 / ambient authority；cap = 不可伪造的对象引用 +
自带 rights 位，只能由内核派生/传递；confused deputy 问题、最小权限、revoke。）

TODO: 在此作答。

## 2. Tanenbaum–Linus 之争里「微内核太慢」的论据是什么？为什么今天看是旧账？

（联系：每次系统调用变成多跳跨地址空间 IPC；Mach 第一代微内核 IPC 重；L4 / seL4 的
IPC fastpath——跳过调度器、寄存器传短消息、统一异常出口 → < 1μs；ASID 切空间不刷 TLB。）

TODO: 在此作答。

## 3. seL4 凭什么能把约 9 千行 C 内核完全形式化证明？「最小 TCB」与可证明性是什么关系？

（联系：refinement 三层精化——抽象规约 / 可执行规约 / C 实现；TCB 越小要证明的代码越少；
微内核把 fs/驱动赶到用户态从而缩小 TCB；seL4 vs Zircon/zCore vs MINIX3 三种谱系。）

TODO: 在此作答。

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
