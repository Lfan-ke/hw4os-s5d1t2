# S18 思考题（参考答案）

## Q1. type-1 / type-2 / type-1.5 / 模拟器 各是什么？为什么 type-2 仍是「虚拟机」而跨架构 qemu 是「模拟器」？

先抓住分界线：**guest 与 host 是不是同一个 ISA**。

- **虚拟机（VM）= 同 ISA**。guest 指令集与物理 CPU 相同，所以 guest 的绝大多数指令可以
  **直接在真 CPU 上原生执行**，只有「会破坏隔离」的特权操作（敏感指令、访问特权 CSR、I/O）
  才被陷入并由 hypervisor 模拟，即 **trap-and-emulate**（有硬件虚拟化扩展时更彻底）。
  - **type-1（裸机型）**：hypervisor 直接坐在硬件上，是机器上最底层的软件，没有底层宿主 OS。
    例：Xen、VMware ESXi、RISC-V H 扩展之上的 KVM。
  - **type-2（宿主型）**：VMM 作为**宿主 OS 上的一个普通应用程序**运行，借宿主 OS 管硬件。
    例：VirtualBox、VMware Workstation。
  - **type-1.5**：一个宿主内核模块把宿主内核**升格**成 hypervisor——平时是普通 OS，
    需要时内核自己变身管理层。例：Linux **KVM**、FreeBSD bhyve。介于 1 与 2 之间。
- **模拟器 / 仿真器（emulator）= 异 ISA**。guest 指令集与 host **不同**（如 host x86 跑 guest RISC-V），
  没有一条 guest 指令能原生跑，于是**每条都得软件解释或翻译**。

**为什么 type-2 仍是虚拟机？** 因为它判定靠的是「同 ISA / guest 原生执行」，而不是「在不在宿主 OS 上」。
type-2 只是把管理层做成了宿主 OS 上的应用，guest 指令照样原生上 CPU、特权操作照样 trap-and-emulate——
本质仍是虚拟机，不做指令翻译。

**为什么跨架构 qemu 是模拟器？** 因为 guest ≠ host ISA，CPU 不认识 guest 指令，
qemu 的 **TCG（Tiny Code Generator）** 必须把 guest 机器码**动态翻译**成 host 机器码再跑——
这是「软件替 CPU 跑」，是模拟而非虚拟化。（注：同 ISA 时 qemu 也能 `-enable-kvm` 走 VM 路径；
是不是模拟器看它走没走 TCG 翻译，而非看是不是 qemu。）

> 一句话：**VM 让真 CPU 替你跑（同 ISA），模拟器让软件替 CPU 跑（异 ISA）。**

## Q2. 动态二进制翻译（TCG）和纯解释执行（逐条 switch）差在哪？「mini-TCG」省了真 TCG 的什么？

- **纯解释器**：取指 → 解码 → 大 `switch` 派发 → 执行，每跑一条都重来一遍解码 + 派发，开销固定且高。
- **动态二进制翻译**：以**基本块**为单位，把 guest 指令一次性**翻译成 host 代码**并**缓存**
  （translation block cache）；同一块第二次执行直接跳进缓存的 host 码，跳过解码与代码生成。
  热路径上「翻译一次、原生执行多次」，所以远快于逐条解释。还能做块链接（block chaining）、
  常量折叠等优化。
- **本实验省了什么**：我们把「翻译产物」做成了 **C 函数指针微操作**，而真 TCG 发射的是
  **真正的 host 机器码**（或先降到 TCG IR 再由后端生成）。我们也没做翻译块缓存、块链接、
  guest 内存 / PC / 分支。但 `decode → IR → 选 host 操作（代码生成）→ run` 这条流水线骨架
  与真 TCG 完全一致——这正是本课要打通的认知。

## Q3. 把 mini-TCG 扩成能跑真 guest 内核，缺口在哪？哪一步又把我们带回 S17？

按缺口从小到大：

1. **更全的指令**：load/store（需 guest 内存模型与地址翻译）、分支 / 跳转（需 guest PC + 块链接）、
   乘除、立即数高位（lui/auipc）、CSR 指令。
2. **guest 状态完整化**：PC、内存、特权级、CSR 寄存器组——目前只有一个 32 槽寄存器堆。
3. **翻译缓存与自修改代码处理**：缓存翻译块，guest 改了自己的代码要让对应缓存失效。
4. **设备模拟**：guest 内核要 UART / 时钟 / 中断控制器，模拟器得给它们建模（MMIO 派发到模拟设备）。
5. **特权与陷入**：guest 执行特权 / 访 CSR / 触发异常时，模拟器要按 guest 架构语义改 guest 的特权状态、
   跳 guest 的 trap 向量——**这就是 trap-and-emulate**，正是 S17 在「同 ISA + 硬件 H 扩展」下做的事。
   区别在于：S17 靠硬件陷入真实地发生（VM 路径），mini-TCG 里这一切都在翻译 / 执行循环中**软件判定**
   （模拟器路径）。两条路在「特权操作要被截获并模拟」这一点上殊途同归。
