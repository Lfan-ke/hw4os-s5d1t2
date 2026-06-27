# 20-signal 思考题（essay 变体）

在每题下方写下你的理解即可（命中关键字即通过：异步/挂起/屏蔽、async-signal-safe、
sigaction、标准 vs 实时信号、与硬件中断类比 等）。

## 1. 什么是「异步事件」？信号为什么需要 handler 表 + pending + mask 这套机制？

（联系：异步=与当前执行流无关随时插进来；handler=事先登记回调；pending=投递不出去先挂起；
mask=主动节流。）

TODO: 在此作答。

## 2. 什么是 async-signal-safe（异步信号安全）？为什么 handler 里只能干极少的事（不能 `malloc`/`printf`）？

（联系：handler 在主程序任意指令中间插入，可能撞上半路的锁/堆链表；只能调可重入函数；
常见范式 = handler 只置一个 `volatile sig_atomic_t` 标志 / self-pipe。）

TODO: 在此作答。

## 3. `signal()` 与 `sigaction()` 有何区别？为什么现代代码该用后者？

（联系：`signal` 语义不一致、有一次性重置 + 重注册竞态；`sigaction` 的 `sa_mask`/
`SA_RESTART`/`SA_SIGINFO`/`SA_NODEFER`。）

TODO: 在此作答。

## 4. 「标准信号」与「实时信号」在排队上有何本质区别？本实验建模了哪一种？怎么升级成另一种？

（联系：标准信号 pending 是一个「位」、合并丢重复、不保序；实时信号逐个排队、可携带值
`sigqueue`/`si_value`。本实验 pending 用位集 → 建模标准信号；升级 = 换成每号一个 FIFO 队列。）

TODO: 在此作答。

## 5. 信号与硬件中断有哪些类比、又有哪些关键不同？

（联系：相似=异步打断 + 预登记 ISR + 屏蔽位 + pending 位 + 处理要短；
不同=触发主体/特权态/投递时机/排队粒度。`mask`↔`mie`、`pending`↔`mip`。）

TODO: 在此作答。

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
