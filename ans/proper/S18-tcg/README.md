# 正经·S18 · mini-TCG（动态二进制翻译 + 虚拟化形态辨析）

> 承接 S17（H 扩展 / type1 VMM）。S17 让同 ISA 的 guest「原生跑、只陷特权操作」；
> 本课走另一条路：**异 ISA 没法原生跑**，于是每条 guest 指令都得**翻译**——这就是
> 模拟器（cross-arch qemu 的 TCG）干的事。我们在 S 态内核里搭一个微缩版翻译器。

## 0. 这节课在讲什么

把「虚拟化形态」一次理清，再亲手实现「解码 → 翻译 → 执行」：

- **虚拟机（VM）= 同 ISA**：guest 与 host 指令集相同，guest 大部分指令**原生**在 CPU 上跑，
  只有特权操作 `trap-and-emulate`。
  - **type-1** 裸机 hypervisor：直接坐在硬件上、是机器上最底层的软件（Xen / ESXi / RISC-V H 扩展上的 KVM）。
  - **type-2** 宿主型 VMM：作为**宿主 OS 上的一个普通应用**运行（VirtualBox / VMware Workstation）——
    它本质**仍是虚拟机**（同 ISA、硬件辅助），只是多了一层宿主 OS。
  - **type-1.5**：宿主内核模块把宿主内核「升格」成 hypervisor（Linux KVM、bhyve），介于 1/2 之间。
- **模拟器 / 仿真器 = 异 ISA**：guest 与 host 指令集**不同**，没有一条能原生跑，
  于是**每条 guest 指令都要解释或翻译**。**跨架构 `qemu-system-*`（TCG 模式）就是模拟器**：
  它的 Tiny Code Generator 把 guest 机器码**动态翻译**成 host 机器码，按基本块翻译执行。

> 一句话：VM 让 CPU 替你跑（同 ISA），模拟器让软件替 CPU 跑（异 ISA）。
> 本实验做的是模拟器那条路的微缩版。

## 1. mini-TCG 流水线（与 qemu TCG 同构）

```
guest insn word --decode--> IR(struct Insn) --translate--> host op(struct HostOp) --run--> guest 寄存器
```

1. **decode**（`tcg_decode`）：把 32 位 guest RISC-V 字拆成字段（opcode/rd/funct3/rs1/rs2/funct7/imm）。
2. **host 微操作**：每个 guest opcode 对应一段「翻译后的 host 代码」（这里是 C 微操作函数）。
3. **translate**（`tcg_translate`）：按解码出的 op 选 host 微操作（即「代码生成」）。
4. **run**（`tcg_run`）：顺序执行翻译块，作用在 guest 寄存器堆上，`x0` 恒为 0。

> 真 TCG 生成的是**真 host 机器码**并跳进去执行；我们生成 C 函数指针微操作。
> 生成物不同，但 `decode → IR → host code → run` 的骨架完全一致。

支持的 guest 指令子集（RV64I）：`addi`（含 `li` = `addi rd,x0,imm`）/ `add` / `sub` / `and` / `or`。

## 2. 你要实现的（`kernel/tcg.c`）

- `tcg_decode(raw)`：字段提取 + opcode 匹配，填 `struct Insn{op,rd,rs1,rs2,imm}`。
  - 字段：`opcode=raw[6:0]`，`rd=raw[11:7]`，`funct3=raw[14:12]`，`rs1=raw[19:15]`，
    `rs2=raw[24:20]`，`funct7=raw[31:25]`。I 立即数用 `(int32_t)raw>>20` 算术右移（符号扩展）。
  - 区分：`0x13/funct3=0`→`addi`；`0x33/funct3=0/funct7=0x00`→`add`、`0x20`→`sub`；
    `funct3=7`→`and`、`funct3=6`→`or`。
- 几条 host 微操作（`h_add`/`h_addi` 等）：算出 `rd = rs1 op rs2`（或 `+imm`），`rd==0` 时丢弃写。

`tcg_translate` / `tcg_run` 与解码字段表已给。

```
labctl run proper/S18-tcg
make -C kernel run     # 手动跑（OpenSBI banner 后见内核输出）
```

判据：输出含 `DECODE_PASS`（解码字段对）/ `EXEC_PASS`（翻译块算出的寄存器对）/ `ALL_PASS`，
不出现 `panic` / `FAIL` / `UNEXPECTED`。

## 3. 完成标准 (DoD)

- [ ] `tcg_decode` 对 7 条样例字正确还原 op + 寄存器号 + 立即数 → `DECODE_PASS`。
- [ ] 翻译执行 guest 小程序后寄存器堆与期望一致（x3=42、x4=40、x5=37、x6=32、x7=37）→ `EXEC_PASS`。
- [ ] 能说清：type2 为什么仍是**虚拟机**（同 ISA、硬件辅助），跨架构 qemu 为什么是**模拟器**（异 ISA、逐条翻译）。

## 4. 引申

- **基本块翻译与缓存**：真 TCG 以基本块为单位翻译，缓存翻译结果（translation block cache），
  下次命中直接跳过解码 + 生成——这是动态翻译相对纯解释器（逐条 `switch`）的核心提速点。
- **真代码生成**：把 host 微操作换成真发射 host 机器码（或先降到 TCG 中间 IR 再后端生成），
  即可逼近 qemu 的实现。
- **更全的 ISA**：补 load/store（要 guest 内存模型）、分支（要 guest PC + 块链接 chaining）、
  CSR / 特权指令（这才接回 S17 的 trap-and-emulate）。
- **VM vs 模拟器混合**：同 ISA 时优先硬件虚拟化（VM 路径），异 ISA 段或不可虚拟化指令再回落到 TCG——
  这正是现代虚拟化栈的分层。
