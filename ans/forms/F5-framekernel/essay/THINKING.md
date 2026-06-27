# F5 框内核(framekernel) 思考题参考答案（essay 变体）

> 原型：Asterinas（蚂蚁集团 OS Lab；论文 arXiv:2506.03876，USENIX ATC'25）。
> `ostd/` = OS Framework（允许 unsafe），`kernel/src/lib.rs:8` = `#![deny(unsafe_code)]`。

## 0. 为什么叫「框」(frame) 内核？

权威来源（Asterinas 论文 + 官方 book）给的命名是双关：

1. **「框」= 中介边界（frame as boundary）**：可信基座 TCB 被画成一个**画框**——非可信代码
   与**下方硬件**、**上方用户态**的一切底层交互，全部**经由这个 TCB「框」中介**。不可信代码被
   「框」在中间，凡碰硬件/用户态都得穿过这层框；隔离的本质，就是「框」把守了所有出入口。
2. **「框」= 框架（framework）**：这层最小 TCB 就是 **OS Framework**，实现叫 **OSTD**
   （取意「OS 开发的 Rust 标准库」）——其余子系统都建在这个框架之上。

所以 framekernel 不是单/微/外/库内核里的任何一种，而是一种**正交的新结构**：同地址空间
（宏内核性能）+ 全 Rust + 一个最小可信「框」(OSTD) 把守 unsafe，其余 `forbid(unsafe_code)`。
内存安全 TCB 仅约 14%，性能比肩 Linux、兼容 210+ Linux syscall。

## 1. framekernel 凭什么「既要又要」：单地址空间的宏内核性能 + 微内核式隔离？类型系统怎么替代 MMU？

宏内核把所有子系统塞进**同一个地址空间**，调用就是普通函数调用、传指针零拷贝，所以快；
代价是**没有隔离**——任何一个子系统的野指针都能踩坏整个内核，TCB = 全部内核代码。
微内核把子系统拆进各自的**地址空间**（用户态进程），靠 MMU/页表硬隔离，TCB 很小；
代价是跨子系统通信要走 **IPC + 上下文切换 + 数据拷贝**，慢。

framekernel 的洞见是：**隔离不一定要靠 MMU**。它让整个内核仍住在同一个地址空间（保住
宏内核的性能、零拷贝、直接函数调用），但把内核劈成两半：

- **OS Framework（OSTD）**：唯一允许 `unsafe` 的最小框架，把裸指针/端口/页表等底层能力
  封装成**安全 API**（`Frame`、`VmSpace`、`VmReader/VmWriter`……），并保证这些 API
  *soundness*（无论怎么调都不会 UB）。
- **OS Services（kernel/）**：所有具体子系统（syscall/fs/net/driver），**全用安全 Rust**，
  顶部 `#![deny(unsafe_code)]`，只能经框架的安全 API 碰资源。

于是「隔离」从**运行时的 MMU 检查**前移成了**编译期的类型检查**：

- 安全 API 的边界检查（本 demo 的 `Frame::write` 越界返回 `Err`）让子系统**拿着句柄也越不过
  自己那段内存**——这对应 MMU 的「越界 → 缺页」，但发生在编译期/受控函数里，没有页表 walk、
  没有 TLB miss、没有 trap，所以**零运行时开销**（zero-cost abstraction）。
- borrow checker + 所有权让「别名 + 可变」这种数据竞争在编译期就被拒，等价于微内核「各自地址
  空间互不可见」的效果，但同样是编译期、免切换。

一句话：**MMU 是用硬件页表在运行时画边界；framekernel 是用类型系统在编译期画边界。**
后者把「隔离」做成了零成本抽象，于是同时拿到宏内核的速度与微内核的小 TCB。
本 demo 里两个相邻 `Frame` 物理上紧挨（同一个 `Vec`/`buf`），却谁也改不了谁——靠的不是
两套页表，而是 `write` 里那一行边界检查 + 子系统模块的 `#![forbid(unsafe_code)]`。

