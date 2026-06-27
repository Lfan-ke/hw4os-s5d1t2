# S17 思考题 · 虚拟化与 RISC-V H 扩展（参考作答）

## Q1 type1 / type2 / type1.5 / 模拟器(emulator) 怎么区分？各举一例。

- **type1（裸机 / bare-metal hypervisor）**：VMM 直接跑在硬件上，自己就是最底层
  那一层，guest 跑在它之上。没有宿主 OS。例：Xen、VMware ESXi、KVM 在“硬件→KVM
  →guest”视角下的裸机部分、RISC-V 上跑在 HS 态的自研 VMM。延迟低、隔离好，常用于
  数据中心。
- **type2（宿主型 / hosted）**：VMM 是跑在一个**通用宿主 OS** 上的普通程序（或内核
  模块 + 用户态进程），借宿主 OS 管硬件。例：VirtualBox、VMware Workstation、
  QEMU+KVM 里的 QEMU 用户态部分。本质上虚拟机是“宿主 OS 上的一个进程”。安装方便，
  但多一层宿主 OS 开销。
- **type1.5**：介于两者之间——宿主是个**被裁剪/特化、主要用来跑 VM** 的 OS，它既像
  通用 OS 又主职 hypervisor。典型说法：Linux + KVM（Linux 既是通用 OS 又通过 KVM
  把自己变成 hypervisor），有人把它归 1.5。
- **模拟器(emulator)**：不依赖“guest 与 host 同架构 + 硬件虚拟化”，而是用**软件
  逐条解释或动态翻译**指令，可**跨架构**。例：QEMU 的 TCG 模式（在 x86 上跑
  RISC-V）、Bochs。慢但通用，是 S18 mini-TCG 的主题。

一句话区分：虚拟化（type1/2/1.5）要求 guest 指令**大多直接在原生硬件上跑**，只在
特权点陷入；模拟器**不要求同架构**，靠翻译/解释执行每条指令。

## Q2 RISC-V H 扩展引入了哪些特权级？两阶段地址转换为什么必要？

H 扩展把 S 态“一分为二”，形成：
- **HS（Hypervisor-extended Supervisor）**：跑 VMM/hypervisor，拥有 H 系列 CSR
  （`hgatp`、`hstatus`、`hedeleg/hideleg`、`hvip` 等）。
- **VS（Virtualized Supervisor）**：跑 **guest 内核**，它以为自己是 S 态，用的
  `sstatus/satp/...` 实际被映射到 `vsstatus/vsatp/...`。
- **VU（Virtualized User）**：跑 guest 的用户态程序。
（M 态不变，仍在最底层。）

**为什么两阶段**：guest 内核自己要管理“它的”虚拟内存，会建并切换页表
（`vsatp`），把 **guest VA → guest PA**。但 guest 看到的“物理地址”不是真的物理
地址——VMM 给每台 VM 划了一块宿主内存。于是必须再加一层 **guest PA → host PA**
的映射，由 VMM 用 `hgatp` 指向的 **G-stage 页表**控制。两阶段让 guest 完全自主管
理自己的页表、感知不到自己被挪了位置，同时 VMM 牢牢掌握客户机物理内存到真实内存
的落点（隔离、迁移、超分都靠这层）。真机上甚至 stage-1 页表的每次访存都要再走一
遍 stage-2（嵌套页表遍历），本实验软件模型为收敛只把最终 GPA 过 stage-2，并把
嵌套遍历的细节留作认知。

## Q3 trap-and-emulate 是什么？它和半虚拟化(paravirtualization)各自取舍？

- **trap-and-emulate**：让 guest 的敏感/特权操作（读写硬件 CSR、访问设备 MMIO、
  执行某些特权指令）**陷入**到 VMM，VMM 解码后**模拟**出一个结果回填给 guest，
  guest 对此无感（保持“我在裸机上”的错觉）。优点：guest **不用改源码**，未经修改
  的内核就能跑；缺点：每次陷入都有上下文切换开销，频繁陷入（如高频读 time、密集
  MMIO）会很慢；而且经典 x86 上有些指令“敏感但不陷入”，需二进制翻译补救（RISC-V
  H 扩展把这些设计成会陷入，干净很多）。
- **半虚拟化(paravirtualization)**：**修改 guest 内核**，让它知道自己在虚拟机里，
  把昂贵操作换成对 VMM 的显式调用（hypercall），还能批量化。优点：少陷入、性能高
  （Xen PV、virtio 设备、KVM 的 paravirt clock/PV-IPI 都是）；缺点：要改 guest，
  不能跑闭源/未改造的系统。实务里两者混用：CPU/内存用硬件辅助的 trap-and-emulate
  （VT-x/H 扩展），I/O 用半虚拟化的 virtio。本实验 ② 模拟的就是 trap-and-emulate
  里“读虚拟化 CSR → 陷入 → VMM 回填伪值”这一拍。

## Q4 本实验为什么只能做“软件模型”，真做 type1 VMM 还差什么？

labctl 的 QEMU 用 `-bios default`（OpenSBI）把内核引到 **S 态**，但并未给 S 态
开放 H 扩展（没有可用的 `hgatp/vsatp/hstatus` 语义、没有把我们置于 HS 态）。真做
type1 VMM 还需要：① 平台/固件支持并进入 HS 态；② 用真实 H-CSR 建 stage-2 页表
（`hgatp`）、给 guest 建 `vsatp`；③ 配 `hedeleg/hideleg` 把异常/中断按需委派给
VS；④ 用 `hfence` 维护两阶段 TLB；⑤ 在 HS 的 trap 入口里真正解码陷入的 guest
指令并模拟（含 `hlv/hsv` 访问 guest 内存）；⑥ 给 guest 准备虚拟设备（virtio）。
这些都依赖硬件/固件把我们放到 HS 态，超出 qemu-virt + OpenSBI 默认配置，故本课用
等价的软件数据结构与函数调用把**机制与数据流**讲清楚，把真实 H-CSR 编程留作扩展。
