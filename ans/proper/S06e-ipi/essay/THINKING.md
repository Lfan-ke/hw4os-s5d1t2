# S06e 思考题（参考解）

## Q1. S 态内核为什么不能直接写 CLINT 的 MSIP 发 IPI？经 SBI 发 IPI 的完整链路是什么？

CLINT（含 MSIP/mtimecmp）映在 `0x0200_0000` 一段，按 qemu virt 的 PMP/domain 配置只对 **M 态**可见（OpenSBI 的 `Domain0 Region00 ... M:(I,R,W) S/U:()`），S 态内核访问会陷出。MSIP 本质是「机器软件中断挂起位」，写它触发的是 **M 态**软件中断，本就该由 M 态固件管。所以 S 态做 IPI 走 **SBI IPI 扩展**：`sbi_send_ipi`（EID=`0x735049`"sPI"，fid=0，参数 `hart_mask`/`hart_mask_base`）。链路是：S 态 `ecall` → OpenSBI 在 M 态写**目标 hart 的 CLINT MSIP** → 目标 hart 进 M 态软件中断 → OpenSBI 的 IPI 处理把目标的 `mip.SSIP` 置上、清掉 MSIP → 返回 S 态时，目标 hart 见 `sip.SSIP`，若 `sie.SSIE & sstatus.SIE` 则取 **`scause=1`（S 态软件中断）**。本实验目标取到 `scause=0x8000000000000001`，正是「中断位 + code 1」。ack 只需 S 态清 `sip.SSIP`（`csrc sip, 1<<1`，sip.SSIP 是 mip.SSIP 的 S 态别名，可写）；不清则一返回又被重新取走，反复重入。

## Q2. 「多核 PLIC claim 仲裁」具体仲裁了什么？为什么本实验用 barrier+轮询而不是异步抢中断？

仲裁的是「**同一个外设源被多个 hart 的 context 同时使能时，谁来处理它**」。本实验把 UART 源（src=10）使能到引导核与 target 两个 hart 的 S-context（各自 `enable`、`priority=1`、`threshold=0`）。源 pending 后，PLIC 给两个 context 都举起 EIP（外部中断挂起）。`claim`（读 claim 寄存器）是**原子领取**：第一个读到的核拿到 `irq=10` 并把该源**移出 pending、置「服务中」**，于是另一个核随后读到 **0**。这保证「**恰一个 hart** 处理该中断」 - 防多核重复处理。用 **barrier+轮询**是为了**确定性**：异步抢中断要靠两核 EIP 几乎同时、且都真进了 trap，时序不可控、易 flaky；而让两核在屏障（原子计数到 2）处**都确定地各读一次 claim**，PLIC 必然产出「一个赢家（irq=10）+ 一个输家（0）」，`claim_nonzero==1 && claim_zero==1` 成为可复现的硬结果。claim 寄存器的语义与是否经 trap 无关 - 它本质是一次原子领取的 MMIO 读，轮询同样触发同一仲裁。

## Q3. claim/complete 缺了会怎样？赢家读 RBR 这一步在多核场景额外重要在哪？

`claim` 防重（原子移出 pending），`complete`（把 irq 写回 claim 寄存器）防丢（gateway 收到 complete 才允许该源再次转发）。**漏 complete**：源停在「服务中」，以后再也不被转发 - 中断丢失。**漏读设备清源（UART RBR）**：设备侧中断线还拉着，complete 一瞬源又立刻 pending，处理函数被反复重入成**中断风暴**。多核场景里读 RBR 还有一层意义：赢家读 RBR 撤掉**共享**的设备中断线后，**两个 context 的 EIP 一起落下** - 否则即便赢家 complete，输家的 context 可能仍看到 EIP、再抢一轮。所以正确顺序仍是 claim → 读设备清源 → complete，三步缺一不可；本实验赢家读回字节 `0x41`（'A'）即证明设备侧被正确清掉。

## Q4. IPI 的内存序为什么关键？发/收两侧各要怎么排？

IPI 常用来「我改好了共享数据，请你来看/来刷」（TLB shootdown、唤醒调度、远程调用）。若没有内存序，**目标核可能在看到 IPI 后仍读到旧数据**（写未传播、或乱序）。所以发送侧要「**先写数据，`fence rw,rw` 之后再 `sbi_send_ipi`**」，保证 IPI 抵达前数据已可见；接收侧「**进软件中断处理，先 `fence` 再读共享数据**」。本实验的邮箱握手 - target「先写 `hart_id`/置 `armed`」、引导核「先布置好邮箱再 `hart_start`/`send_ipi`」、每步间 `smp_fence()` - 正是这套「先数据后举旗 + 屏障」的最小落地（呼应 16e essay 的 `barrier`/内存序关键字）。`fence` 在弱内存序的多核上不是可选项；关中断只挡本核重入、**挡不住别的核同时改同一物理内存**（S15 已论证）。

## Q5（Rust / tock-registers 视角）. 若用 Rust no_std 把 CLINT/PLIC 写成类型化寄存器图，会怎么抄？为何 runnable 变体仍以 C 为准？

承接 16b 的「抄布局」：CLINT 的 `MSIP` 是一组 per-hart 的 `ReadWrite<u32>`，PLIC 的 `priority[src]`、`enable[ctx]`（位图）、`threshold/claim[ctx]` 各是 `ReadWrite<u32>`。Rust 里用 `tock_registers::register_structs!` 描述偏移与读写性、`register_bitfields!` 给 sie/sip 之类的位（`SSIE OFFSET(1) NUMBITS(1)`、`SEIE OFFSET(9) NUMBITS(1)`、`SSIP OFFSET(1) NUMBITS(1)`），访问处用 `registers.msip[hart].set(1)` / `let irq = registers.claim.get()` - 把本实验 C 里的 `plic_read/plic_write(PLIC_CLAIM(ctx))` 升成**编译期就知道偏移与 RO/WO/RW** 的类型化访问，越权写 RO 直接编译不过。但 IPI 真正越权的那一步（写 MSIP）在 **M 态**，S 态 Rust 同样只能走 `ecall` 调 SBI - 类型化只是把「MMIO 布局」写得更安全，**改变不了特权边界**。本套件 proper 侧的 runnable 变体以 **C 为准**（与 S06/S06c/S13 同构、且本仓库无可离线构建的 Rust no_std + qemu-virt + OpenSBI 脚手架），tock-registers 的写法作为本思考题与 16b 的对位；真要落 Rust 变体，需补一份能 `cargo build` 出 S 态内核 ELF 的链接/启动脚手架，再把上面这套 `register_structs!` 接到同样的 `IPI_PASS/CLAIM_PASS/SMP_PASS` 输出上，逐位与 C 变体一致。
