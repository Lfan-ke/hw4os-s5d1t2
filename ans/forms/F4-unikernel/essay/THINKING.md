# 形态 F4 · 库OS / Unikernel 思考题参考答案（essay 变体）

## 1. 为什么 app→OS 是「直接函数调用、零陷入」？省了什么、又失去了什么？

传统 OS 把内核放在更高特权级（RISC-V S 态 / x86 ring0），应用在用户态。应用每要一次
内核服务（read/write/mmap…）都得执行 `ecall`/`syscall` **陷入**：CPU 切换特权级、保存
用户上下文、查 syscall 表、跑内核处理、再切回来。这道「陷入墙」是隔离的代价。

Unikernel 把 OS 抽象做成**库**，和应用**静态链接成同一个 ELF、跑在同一个地址空间、
同一个特权级**。于是 `app→OS` 退化成一次普通的函数 `call`——没有特权级切换、没有
上下文保存、没有 syscall 表查找。本课用「陷入计数器」把这点演出来：传统模型
`dispatch_trap` 每次服务 `traps += 1`，unikernel 模型 `dispatch_direct` 直接调同一批
`uni_*` 例程，陷入恒为 0；两者干的活完全一样，差别只在那道墙。

**省下的**：模式切换 / 上下文保存恢复 / TLB 与缓存抖动 / syscall 分发开销，因此延迟更低、
吞吐更高（这正是网络数据平面、HFT 等场景看中它的原因）。

**失去的**：**保护边界**。同地址空间意味着 app 的野指针能直接踩烂「内核」数据；
没有用户/内核隔离，一个 bug 就是整镜像的故障域；而且**一份镜像只能跑一个应用**
（单应用），没有多进程隔离、没有 `fork` 出一群互不信任的进程。Unikernel 用
「外面套一层 hypervisor/VM 做隔离」来补这一刀——隔离边界从「进程」上移到了「虚拟机」。

## 2. 为什么「单应用」是「编译期特化裁剪」的前提？换来/失去什么？

通用 OS 必须服务**任意**应用，所以网络栈、块设备、多种文件系统、多进程、信号、ptrace…
都得带着——它不知道下一个跑的程序要用哪些。Unikernel 反过来：**一份镜像只服务一个
已知应用**，于是在**编译期**就能确定「这个应用到底用了哪些子系统」，把没用到的整段
**不链接进去**。本课用模块表 + `is_linked`/`image_symbols` 模拟：app 只用
console/alloc/clock，net/blk/fs 直接不链入，符号数从全量 152 降到特化 30——「镜像变小」。

工程对应：**Unikraft** 把 OS 拆成 88 个微库（ukboot/uksched/uknetdev/vfscore/posix-\*…），
用 buildroot 风的 **Kconfig**（`.config` 决定哪些 `lib/` 参与最终 binary）做编译期裁剪；
**MirageOS** 用 OCaml 的 functoria **类型驱动**配置，按应用声明的设备/协议在编译期组装
模块；**HermitOS** 用 Rust 的 **feature flag** + `cargo` 把 libOS 编成一个静态库再与应用链接。

**换来**：更小的镜像（MB→几百 KB）、更短的启动（毫秒级冷启动）、更小的攻击面
（不在镜像里的代码无法被攻击）、更省内存。
**失去**：通用性与复用——换一个应用、或应用新用到一个子系统，就得**重新配置、重新编译**
一份镜像；调试工具链也更原始（没有 shell、没有 `ps`、没有动态加载）。

## 3. 1995 年就有的思想，为什么 2020 后才靠 serverless / FaaS 二度复兴？

LibOS / Unikernel 的思想 1995 年就由 MIT 的 Exokernel 论文 + jos 教材 OS 提出，2013 年
MirageOS（ASPLOS'13 "Unikernels: LibOS for the Cloud"）把它工业化。但 **2014 年 Docker
容器**抢走了风口——容器复用现成 Linux 生态、迁移成本几乎为零，而 Unikernel 要求
「换语言/换工具链/单应用/难调试」，性价比在当时不划算，被压制了约五年。

2020 后 **serverless / FaaS** 改变了天平：

- **冷启动是核心 KPI**：FaaS 函数按调用计费、随时起停，启动越快越好。Unikernel 没有
  完整 OS 引导、镜像极小，能做到**毫秒级冷启动**——正好命中。AWS Lambda 的 **Firecracker**
  微 VM、Cloudflare Workers 的「每个请求一个轻量隔离」都是这个方向的产物（Workers 用
  V8 isolate，思路同源：极小运行时 + 强隔离 + 快启动）。
- **一镜像一应用正是云函数的形态**：FaaS 函数天然就是「单一、无状态、短生命」的应用，
  和 Unikernel「单应用」假设完美契合，不再是缺点。
- **极小攻击面 = 多租户安全**：云上多租户最怕逃逸。镜像里没有 shell、没有多进程、
  没用到的子系统都不在，攻击面被压到最小。
- **WebAssembly + WASI** 成了「新一代 LibOS」载体，让这套思想以更易移植的形式再普及。

一句话：思想没变，是**场景**变了——云函数的「快启动 + 单应用 + 多租户隔离 + 省资源」
需求，正好把 Unikernel 当年的「缺点」全部变成了「优点」。

## 4. 把本课模型对到真实工程

| 本课模型 | 真实对应 | 工程例子 |
| :-- | :-- | :-- |
| `uni_*` 例程被 app 直接链接 | **静态链接 libOS**：OS 抽象编成库与应用链成一个 ELF | HermitOS 自称 "compiles to a static library"；Unikraft 用 `__weak main` 把应用 `main` 链进内核镜像 |
| `dispatch_direct` 陷入计数=0 | **同地址空间、无 `ecall`**：app→OS 是函数调用而非特权切换 | MirageOS/Unikraft 应用与 OS 同特权级运行，无 user/kernel syscall 边界 |
| `image_symbols` 特化变小 | **编译期裁库**：Kconfig / feature / 类型驱动只链用到的子系统 | Unikraft 的 `.config` 选 88 微库子集；MirageOS functoria 类型驱动组装；HermitOS cargo feature |
| 一镜像 = app + OS 同地址空间 | **单应用 Unikernel 镜像**：含 boot + kernel + app 的一份可启动 binary | Unikraft `build/<image>_<arch>-<plat>`；MirageOS 产出可直接被 hypervisor 启动的 unikernel |

补充权衡口诀：**Unikernel = LibOS 思想（同地址空间无陷入）+ 单应用（可编译期特化）+
一份可启动镜像**；它拿「隔离/通用性」换「性能/镜像大小/启动速度/攻击面」，在 FaaS 时代
这笔交易重新变得划算。
