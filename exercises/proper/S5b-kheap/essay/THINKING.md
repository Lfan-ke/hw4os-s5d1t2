# S5b 思考题（学生作答）

按你的理解作答；答案非空且命中关键字即可
（如 `动态`/`数量不定`、`first-fit`/`buddy`/`slab`、`外部碎片`/`coalesce`、`global_allocator`）。

## Q1. 内核为什么需要动态分配器？静态/栈分配不够吗？

（在此作答：内核要管「数量不定、寿命交错」的对象——PCB、fd、缓冲区；静态数组定不下大小、
栈寿命绑作用域。动态堆才能按需申请、用完归还、归还可复用。）

## Q2. first-fit / best-fit / buddy / slab 各自取舍？

（在此作答：first-fit 简单快但低地址易碎；best-fit 省内存却留极小碎块且扫全表；
buddy 2 的幂、合并 O(1)、有内部碎片；slab 固定对象 O(1) 零外碎片。）

## Q3. 什么是外部碎片？coalesce 如何对抗它？没有合并会怎样？

（在此作答：空闲总量够但被切成不相邻小块，装不下大请求；释放时合并相邻空闲块回收连续空间；
不合并则相邻碎块各自太小，明明腾出连续空间却用不上——对照本实验 COALESCE 关。）

## Q4. 对照 Rust 的 `#[global_allocator]`（S1b）——它和本实验是一回事吗？

（在此作答：是。kalloc/kfree 对应 GlobalAlloc 的 alloc/dealloc；挂上全局分配器后 Box/Vec
在内核里可用，rcore ch4 即如此。C 手动调，Rust 由编译器在析构时替你调 dealloc。）

<!-- 删除本行并写满上面四题后再提交：LABCTL_ESSAY_TODO -->