## 2. 为什么 C 做不到，只能靠「约定 / MMU」近似？（类比：C 也没有 async）

framekernel 的隔离**全靠编译器强制**两件事：

1. 子系统模块**禁用 unsafe**（`#![deny/forbid(unsafe_code)]`）——任何裸指针解引用、
   任意转型越权，直接**编译不过**。
2. 安全 API 自身 sound——框架作者审计那一小块 unsafe 即可。

C 两件都给不了：

- **没有 unsafe 边界**：C 里**每一次指针解引用都是「unsafe」**，编译器不区分「框架」和
  「子系统」。我可以把 `Frame` 句柄 `(uint8_t*)f->base` 强转后 `base[1000] = x` 直接越权，
  编译器一声不吭。本 demo 的 C 版只能用 **opaque handle + 受控访问函数 + 运行时计数器**做
  **约定式**封装——子系统**自觉**只调 `frame_write/read`，审计是运行时统计而非编译期保证。
  纪律一旦破坏（有人偷偷裸写），没有任何机制拦得住。
- 要在 C 里拿到**硬**隔离，只能退回 **MMU/页表**：把子系统拆成独立进程、靠页表 + 系统调用
  边界——这恰恰就是 framekernel 想避开的微内核开销（IPC、切换、拷贝）。

这和「C 没有 `async/await`」是同构的：协程的「在 `await` 处挂起 / 恢复、编译器自动切栈帧」
在 Rust/C++ 是**语言级**能力，编译器生成状态机；C 只能用 `ucontext`/手写状态机/回调来
**约定式模拟**，既不安全也不零成本。**语言把某种纪律变成编译期可强制的属性**——
framekernel 之于隔离，正如 async/await 之于协程：缺了这层语言能力，C 只能退化成约定或更重的
运行时机制。

## 3. 把 unsafe 收敛到框架（最小 TCB）的意义？soundness 为何是硬要求？对照 Rust-for-Linux。

**意义：memory-safety TCB 最小化、可审计。** 整个内核可能上百万行，但只有 OSTD 那一小块
允许 unsafe。要论证「内核没有内存安全漏洞」，**只需审计框架那几千行 unsafe**，其余几十万行
安全 Rust 由编译器背书。本 demo 的 `AUDIT framework_unsafe=N subsystem_unsafe=0` 就是这个
心智模型的缩影：越权能力被钉死在框架里，子系统恒为 0。

**soundness 是硬要求**：安全 API 的承诺是「无论调用方怎么用都不会 UB」。只要框架里有**一个**
unsound 的 safe API（比如忘了边界检查、或暴露了能造出悬垂引用的接口），整个「子系统是安全的」
论证就崩塌——一颗老鼠屎坏一锅汤，TCB 的边界形同虚设。所以 OSTD 的四条硬性要求里，
**Soundness（zero unsoundness goal）排第一**，其次才是 Expressiveness / Minimalism / Efficiency。
本 demo 里 `Frame::write` 那行边界检查就是「soundness 责任」的具体落点：删掉它，安全 API
就不再 sound，子系统的越界写会真的改坏邻居（TYPESAFE 子题演示的就是这个失效）。

**对照 Rust-for-Linux**：RFL 是把 Rust 嵌进一个**本质 unsafe 的 C 内核**——Rust 驱动要跟
C 核心打交道，大量接口仍得 `unsafe` 包裹、TCB 边界模糊（C 那半边随时能破坏内存安全），
得到的是「**局部**更安全的驱动」。framekernel 是**clean-slate**：从地基（OSTD）就用 Rust
定义好安全 API，子系统**整体** `deny(unsafe_code)`，TCB 边界清晰且可证。一句话：RFL 是在
不安全地基上盖安全房间，framekernel 是先浇一块小而可信的地基、整栋楼都安全。
