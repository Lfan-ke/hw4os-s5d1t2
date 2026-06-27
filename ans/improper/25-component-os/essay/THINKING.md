# 25 · 组件化 OS 思考题参考答案（essay 变体）

## 1. 组件化组装 vs 从头手搓：省了什么、为什么同源能拼出三种形态？

「从头手搓」每做一个新形态都要重写一遍 boot / 内存 / 调度 / 驱动，重复劳动巨大、容易
各自长歪。**组件化**把内核拆成一组**有统一接口、可独立替换**的组件（arceos 的 `axalloc`
分配器、`axtask` 调度器、`axhal` 硬件抽象、`axconsole`、`axfs`、`axnet`…），上层用
**cargo features** 这样的「特性开关」声明「装哪些组件、每个用哪种实现」，构建系统据此把
依赖图 wire 成一个具体内核。本课用三个 `vtable` 组件 + `build_kernel(cfg)` 把这道工序演出来。

**省下的**：不重复造轮子——同一个分配器/调度器组件被所有形态复用；新形态 = 写一份新 cfg +
（必要时）补一两个组件，而不是从头重写。**换来的**：可组合性、可替换性、可裁剪（不装的组件
不进镜像）。代价是要先付一笔「定义清晰接口契约」的设计税，且过度抽象会有一层间接开销。

**为什么同一套组件能拼出 unikernel / 宏内核 / hypervisor**：因为这三种形态的差别**不在组件本身，
而在「装哪些 + 怎么连」**：

- **Unikernel** = 最小组件子集（console + alloc），app 与 OS 直链同地址空间、**不要 syscall 边界**
  → 本课 `UNI = {Bump, NoSched, syscall=false}`，`kcall` 不计陷入。
- **宏内核** = 再装上调度器（多进程）、隔出 **syscall 边界**（特权切换）
  → `MONO = {Bump, Fifo, syscall=true}`，每次 `kcall` 过墙 `traps+1`。
- **Hypervisor** = 在 HAL 组件上再装一层 vCPU / 两阶段地址翻译 / vm-exit 处理组件，把「应用」换成
  「客户机」。arceos 的 `axvm`/guest 系列 app 正是这么在同一套组件上拼出来的。

同源、不同拼法——这就是 arceos「一套组件，多种形态」的精髓。

## 2. 组件化是方法论、forms 是结果：unikernel 只是一种组装结果

forms 专题（F1 宏内核 / F2 微内核 / F3 外核 / F4 unikernel / F5 多内核…）讲的是**架构形态**——
长成什么样、有没有保护边界、谁在哪个地址空间。组件化讲的是**怎么把它造出来**的方法论。
两者是「结果」与「手段」的关系：**forms 的每一种形态，都可以是同一套组件的一种组装结果。**

对照 forms-F4：F4 的本质是「OS 当库被 app 直接链接、同地址空间、零陷入、单应用可编译期特化」。
本课的 `UNI = {最小组件 + 无 syscall 边界}` 跟它**是一回事**——

- F4「同地址空间、app→OS 是直接函数调用」↔ 本课 UNI `syscall_boundary=false`，`kcall` 直接路由到
  组件函数、`traps` 恒 0；
- F4「单应用」↔ 本课 UNI `NoSched`（不装调度器，不需要多进程）；
- F4「编译期特化裁剪、不用的子系统不链入」↔ 本课「不装的组件不进 Kernel」（cfg 没选就不 wire）。

区别只是视角：F4 站在「形态」角度描述结果，本课站在「组装」角度描述手段。一句话——
**unikernel 不是一种独立技术，而是「用最小组件、去掉 syscall 边界」这种组装方式的产物**。
同样地，把 cfg 改成「多组件 + syscall 边界」就得到宏内核形态（F1）。形态是组装的函数。

## 3. 为什么「可热替换」重要？组件契约不变时换实现为何不破坏系统？

可替换性是组件化最实在的红利：**只要接口契约（函数签名 + 行为不变量）不变，换实现对其余系统透明。**

- arceos `axalloc`：分配器可在 **bump / slab / buddy / tlsf** 之间换。契约是「`alloc(n)` 返回一段
  不与现存分配重叠的内存、`dealloc` 归还」。上层 `Box`/`Vec`/内核数据结构只依赖这条契约，不关心
  底层是线性 bump 还是伙伴系统——换了分配策略，碎片/速度变了，但「不重叠」这条不变量没破，系统照跑。
- arceos `axtask`：调度器可在 **fifo / round-robin / cfs** 之间换。契约是「每个就绪任务最终都会被调度、
  跑完它的工作」。换 fifo→cfs 改变的是公平性/延迟，但「任务都能跑完」不变，业务正确性不受影响。

这跟本课 `SWAP` 是**同一回事**：把 `Allocator` 从 `bump` 换成 `freelist`（按 64 字节槽分配，策略完全
不同）、`Scheduler` 从 `fifo` 换成 `rr`，只要两条 OS 级不变量——**分配区间不重叠、每个任务 burst 步全
跑完**——仍成立，`kcall` 和 `sched.run` 这些调用方一行都不用改，OS 仍工作。契约把「用什么实现」和
「依赖这个实现的人」**解耦**了，这正是组件化能热替换的根。

## 4. 把本课模型对到真实工程 + StarryOS

| 本课模型 | 真实对应 | 工程例子 |
| :-- | :-- | :-- |
| `make_allocator(kind)` 按枚举选实现 | **cargo feature / Kconfig 选实现**：用特性开关挑组件的哪个版本 | arceos `axalloc` 的 `feature = ["bump"/"slab"/"buddy"]`；exercise-altalloc 就是换分配器实现的练习 |
| `build_kernel(cfg)` 组装 | **按 feature 组装 crate 依赖图**：构建系统据 features wire 出具体内核 | arceos `axstd` 的 `features=["alloc","paging","multitask"]` 决定链入哪些 ax* 模块 |
| `SWAP` 换 freelist+rr | **替换一个 mod 的实现**而不动调用方 | arceos `axtask` 换调度算法、`axhal` 换平台后端（qemu-virt / 真板子） |
| `UNI` vs `MONO` 两份 cfg | **同一套组件拼不同形态** | arceos 的 `app-helloworld`(unikernel) 与 `app-guestmonolithickernel`(宏内核/虚拟化) 共用底层模块 |

**StarryOS**：它不是从零写的宏内核，而是**站在 arceos 组件之上**再拼一层——复用 arceos 的
`axalloc`/`axtask`/`axhal`/`axfs` 等组件，在上面加「进程 / 地址空间隔离 / Linux 兼容 syscall 层」
这组组件，组装出一个能跑 Linux 用户态程序的宏内核。同一套底层组件，arceos 默认拼成 unikernel，
StarryOS 换个拼法（多装隔离 + syscall 兼容组件）就拼成 Linux 兼容宏内核——正是本课
「同源组件、换个 cfg、换出另一种形态」在真实项目里的样子。

口诀：**组件化是手段，形态是结果；接口契约不变，实现随便换；同一套积木，cfg 决定它是哪种 OS。**
