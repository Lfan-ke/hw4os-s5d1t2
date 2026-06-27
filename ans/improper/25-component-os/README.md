# 25 · 组件化 OS：内核不必从头写，把组件当积木按需组装

> 不正经赛道 · 第 25 课 —— 纯软件心智模型，host 直接跑。
> 一句话母题：**内核不是一块从头雕的整石头。把可复用的「组件」(分配器/调度器/控制台)
> 做成统一接口的积木，用「特性开关」按需组装——同一套组件，不同拼法就拼出不同形态的 OS。
> 这正是 arceos 的精髓：cargo features 组装出 unikernel / 宏内核 / hypervisor。**

## 1. 这节课在讲什么

传统印象里写 OS 是「从 bootloader 一路手搓到 shell」。arceos 给的另一种心智模型是：
内核 = 一堆**组件**（axalloc 分配器、axtask 调度器、axconsole 控制台、axfs 文件系统…），
每个组件有**统一接口**、**可独立替换**；上层用 **cargo features** 这样的「特性开关」声明
「我要装哪些组件、每个用哪种实现」，构建系统就把它们 wire 成一个具体内核。

于是「形态」不再是从头重写，而是**同一套组件的不同组装结果**：

- 只装 `console + alloc`、app 和 OS 直链同地址空间、无 syscall 边界 → **Unikernel** 形态；
- 再装上 `sched`、隔出 syscall 边界 → **宏内核 (Monolithic)** 形态；
- 换一个分配器/调度器实现，OS 照常工作 → 组件**热替换**。

本课在一套「函数指针 vtable 组件 + 特性开关组装」的最朴素软件模型上，把这四件事演出来。
你不写真内核，而是用三个组件 vtable + 一个 `build_kernel(cfg)` 组装器，把「按需组装、
不同形态、可替换」的直觉建起来。

## 2. 你要实现什么

三个组件，每个是「名字 + 函数指针」的 vtable，各有 1~2 个可替换实现：

| 组件 | 接口 | 实现 |
| :-- | :-- | :-- |
| `Allocator` | `alloc(state, n) -> off`（OOM 返回 -1） | `bump`（给定）/ **`freelist`（你填）** |
| `Scheduler` | `run(tasks, out) -> len`（执行序列） | `fifo`、`rr`（均给定） |
| `Console` | `write(len, n) -> n` | `plain`（给定） |

四段判据与你要填的**两处**：

| 函数 | 角色 | 判据 |
| :-- | :-- | :-- |
| 组件 + 注册表 `make_*`（已给） | 组件可独立用 / 按特性可选 | `COMPONENT_PASS` |
| **`build_kernel(cfg)`（你填 ①·组装/wiring）** | 按特性开关选实现并接线 | `COMPOSE_UNI_PASS` / `COMPOSE_MONO_PASS` |
| **`freelist_alloc`（你填 ②·替换一个组件实现）** | 换上替换实现，OS 仍工作 | `SWAP_PASS` |

四段皆过再打印 `ALL_PASS`。两种形态只是两份 `KernelConfig`：

```
UNI  = { alloc=Bump,  sched=NoSched, syscall=false }   # 最小组件 + 直链 + 零陷入
MONO = { alloc=Bump,  sched=Fifo,    syscall=true  }   # 更多组件 + syscall 边界 + 有陷入
SWAP = { alloc=FreeList, sched=Rr,   syscall=true  }   # 同源组件、换两个实现
```

```
labctl run improper/25-component-os     # 跑 rust / c 两条路径
labctl watch                            # 边改边自动判定
labctl hint improper/25-component-os    # 卡住看提示
```

## 3. 完成标准 (DoD)

- [ ] `COMPONENT_PASS`：bump/console/fifo/rr 组件各自独立可用，注册表按特性选对实现（已给，编译即过）。
- [ ] `COMPOSE_UNI_PASS`：`build_kernel(UNI)` 拼出 unikernel 形态——无调度器、无 syscall 边界、全程 0 陷入。
- [ ] `COMPOSE_MONO_PASS`：`build_kernel(MONO)` 拼出宏内核形态——装调度器、过 syscall 边界（4 次调用 = 4 次陷入）、进程跑完。
- [ ] `SWAP_PASS` + `ALL_PASS`：把分配器换成你填的 `freelist`、调度器换成 `rr`，同一份工作负载下 OS 级不变量仍成立。
- [ ] 能讲清「组件化(方法论) 与 forms(形态) 是什么关系：unikernel 只是一种组装结果」（essay 思考账本）。

## 4. 关键约定（判题用）

- **组件接口**：三个组件都是 vtable（`name` + 函数指针）。注册表 `make_allocator(kind)` /
  `make_scheduler(kind)` 按枚举返回对应实现——这就是「按特性选实现」的接口面，是组件**可替换**的根。
- **组装 = `build_kernel(cfg)`**：`alloc = make_allocator(cfg.alloc_kind)`；
  `has_sched = (cfg.sched_kind != NoSched)`，`sched = make_scheduler(cfg.sched_kind)`；
  `syscall_boundary = cfg.syscall`。把选好的组件塞进 `Kernel`。
