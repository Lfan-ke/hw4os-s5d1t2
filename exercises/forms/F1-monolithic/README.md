# 形态 F1 · 宏内核 / 单内核（monolithic）

> 形态认知专题 · 第 1 课 —— host 软件直觉 demo（不是完整内核）。
> 一句话母题：**把 fs、调度、驱动、内存全塞进同一个地址空间，彼此直接函数调用——
> 于是快得飞起；可也正因为同住一屋、没有隔墙，一个驱动写越界就能踩坏调度器，
> 一处崩则全崩。**

## 0. 这节课在讲什么

「宏内核」(monolithic kernel)，又叫「单内核」——这两个词是**同义词**。它的定义只有一句：

> 所有 OS 服务（mm / sched / fs / net / driver）都跑在**同一个特权地址空间**里，
> 子系统之间靠**直接函数调用**协作，没有 IPC、没有特权切换。

Linux、xv6、FreeBSD、Solaris 全是这一系。与微内核（F2，fs/driver 在用户态服务进程里、
靠 IPC 通信）相反，宏内核把所有东西耦合在 `kernel/` 下，互相 `grep` 得到、共享 `struct`。

这种设计是一笔**经典权衡**：

| 维度 | 宏内核（本课） | 微内核（F2） |
| :-- | :-- | :-- |
| 一次系统调用 | 几次**普通函数调用** | 多次 **IPC + 上下文切换** |
| 性能 | **快**（零消息、零切换） | 慢（消息税） |
| 隔离 | **无**（同一地址空间） | 强（各服务独立地址空间） |
| 一个驱动崩了 | **整个内核崩** | 只崩那个服务，可重启 |

本课用最朴素的软件模型把这笔权衡**两面都演示出来**：一面是「直调有多省」，
另一面是「无隔离有多脆」。

## 1. 你要填的 6 个函数

软件在 `sw/rust/src/main.rs` 或 `sw/c/mono.c`。共一块「内核地址空间」`kmem[8]`：

```
逻辑布局（这条边界只存在于注释里！）：
  kmem:  [ 0  1  2  3 | 4  5  6  7 ]
          \__驱动缓冲__/ \_调度器队列_/
          DRV_BASE=0     SCHED_BASE=4
          DRV_LEN=4      SCHED_LEN=4
```

| 子实验 | 函数 | 要求 | 判据 |
| :-- | :-- | :-- | :-- |
| 1 直调 | `fs_read` / `sched_pick` / `driver_io` | 三个「内核服务」(就是普通函数) | `MONO_PASS` |
|        | `syscall_dispatch` | 直接串起三服务：result=和，hops=3，IPC=0 | |
| 2 脆性 | `driver_dma_write` | 写进 `kmem[DRV_BASE+off]`，**不做边界检查** | `FRAGILE_PASS` |
|        | `detect_corruption` | 调度器区 vs 基准快照逐字节比对，被改即 true | |

两段都过再打印 `ALL_PASS`。失败会打印含 `FAIL` 的行（如 `MONO_FAIL`、`FRAGILE_FAIL`）。
`MONO_DISPATCH`、`FRAGILE_OBSERVE` 是信息行（展示 IPC=0、决策 1→2），不计判据。

## 2. 完成标准 (DoD)

- [ ] 至少一条变体跑出 `MONO_PASS`/`FRAGILE_PASS`/`ALL_PASS`，无任何 `*_FAIL`（必修）。
- [ ] 能口述「直接调用链」为什么省：一次 syscall = 3 次函数调用，0 条 IPC、0 次上下文切换。
- [ ] 能口述「无隔离边界」怎么暴露：`driver_dma_write` 没有 MMU/边界保护，
      `off=5` 越过 `DRV_LEN=4` 直接落进 `kmem[5]`（调度器任务1），决策从 1 变成 2。
- [ ] essay 答出「单内核=宏内核」「为何 Linux/xv6 选它（性能）」「代价（一处崩全崩）」。

## 3. 关键约定（判题用）

- **服务直调链**：`syscall_dispatch(inode, prios)` 依次调 `fs_read → sched_pick →
  driver_io`，`result = data + idx + ack`，`hops = 3`。这 3 次都是普通函数调用，
  没有任何 IPC——这正是宏内核「快」的来源。
- **越界 DMA**：`driver_dma_write(kmem, off, val)` 就是 `kmem[DRV_BASE+off] = val`，
  **不许加边界检查**。harness 喂 `off=2`（合法，落在驱动缓冲）和 `off=5`（越界，
  踩进调度器区 `kmem[5]`）两组。无隔离 ⇒ 越界写直接生效。
- **破坏检测**：`detect_corruption(region, baseline)` 把调度器区与「驱动跑之前」
  的基准快照比对，不同即 true。越界后必须 true、合法后必须 false。
- 一切走**确定性向量**（无真并发、无真崩溃），把「同一地址空间 ⇒ 无隔离」的心智模型
  保留下来，免去触发真 segfault 的调试地狱。

## 4. 思考题（`essay/THINKING.md` 作答即可通过）

1. 「单内核」和「宏内核」是不是一回事？宏内核到底「宏」在哪、和微内核的根本分界线是什么？
2. 为什么 xv6 / Linux 这种「教学/工业主力」都选了宏内核？直调相对 IPC 到底省了什么？
3. 「一处崩溃全崩」具体怎么发生？本课 `off=5` 的越界写，对应真实内核里的什么 bug？
   为什么宏内核挡不住它，而微内核能？Linux 的内核模块（LKM）让这个问题更轻还是更重？
