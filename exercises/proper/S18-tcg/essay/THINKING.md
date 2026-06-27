# S18 思考题（学生作答）

按你的理解作答；答案非空且命中关键字即可
（如 `同 ISA`/`异 ISA`、`trap-and-emulate`、`type1`/`type2`/`type1.5`、`TCG`/`动态翻译`、`基本块`/`缓存`）。

## Q1. type-1 / type-2 / type-1.5 / 模拟器 各是什么？为什么 type-2 仍是「虚拟机」而跨架构 qemu 是「模拟器」？

（在此作答：抓「同 ISA / 异 ISA」这条分界线。VM 同 ISA、guest 原生跑、只有特权操作 trap-and-emulate；
type1 裸机、type2 宿主 OS 上的应用、type1.5 宿主内核模块化(KVM)。type2 判定靠「同 ISA」而非「在不在宿主 OS 上」，
故仍是 VM；跨架构 qemu guest≠host ISA，必须逐条翻译，是模拟器。）

## Q2. 动态二进制翻译（TCG）和纯解释执行（逐条 switch）差在哪？「mini-TCG」省了真 TCG 的什么？

（在此作答：解释器每条都重解码+派发；TCG 以基本块为单位翻译成 host 码并缓存，热路径翻译一次原生执行多次。
本实验把翻译产物做成 C 微操作而非真 host 机器码，也没做翻译缓存/块链接/内存/PC，但 decode→IR→选 host 操作→run 骨架一致。）

## Q3. 把 mini-TCG 扩成能跑真 guest 内核，缺口在哪？哪一步又把我们带回 S17？

（在此作答：补 load/store/分支/CSR、完整 guest 状态(PC/内存/特权级)、翻译缓存、设备模拟；
其中 guest 特权操作/异常的「按 guest 语义改特权状态+跳 trap 向量」就是 trap-and-emulate，正接回 S17，
区别是 S17 靠硬件 H 扩展真实陷入(VM 路径)，mini-TCG 在翻译/执行循环里软件判定(模拟器路径)。）

<!-- 删除本行并写满上面三题后再提交：LABCTL_ESSAY_TODO -->
