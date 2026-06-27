# F5 框内核(framekernel) 思考题（essay 变体）

> 原型：Asterinas（蚂蚁集团 OS Lab，SOSP'25 Best Paper）。
> 在每题下方用自己的话作答；答完后**删除文件末尾的哨兵行**（含未作答标记的那一行）即判过。

## 1. framekernel 凭什么「既要又要」：单地址空间的宏内核性能 + 微内核式隔离？类型系统怎么替代 MMU？

（在此作答：宏/微内核在「性能 ↔ 隔离」上的二选一；framekernel 如何把内核劈成
OS Framework + OS Services 两半；隔离如何从「运行时 MMU 检查」前移成「编译期类型检查」、
为什么是零成本抽象。可结合本 demo 里两个相邻 Frame 共享物理池却互不可改的现象。）

## 2. 为什么 C 做不到，只能靠「约定 / MMU」近似？（类比：C 也没有 async/await）

（在此作答：framekernel 的隔离依赖编译器强制「子系统禁用 unsafe」+「安全 API sound」；
C 里每次指针解引用都是 unsafe、编译器不区分框架/子系统，只能 opaque handle + 约定 +
运行时计数器近似，纪律一破就无人拦截；要硬隔离只能退回 MMU/进程边界。可类比 async/await
在 C 里只能用状态机/ucontext 约定式模拟。）

## 3. 把 unsafe 收敛到框架（最小 TCB）的意义？soundness 为何是硬要求？对照 Rust-for-Linux。

（在此作答：最小化 memory-safety TCB → 只需审计框架那一小块 unsafe；为什么一个 unsound 的
safe API 会让整个论证崩塌；对照 Rust-for-Linux 把 Rust 嵌进 unsafe 的 C 内核 vs
framekernel clean-slate 整层 deny(unsafe_code) 的区别。）

LABCTL_ESSAY_TODO 作答完成后删除本行即判过
