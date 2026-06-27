# S17 思考题 · 虚拟化与 RISC-V H 扩展

用自己的话回答下面四问（每问 3-5 句即可），把 `LABCTL_ESSAY_TODO` 那一行替换成你的作答后再交。

## Q1 type1 / type2 / type1.5 / 模拟器(emulator) 怎么区分？各举一例。

（提示：VMM 是否跑在裸机上、是否依赖宿主 OS、是否要求 guest 与 host 同架构；
Xen / ESXi、VirtualBox / VMware Workstation、Linux+KVM、QEMU-TCG / Bochs）

## Q2 RISC-V H 扩展引入了哪些特权级？两阶段地址转换为什么必要？

（提示：HS / VS / VU 各跑什么；vsatp 管 guest VA→guest PA，hgatp 管 guest PA→host PA；
guest 看到的“物理地址”其实不是真物理地址）

## Q3 trap-and-emulate 是什么？它和半虚拟化(paravirtualization) 各自取舍？

（提示：陷入→模拟→回填 vs 改 guest 用 hypercall；改不改 guest、陷入开销、virtio）

## Q4 本实验为什么只能做“软件模型”，真做 type1 VMM 还差什么？

（提示：labctl 的 QEMU + OpenSBI 把内核引到 S 态但不开放 H 扩展/HS 态；
真做还需 hgatp/vsatp 编程、hedeleg/hideleg 委派、hfence、解码并模拟 guest 指令、虚拟设备）

---

LABCTL_ESSAY_TODO: 在此写下你的作答（替换本行）。