- **形态差别只在 cfg**：`kcall` 是 app→OS 的统一入口，`syscall_boundary` 为真才 `traps += 1`。
  UNI 不装调度器、`traps` 恒 0；MONO 装调度器、4 次调用 4 次陷入。**同一批组件函数被调用，差别只在那道陷入墙**。
- **OS 级不变量（与用哪个实现无关）**：① 分配区间两两不重叠（`bump` 紧凑、`freelist` 按 64 槽，皆满足）；
  ② 调度器把每个任务的 `burst` 步全跑完（`fifo` 顺序、`rr` 轮转，皆满足）。SWAP 换了组件这两条仍成立 = 可热替换。
- 失败会打印含 `FAIL` 的诊断行（如 `TRAP_LEAK_FAIL` unikernel 直链却产生陷入、
  `SWAP_FAIL` 换实现后不变量被破坏、`COMPOSE_MONO_FAIL` 形态没装对组件）。
  `FORM_UNI`/`FORM_MONO`/`HOTSWAP` 是信息行，非判据。

## 5. 引申：从「函数指针玩具」到真实组件化内核

本课把组件化压缩成最小可演示形态：三个组件、运行期 **vtable（函数指针）** 组装、
特性开关只是 `KernelConfig` 里几个枚举字段、不变量只验「不重叠 + 跑完」。真实的 arceos
组件化在三个维度上比这丰满得多，按兴趣往下走：

1. **把组装从运行期搬到编译期**：本课 `build_kernel(cfg)` 在运行期用函数指针选实现；真实
   arceos 用 **cargo features + crate 依赖图**在**编译期**就把组件 wire 死（没选的组件根本不进
   二进制）。试着用 Rust **trait 对象**或泛型替换裸函数指针，体会「零成本抽象」与 vtable 间接调用的取舍。
2. **加更真实的组件实现**：分配器从 `bump`/`freelist` 扩到 **buddy（伙伴系统）/ slab**（对照
   arceos `axalloc`、Linux SLUB）；调度器从 `fifo`/`rr` 扩到**优先级 / CFS / EDF**（对照 `axtask`）。
   关键是：只要组件契约（接口 + 不变量）不变，换实现不动其余系统——这正是 `SWAP` 想说的事。
3. **补缺的组件，拼出更多形态**：加 `axfs`（文件系统）、`axnet`（协议栈，接上 21/23 课）、
   `axdriver`（设备）。再加一种 `KernelConfig` 拼出 **hypervisor 形态**（对照 arceos 的 `axvm`/
   AxVisor），印证母题「同一套组件、不同拼法拼出 unikernel / 宏内核 / hypervisor」。
4. **把 syscall 边界做真**：本课 `syscall_boundary` 只是 `traps += 1` 的计数；真实形态差别在
   **地址空间隔离 + 特权级切换**（unikernel app/OS 同 ring，宏内核要 U/S 态陷入）。对照 `proper/S8`
   的真实 syscall、forms-F4 的「同地址空间零陷入」，把那道「陷入墙」从计数器换成真页表/特权级。
5. **接口稳定性与版本治理**：组件能热替换的前提是**契约稳定**。看看 arceos 怎么用
   `crate_interface`（声明接口与实现解耦）、版本号约束依赖；想想契约一旦要改（加个方法）时
   全体实现怎么平滑迁移——这是「组件化」从玩具走向工程的真正难点。
6. **读一个真项目落地**：**StarryOS** 在 arceos 组件之上拼出宏内核 + Linux 兼容层——同一套
   `axalloc/axtask/axhal`，换个拼法、补一层 syscall 翻译就成了能跑 Linux 程序的 OS。对照本课
   `make_*` ↔ feature 选实现、`build_kernel` ↔ 依赖图组装、`SWAP` ↔ 替换一个 mod。

## 6. 思考题（`essay/THINKING.md` 作答即可通过）

1. 「内核组件化 + cargo features 组装」相比「从头手搓一个内核」，省了什么、换来了什么？为什么同一套组件能拼出 unikernel / 宏内核 / hypervisor 三种形态？
2. 组件化是**方法论**，forms（F1~F5 那些架构形态）是**结果**。请说明「unikernel 只是一种组装结果」：对照 forms-F4，本课的 `UNI = {最小组件 + 无 syscall 边界}` 与 F4 的「同地址空间、零陷入」是不是一回事？
3. 「可热替换」为什么重要？拿 arceos 的 `axalloc`（换 bump / slab / buddy）、`axtask`（换 fifo / cfs 调度）举例：组件契约（接口 + 不变量）不变时，换实现为何不影响其余系统？这和本课 `SWAP`（换 freelist+rr）是同一回事吗？
4. 把本课模型对到真实工程：`make_allocator(kind)` ↔ ?（cargo feature / Kconfig 选实现）、`build_kernel(cfg)` ↔ ?（按 feature 组装 crate 依赖图）、`SWAP` ↔ ?（替换一个 mod 实现）。再说说 StarryOS（在 arceos 组件上拼出的宏内核 / Linux 兼容层）是怎样「复用同一套组件、换个拼法」的。
