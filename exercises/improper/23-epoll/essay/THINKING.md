# 23-epoll 思考题（essay 变体）

在每题下方写下你的理解即可（命中关键字即通过：C10k、就绪式/完成式、水平/边沿、reactor 等）。

## 1. C10k 问题：为什么「一个连接一个线程 + 阻塞 read」扛不住一万个连接？epoll 凭什么能扛？

（联系：每线程栈内存、上下文切换开销、绝大多数线程在干等；事件驱动 + 少量线程。）

TODO: 在此作答。

## 2. select/poll 是 O(n)，epoll 是 O(ready)，差别从哪来？

（联系：select 每次拷全部 fd 进内核 + 逐个轮询；epoll 兴趣集常驻内核、就绪回调挂就绪链表，
wait 只看链表。回到本实验里 `select_scan` 与 `epoll_wait` 的两个计数器。）

TODO: 在此作答。

## 3. 水平触发 (LT) vs 边沿触发 (ET)：各自语义？ET 为什么必须把 fd 读到 EAGAIN？

（联系：LT 只要当前满足就持续报告；ET 只在状态变化的「沿」报告一次；本实验 `edge_ready`。）

TODO: 在此作答。

## 4. 就绪式 (readiness, epoll) vs 完成式 (completion, io_uring / IOCP)：两种异步范式有何不同？

（联系：就绪式通知「能读了，你自己去 read」；完成式「我帮你读完了，数据在 buffer 里」。）

TODO: 在此作答。

## 5. 事件循环 / reactor 模式是怎么把它们串起来的？对照「一连接一线程的阻塞 IO」。

（联系：`loop { epoll_wait; for fd in ready { dispatch }}`，handler 必须非阻塞；
Redis/Nginx/Node.js。对照：阻塞模型把「等」分散到每线程的 read，调度器替你做多路复用。）

TODO: 在此作答。

---

> 作答完成后，把你在 `labctl run` 里验证过的里程碑名抄到这里（例如 `SELECT_PASS` …）。
>
> 我已点亮：（待填）

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
