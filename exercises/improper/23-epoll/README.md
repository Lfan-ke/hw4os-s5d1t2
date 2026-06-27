# 23 · I/O 多路复用：select 全表扫描 vs epoll 就绪链表与边沿触发

> 不正经赛道 · 心智模型课 —— 纯软件建模，host 直接跑（Rust / C 双语言）。
> 一句话母题：**一个线程怎么同时盯住一万个连接？别一连接一线程地死等，而是
> 让内核在「谁就绪了」时只把那一小撮就绪的 fd 递给你。**

## 0. 这节课在讲什么

这是「没吃过猪肉但见过猪跑」式的心智模型课：不抠 `epoll_create` 的真实系统调用，
只用一个**「N 个 fd，每个有就绪态」**的玩具，亲手对比两条「找就绪」的路：

- **select**：把全部 fd 从头扫到尾，凡可读就收集。简单，但 O(n)——1 个就绪也得扫一万次。
- **epoll**：先 `epoll_ctl` 注册「兴趣集」，内核在 fd「**变就绪那一刻**」把它挂上一条
  **就绪链表**；`epoll_wait` 只遍历就绪链表，O(ready)。这就是 epoll 能扛 C10k 的根。

顺手把 **边沿触发 ET vs 水平触发 LT** 的差别也用同一个玩具演出来。

对应真实系统：`epoll`/`kqueue`/`io_uring`、Nginx/Redis/Node.js 的事件循环 (reactor)、
Rust `tokio`/`mio`、Linux `EPOLLET`、`select(2)`/`poll(2)` 的 fd_set 全表轮询。

## 1. 你要实现什么

软件在 `sw/rust/src/main.rs` 或 `sw/c/epoll.c`。模型：`fds[]` 每个 fd 一个 `readable` 位；
一个极简 `Epoll`：`interest`（兴趣集）+ `ready`（就绪链表）+ `scan`（检视计数器）。

**你只需填 2 处**（其余 select/内核侧/harness 全部给好）：

| 填空 | 函数 | 要求 | 判据 |
| :-- | :-- | :-- | :-- |
| 1 | `epoll_wait` 收集 | 遍历就绪链表，只收「已注册 且 当前可读」的 fd | `EPOLL_PASS`/`SCALE_PASS` |
| 2 | `edge_ready` | 边沿判定：`!prev && cur`（不可读→可读的上升沿） | `EDGE_PASS` |

四段子实验：

| 子实验 | 判据 | 验证什么 |
| :-- | :-- | :-- |
| 1 select 全表扫描 | `SELECT_PASS` | 扫描全部 fd 返回就绪集，计数器=n（已给好，自动过） |
| 2 epoll 注册+wait | `EPOLL_PASS` | `epoll_ctl` 注册兴趣集；wait 只返回已注册且就绪，未注册的不返回 |
| 3 边沿 vs 水平 | `EDGE_PASS` | ET：不可读→可读只通知一次；LT：仍可读则持续通知 |
| 4 伸缩性 | `SCALE_PASS` | 1 就绪/1000 fd，epoll 检视 O(ready)=1、select 检视 O(n)=1000，计数器证明 |

四段皆过再打印 `ALL_PASS`。失败诊断打印 `*_MISS`（少了/没返回）/ `*_BAD`（多了/计数错），
不含 `FAIL`/不崩。`SCALE_INFO` 是信息行（打印两个计数器对比），非判据。

```
labctl run improper/23-epoll      # 跑 rust / c 两条路径
labctl watch                      # 边改边自动判定
labctl hint improper/23-epoll     # 卡住看提示
```

## 2. 完成标准 (DoD)

- [ ] 至少一条变体跑出 `SELECT_PASS`/`EPOLL_PASS`/`EDGE_PASS`/`SCALE_PASS`/`ALL_PASS`，无任何 `*_MISS`/`*_BAD`（必修）。
- [ ] Rust 与 C 两条路径行为一致，且 `cargo run` 0 warning。
- [ ] `epoll_wait` 只遍历就绪链表（`SCALE_INFO` 里 epoll 检视数 == 就绪数，远小于 select）。
- [ ] essay 子题答出「C10k、O(n) vs O(ready)、LT vs ET、就绪式 vs 完成式、reactor」要点。
- [ ] 能口述 `edge_ready` ↔ `EPOLLET`、就绪链表 ↔ `epoll` 的 ready list、`scan` 计数器 ↔ 为何 epoll 扛得住一万连接。

