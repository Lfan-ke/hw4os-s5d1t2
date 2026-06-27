# S4 思考题（参考解）

## 1. 无栈协程「无」的是哪根栈？poll 的状态存在哪里？

有栈协程（纤程/线程）每个任务**独占一根栈**，暂停时把「现在执行到哪、局部变量是什么」
天然留在那根栈上，恢复时换回栈指针即可（rcore `__switch` 换 `sp`+callee-saved）。

无栈协程**没有每任务独立栈**：所有「跨让出点还要活着」的状态被编译器/程序员搬进一个
**状态结构体**（本课的 `struct Task`：`state` 是状态号，`n`/`wake_tick` 是跨 poll 存活的局部）。
`poll()` 每次从 `state` 续上、跑到下一个让出点、更新 `state` 再返回 `Pending`。
一句话：**有栈协程换的是栈指针，无栈协程换的是状态号**。代价是「跨让出点的局部必须显式入
struct」（Rust 由编译器自动生成这台状态机，C 只能手写 enum/switch 或 protothread 宏）。

## 2. waker / reactor 解决了什么？为什么不在 executor 里 busy-poll？

如果延时 future 没好就让 executor 把它**立刻重新入队反复 poll**，就是 busy-poll 空转：
CPU 100% 占用、烧电、且挤占其他任务。embassy 式做法是：future 返回 `Pending` 时**登记一个
唤醒条件**（这里是 `wake_tick`，挂到 timer reactor），然后 executor 在就绪队列空时直接 `wfi`
睡死，等**事件源**（时钟中断推进 `g_ticks`）来 `reactor_wake_due()` 把到期任务**重新入就绪
队列**——只有被「叫醒」的任务才会被再 poll。这就是 `Waker`：一个「把我放回就绪队列」的回调。
真实 embassy/tokio 里每个外设（timer/UART/网卡中断）都有自己的 reactor，中断处理程序调
`waker.wake()` 唤醒等它的任务。

## 3. 「无让出即退化为顺序执行」——给个例子，并说怎样才真交错。

把 `count_poll` 改成「第一次 poll 就返回 `Ready`（从不返回 Pending）」：executor 取出任务 A，
poll 一次即完成、出队，再取 B……输出变成 `AABBCC`（其实每个任务只打一次，是严格顺序）。
这时多任务运行时**退化为顺序批处理**，毫无交错/并发——和纤程课「任务体不 yield 就是批处理」
是同一回事，只是让出方式从「调 yield 切栈」变成「poll 返回 Pending」。

要真交错，至少需要一个**让出点**：poll 在「逻辑上该等一等」的地方返回 `Pending`（WAKE_NOW
让出 / WAKE_TIMER 等定时 / 等 I/O），把 CPU 交还 executor 去推进别的任务。本课 `n=2` 的计数
任务每次让出一次，三个任务就交错成 `ABCABC`。注意：单核 + 协作式下这是**并发不并行**——
靠让出点把一条 CPU 时间切给多个任务交替推进，掩盖等待，而非真的同时跑。
