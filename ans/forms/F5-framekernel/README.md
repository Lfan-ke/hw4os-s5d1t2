# F5 · 框内核(framekernel)：用类型系统替代 MMU 的最小 TCB 隔离

> 形态认知专题 · 第 5 形态 —— host 软件直觉 demo（不是完整内核）。
> 原型：**Asterinas**（蚂蚁 OS Lab，SOSP'25 Best Paper）。
> 一句话母题：**宏内核的单地址空间性能，微内核式的隔离——怎么同时拿到？**
> 答案是把可信计算基(TCB)收敛成一个最小「框架」，框架内用 `unsafe` 把底层能力
> 包成**安全 API**，其余子系统全用安全 Rust，**隔离来自类型系统，不是 MMU**。

## 0. 这节课在讲什么

五形态里，宏/微内核在「性能 ↔ 隔离」上二选一：宏内核同地址空间快但 TCB = 全部内核；
微内核拆地址空间隔离好但 IPC/切换慢。**framekernel** 给出第三条路：

| | 宏内核 | 微内核 | **框内核(framekernel)** |
| :-- | :-- | :-- | :-- |
| 地址空间 | 单一 | 每子系统一个 | **单一**（快、零拷贝） |
| 隔离机制 | 无 | MMU/页表（运行时） | **类型系统**（编译期、零成本） |
| TCB | 全部内核 | 微内核核心 | **最小框架 OSTD**（可审计的 unsafe） |
| 子系统语言 | C | 任意 | **安全 Rust**（`#![deny(unsafe_code)]`） |

Asterinas 把内核劈两半：`ostd/`（OS Framework，允许 unsafe，包安全 API）+ `kernel/`
（OS Services，`kernel/src/lib.rs:8` 即 `#![deny(unsafe_code)]`，全安全 Rust）。

## 1. 你要填的 2 处

软件主路径在 `sw/rust/src/main.rs`（Rust 是本形态的主角，因为它有 borrow checker）；
`sw/c/frame.c` 给出 C 的「约定式」近似。

| 子实验 | 填什么 | 判据 |
| :-- | :-- | :-- |
| 1 安全 API 往返 | `Frame::write` 的**边界检查**封装 | `FRAME_PASS` |
| 2 类型隔离 | 同上检查使越界写被拒、邻居不被改坏 | `TYPESAFE_PASS` |
| 3 最小 TCB 审计 | `check_mintcb` 的 **unsafe 计数**逻辑 | `MINTCB_PASS` |

三段皆过再打印 `ALL_PASS`。失败打印含 `_MISS` 的诊断行（如 `TYPESAFE_MISS`、`MINTCB_MISS`），
不含 `FAIL`/`panic`/`ERROR`。

```
labctl run forms/F5-framekernel      # 跑 rust/c/essay 三条路径
labctl watch                         # 边改边自动判定
labctl hint forms/F5-framekernel     # 卡住看提示
```

## 2. demo 的三条判据怎么演示「本质权衡」

- **FRAME_PASS（安全 API 往返）**：框架 `Pool` 把一块物理 buffer 切成互不重叠的 `Frame`
  句柄；子系统经 `write/read` 安全 API 存取、往返一致。对应 OSTD 的 `Frame`/`VmReader`。
- **TYPESAFE_PASS（类型替代 MMU）**：两个相邻 `Frame`（A=[0,64), B=[64,128)）**共享同一
  物理池**、物理上紧挨。A 越界写 `far=64`（正是 B[0] 的物理位置）——安全 API 的边界检查
  把它挡成 `Err`，B 的哨兵毫发无损。**没有第二张页表，隔离全靠那行 `if i >= len`**。
  对照：若子系统能写裸指针 `*p.add(64)=0xFF` 就会改坏 B——但子系统模块带
  `#![forbid(unsafe_code)]`，那行**根本无法通过编译**（反例写在 `try_overreach` 注释里）。
- **MINTCB_PASS（最小可信基）**：`check_mintcb` 用 `include_str!` 读自己的源码，统计
  `LABCTL-TCB-*` 与 `LABCTL-SUBSYS-*` 两段里的 `unsafe` 数量——框架段 >0、子系统段 ==0。
  这就是「把 unsafe 收敛进可审计的小框架」的可执行版。

## 3. 关键约定（判题用）

- `Frame::write/read`：**先**做 `i >= len` 边界检查（越界 `Err(OutOfBounds)`），**再**落到
  框架内部的裸指针读写。删掉检查 → 安全 API 不再 sound → 越界写改坏邻居 → `TYPESAFE_MISS`。
- 子系统模块 `#![forbid(unsafe_code)]`：编译期保证子系统 0 unsafe；该属性写在
  `LABCTL-SUBSYS-BEGIN` 标记**之前**，不计入审计区段。
- `check_mintcb`：`tcb.matches("unsafe").count() > 0` 且 `sub.matches("unsafe").count() == 0`。
  注意子系统区段内**不要出现英文 `unsafe`**（连注释也不行），否则计数 >0。
- **C 变体是近似**：C 无 borrow checker，用 opaque handle + 受控访问函数做**约定式**封装，
  用运行时计数器 `g_tcb_raw_ops`/`g_subsys_raw_ops` 近似审计。这只是约定、不是强制——
  没有任何机制能阻止 C 子系统强转句柄裸写越权。

## 4. 完成标准 (DoD)

- [ ] 至少一条变体跑出 `FRAME_PASS`/`TYPESAFE_PASS`/`MINTCB_PASS`/`ALL_PASS`，无任何 `_MISS`（必修）。
- [ ] rust 主路径 0 warning（`cargo build`）；c 路径 `gcc -Wall -Wextra -O2` 0 warning。
- [ ] essay 答出「类型系统如何替代 MMU、为何 C 只能近似、最小 TCB + soundness」的要点。
- [ ] 能口述本 demo 的 `Frame` ↔ asterinas OSTD `Frame`、`#![forbid(unsafe_code)]` ↔
      `kernel/src/lib.rs:8` 的 `#![deny(unsafe_code)]`、`framework_unsafe` 计数 ↔ 可审计 TCB。

## 5. 思考题（`essay/THINKING.md` 作答即可通过）

1. framekernel 凭什么「既要又要」：单地址空间的宏内核性能 + 微内核式隔离？类型系统怎么替代 MMU？
2. 为什么 C 做不到，只能靠「约定 / MMU」近似？（类比：C 也没有 async/await，协程只能约定式模拟）
3. 把 unsafe 收敛到框架（最小 TCB）的意义？soundness 为何是硬要求？对照 Rust-for-Linux。