## 3. 关键约定（判题用）

- `Epoll.interest[fd]`：`None` 未注册 / `Some(false)` 水平触发 LT / `Some(true)` 边沿触发 ET。
- `set_readable`（内核侧，已给）：设 fd 可读态；构成上升沿(`edge_ready`)且已注册 → 挂上就绪链表。
  **未注册的 fd 永不入链**，故 `epoll_wait` 永不返回它（EPOLL 子题里 fd6 就是这样被排除的）。
- `epoll_wait`（你填收集 + 给定的「重新武装」）：只遍历就绪链表，`scan += 链表长度`。
  - **LT 重新武装**：报告后若 `readable` 仍为真，把 fd 挂回链表 → 下次 wait 继续通知。
  - **ET 不武装**：报告后摘除，要等 `edge_ready` 再判出一次新沿才重新入链。
- `select_scan`（已给）：遍历全部 n 个 fd，计数器 `+= n`。两个计数器之比即 O(ready) vs O(n)。

## 4. 引申：从「就绪态玩具」到真实事件驱动栈

本课只保留了 epoll 的**骨架**：一个 `readable` 位、一条就绪链表、一个 `scan` 计数器，
单线程、无真实 fd、无系统调用、`set_readable` 由 harness 喂。真实事件循环里被省掉的那些，
恰恰是写高并发服务器最容易踩坑的地方。按兴趣挑一条深入：

1. **换成真 syscall 跑通一遍**：用真的 `epoll_create1/epoll_ctl/epoll_wait`（C）或
   Rust **mio** 监听一个 TCP `listener`，验证「未注册的 fd 不会被返回」「ET 只在沿上通知一次」
   在真内核里也成立。对照 BSD 的 **kqueue**、Windows 的 **IOCP**。
2. **把 ET 的坑踩实**：写一个 ET 模式的 echo 服务器，故意**只 read 一次不读到 `EAGAIN`**，
   观察「剩余数据再也收不到通知」的挂起 bug——这正是本课 `edge_ready` 上升沿语义的现实代价。
   再补上「循环读到 EAGAIN」修好它。
3. **补缺失的内核侧机制**：`EPOLLONESHOT`（报告后自动摘除兴趣集，多线程取事件防重复）、
   `EPOLLRDHUP`（对端半关闭）、`EPOLLEXCLUSIVE`/多线程 accept 的**惊群 (thundering herd)**——
   本课单线程无锁，加上多 worker 才会暴露这些。
4. **接一个真的 reactor 事件循环**：把就绪集喂给一组回调（handler），做成 Nginx/Redis 那样的
   单线程 reactor；或对照 Rust **tokio**（mio + waker + Future）看「就绪事件」如何被翻译成
   `async/await` 的唤醒。回扣 `proper` 里的异步执行器。
5. **就绪式 vs 完成式**：把同一个 echo 服务器改用 **io_uring**（提交/完成两条环形队列），
   体会「epoll 告诉你『可以读了』、io_uring 告诉你『已经读完了』」的范式差异，以及零拷贝/批量提交。
6. **数据结构对照**：本课 interest 是个数组、ready 是条链表；Linux 真实实现里 interest 集合是
   **红黑树**（`epoll_ctl` O(log n) 增删）、ready 是双向链表 + 就绪回调 `ep_poll_callback`。
   想想 fd 上万、增删频繁时为什么需要平衡树而不是数组。

## 5. 思考题（`essay/THINKING.md` 作答即可通过）

1. C10k 问题：为什么「一连接一线程 + 阻塞 read」扛不住一万连接？epoll 凭什么能扛？
2. select/poll 是 O(n)、epoll 是 O(ready)，差别从哪来？（兴趣集常驻内核、就绪回调挂链表）
3. 水平触发 LT vs 边沿触发 ET：语义差别？ET 为什么必须把 fd 读到 `EAGAIN`？
4. 就绪式 (epoll) vs 完成式 (io_uring/IOCP)：两种异步范式有何不同？
5. 事件循环 / reactor 怎么把它们串起来？对照「一连接一线程的阻塞 IO」。
