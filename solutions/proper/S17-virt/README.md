# 正经·S17 · 虚拟化：H 扩展两阶段地址转换 + trap-and-emulate（软件模型）

> 承接 S13/S15（多核 / SMP）。本课认知 RISC-V **H 扩展**与 **type1 hypervisor**
> 的两个核心机制——**两阶段地址转换**与 **trap-and-emulate**。
>
> ⚠️ labctl 用的 QEMU（`-bios default`，OpenSBI）**不把 H 扩展暴露给 S 态**，没有
> `hgatp/vsatp` 这些 CSR 可用，真起一个 guest 内核超出本课范围。所以本实验是
> **纯软件模型**：在普通 S 态内核里用数组/指针搭出两套二级页表、用一次跨“特权
> 边界”的函数调用模拟陷入，把 type1 的思想跑通并打印 PASS。概念落在 essay。

## 0. 这节课在讲什么

虚拟化要解决两件事：

1. **内存**：guest 以为自己独占物理内存，但它看到的“物理地址”只是**客户机物理
   地址(GPA)**，还得再被 VMM 映射到**宿主物理地址(HPA)**。于是地址翻译从一阶段
   变成**两阶段**：
   ```
   guest VA --[stage-1 / vsatp：guest 自己的页表]--> guest PA
            --[stage-2 / hgatp：VMM 的 G 页表]------> host PA
   ```
2. **特权指令**：guest 内核以为自己在最高特权级，会去读/写硬件 CSR、操作设备。
   VMM 不能让它真碰硬件，于是让这些操作**陷入(trap)**到 hypervisor，由 VMM
   **模拟(emulate)** 出一个合理的结果回填——这就是 **trap-and-emulate**。

H 扩展为此加了一套特权级：**HS**（hypervisor-extended S，跑 VMM）、**VS**
（virtualized S，跑 guest 内核）、**VU**（跑 guest 用户态）。本实验用软件模型
分别演示这两件事，打印 `TWOSTAGE_PASS` / `TRAP_EMU_PASS` / `ALL_PASS`。

## 1. 软件模型怎么搭

- **宿主物理内存** = 一个静态数组 `arena[]`，页对齐分配；HPA 就是 arena 内的地址。
- **stage-1 页表（VS / vsatp）**：guest 自己的二级页表，叶 PTE 的 PPN 字段是
  **客户机物理帧号(GPPN)**。`stage1_walk(gva) -> gpa`（已给）。
- **stage-2 页表（G / hgatp）**：VMM 的二级页表，把 **GPPN 当成被翻译的页号**，
  叶 PTE 的 PPN 字段是**宿主物理帧号(HPPN)**。`stage2_walk(gpa) -> hpa`
  （**你要填**）。
- 完整翻译 `two_stage_translate`：先 stage-1 再 stage-2，串起 GVA→GPA→HPA。

测试：把魔数写进某个宿主帧，建好两套映射，让某个 GVA 一路翻译到那个帧再读回，
读出的值等于魔数 → `TWOSTAGE_PASS`。

trap-and-emulate（**已给**）：模型里 guest 执行 `csrr a0, <CSR>` 退化为调用
`guest_execute_csrr()`，它越过“特权边界”进入 `vmm_emulate_csr_read()`——这就是
一次陷入。VMM 按 CSR 号回填伪值：`VMID` 返回 VMM 给这台 VM 分配的 id；虚拟
`time` 返回**被偏置过**的时间（而非宿主真实 mtime）。guest 拿到伪值且确实经过
拦截 → `TRAP_EMU_PASS`。

## 2. 你要实现的（`kernel/main.c` 的 `stage2_walk`）

照着上面给定的 `stage1_walk`，补全 stage-2 的二级页表查找：

```
i1 = (gppn >> 9) & 0x1FF;  i0 = gppn & 0x1FF;     // gppn = gpa >> 12
pte1 = g_root[i1];                 若 !PTE_V -> *ok=0 返回 0   // 第一级
l0   = pte_ptr(pte1);                                          // 下一级 G 页表
pte0 = l0[i0];                     若 !PTE_V 或 !PTE_LEAF -> *ok=0 返回 0  // 叶
hppn = pte0 >> 10;                 *ok=1;                       // 宿主物理帧号
返回 (hppn << 12) | off;                                       // host PA
```

填之前：`stage2_walk` 安全占位（报未命中），程序能编译、能跑完，但打印
`translation incomplete`、不会出 `ALL_PASS`（`TRAP_EMU_PASS` 仍会出，因为那部分已给）。
填对后三个 PASS 全出。

## 3. 跑 & 自测

```sh
cd kernel && make kernel.elf
make run     # 或 labctl 的 qemu-virt：-smp 4 -bios default
```

期望（参考解）：
```
TWOSTAGE_PASS
TRAP_EMU_PASS
ALL_PASS
```
`kmain` 返回后 `entry.S` 调 `k_shutdown`，QEMU 退出码 0。

## 4. 思考题

见 `essay/THINKING.md`（type1/2/1.5/模拟器的区分、HS/VS/VU、为什么需要两阶段、
trap-and-emulate vs 半虚拟化）。
