# 07-ipc 思考题（essay 变体）

在每题下方写下你的理解即可（命中关键字即通过：内存序/可见性、自旋/阻塞、关中断/LR-SC 等）。

## 1. 为什么 B 必须「先写 RESULT 再置 DONE」？若顺序反过来，A 可能读到什么垃圾值？

（联系内存序、可见性、`fence`、release-acquire。）

TODO: 在此作答。

## 2. 自旋锁（spin）与阻塞锁（block）各自浪费/节省了什么？临界区很短/很长、单核/多核分别该用哪个？

（联系 rcore `MutexSpin` vs `MutexBlocking`。）

TODO: 在此作答。

## 3. 单核内核可以靠「关中断」得到原子性（rcore `UPIntrFreeCell`），为什么到了 SMP 就必须用真正的 `lr.sc`/`amoswap`？硬件层面那一个 `DONE` 位的「原子置位」又靠什么保证不被读到中间态？

TODO: 在此作答。

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
