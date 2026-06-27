# 22-namespace 思考题参考答案（essay 变体）

## 1. namespace + cgroup = 容器。这两者各管「隔离」的哪一半？

容器 = **namespace（隔离视图）** + **cgroup（限制资源）**，缺一不可。

- **namespace 管「看见什么」**：给进程换一套**视图**，让它以为自己独占整台机器。
  PID namespace 让容器里看到的进程从 1 号(init) 起编号，看不到宿主的别的进程；
  mount namespace 给它一张私有挂载表，A 容器挂的盘 B 根本不存在；net namespace
  给它独立的网卡/路由/端口……隔离的是**命名与可见性**——同一个内核对象，
  在不同 ns 里要么换了名字(pid 1000↔局部 1)，要么干脆不可见(/data-a 在 B 里查不到)。
- **cgroup 管「能用多少」**：namespace 不限量——容器照样能把内存/CPU 吃光拖垮整机。
  cgroup（control group）给每个容器记一本**配额账**：内存上限、CPU 份额、PID 数量、
  IO 带宽……申请超过 mem 上限就被拒（甚至触发 OOM kill），CPU 超额就被**限流**(throttle)。
  本实验的 `cgroup_charge` 就是这本账最朴素的样子：`used+request<=quota` 才批准，
  否则拒绝且用量不动。

一句话：**namespace 让你「看不见」别人，cgroup 让你「占不了」太多。** 两者合起来，
进程才被关进一个既隔离又有上限的盒子里——这就是容器。

## 2. 容器 vs 虚拟机：到底差在哪？

核心差别一句话：**容器共享宿主内核，虚拟机各跑一份独立内核。**

| 维度 | 容器(container) | 虚拟机(VM) |
| :-- | :-- | :-- |
| 内核 | **共享宿主同一颗内核**，靠 namespace/cgroup 切隔离 | 每台 VM 跑**自己完整的 OS 内核**，由 hypervisor 虚拟硬件 |
| 隔离强度 | 较弱（共享内核 = 共享攻击面，一个内核漏洞通吃） | 较强（硬件级隔离，逃逸要穿透 hypervisor） |
| 启动/开销 | 毫秒级、几乎无额外内存——本质就是几个被打了标记的进程 | 秒级以上、每台多吃几百 MB（整套 OS） |
| 密度 | 单机可塞成百上千个 | 单机几十个就吃紧 |
| 能跑什么 | 只能跑与宿主内核兼容的东西（Linux 容器需 Linux 内核） | 可跑完全不同的 OS（Linux 上跑 Windows VM） |

所以容器「轻」是因为它根本没虚拟硬件、没装第二个内核——它就是一组**被 namespace
换了视图、被 cgroup 限了量的普通进程**。代价是隔离不如 VM 硬。现实里常叠加使用：
Kata Containers / Firecracker microVM 就是「给容器套一层轻量 VM」来补隔离强度。

## 3. OCI / Docker：标准与工具分别是什么？

- **Docker** 是把「namespace+cgroup+联合文件系统(overlayfs)+镜像分发」打包成好用工具的
  那个产品，让「`docker run nginx`」一行就起一个容器。它点燃了容器普及。
- **OCI（Open Container Initiative）** 是为了不被单一厂商绑定而立的**开放标准**，主要两份规范：
  - **image-spec**：容器**镜像**长什么样（分层文件系统 + 配置 JSON + manifest）。
  - **runtime-spec**：拿到一个解包好的 rootfs + `config.json`，**怎么把它跑成容器**
    （建哪些 namespace、设哪些 cgroup 限制、用什么 rootfs）。
  参考实现是 **runc**（Docker 抽出来捐给 OCI 的底层运行时）。
- 分层关系：`docker` / `containerd`（高层守护进程，管镜像拉取、生命周期）→ 调用
  `runc`（OCI runtime，真正去 `clone()`+`setns()`+写 cgroup 把进程关进盒子）。
  Kubernetes 通过 **CRI** 接口对接 containerd，最终也落到 OCI runtime。
  标准化的好处：镜像在 Docker 打、在 Podman / containerd / CRI-O 里都能跑。

## 4. Linux 六种主要 namespace，各管什么？

| namespace | 隔离的资源 | 容器里因此「独立」的东西 | 对应系统调用标志 |
| :-- | :-- | :-- | :-- |
| **PID** | 进程号空间 | 容器内 init=1，看不到宿主进程；同一进程在内外 pid 不同（本实验 (1)） | `CLONE_NEWPID` |
| **Mount(MNT)** | 挂载点表 | 私有的文件系统挂载视图，A 挂的盘 B 看不见（本实验 (2)） | `CLONE_NEWNS` |
| **Network(NET)** | 网络栈 | 独立网卡/IP/路由/端口/防火墙；容器各自的 lo 和 eth0 | `CLONE_NEWNET` |
| **UTS** | 主机名/域名 | 独立 hostname（容器里 `hostname` 改了不影响宿主） | `CLONE_NEWUTS` |
| **IPC** | System V IPC / POSIX 消息队列 | 独立的共享内存段、信号量、消息队列 | `CLONE_NEWIPC` |
| **User** | uid/gid 映射 | 容器内的 root(uid 0) 映射到宿主的普通用户 → 非特权容器 | `CLONE_NEWUSER` |

（另有较新的 **Cgroup namespace** 隔离 cgroup 根视图、**Time namespace** 隔离系统时钟偏移，
常说的「六种」一般指上表。）关键直觉：**每种 namespace 各切一类内核资源的视图**，组合起来
才让进程产生「我独占一台机器」的错觉；再叠加 cgroup 限量，就是一个完整的容器。
本实验只挑了 PID 与 Mount 两种最直观的来「见猪跑」——映射换个号、挂载表换一张，
隔离的本质就这么朴素。
