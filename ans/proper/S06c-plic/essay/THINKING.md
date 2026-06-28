# S06c 思考题（参考解）

## Q1. PLIC 与 CLINT 分工有何不同？为什么外设中断要专门设一个 PLIC？

CLINT（Core-Local Interruptor）管的是**每个 hart 本地**的两类中断：机器/监管定时器（mtime/mtimecmp，S 态 `scause=5`）与软件中断/IPI（`scause=1`）。它们数量固定、目标就是「本核」，由 `sie.STIE/SSIE` 直接开关，不需要仲裁。PLIC（Platform-Level Interrupt Controller）管的是**全局外设**中断（UART、磁盘、网卡……，S 态 `scause=9`）：外设数量多、优先级不同、还要决定「送给哪个 hart 的哪个特权级」。把它们集中到 PLIC，就能统一做**优先级仲裁、按 context 使能、阈值屏蔽、claim/complete 防重防丢**。所以一个是 CPU 自带的本地小硬件，一个是可路由「多源→多目标」的外部仲裁器；S02 走的是前者，本课补的是后者。

## Q2. claim/complete 这套握手是干嘛的？少了它会出什么问题？

`claim`（读 claim/complete 寄存器）做两件事：返回当前该 context 下优先级最高的待决源 ID，并**原子地把这个源移出 pending、置为「服务中」**。后者保证：即使多个核都被同一源唤醒，只有第一个 claim 到的核拿到非零 ID，其余拿到 0——**防重**。`complete`（把 ID 写回同一寄存器）告诉 PLIC「这次处理完了」，gateway 才会重新允许该源转发下一次中断——保证后续中断**不丢**。若不 complete，gateway 停在「服务中」，该源以后再也不会被转发（丢中断）；若 complete 了但没先去**读 UART RBR 清掉设备侧的中断线**，则 complete 一瞬间源又立刻 pending，处理函数被反复重入形成**中断风暴**。所以正确顺序是：claim → 读设备清源 → complete，三步缺一不可。本实验用 `trap.c` 里的计数守卫给风暴兜底，但正解就该一次处理干净（外部中断计数==1）。

## Q3. PLIC 的 priority / threshold / enable / context 各自的作用？为什么本实验要按当前 hartid 配置？

- **priority[src]**：每个源一个优先级，**0 表示禁用**，非 0 才可能被路由；多源同时待决时按它仲裁。
- **enable[context]**：位图，决定「这个 context 关不关心这个源」。
- **threshold[context]**：阈值，只放行 `priority > threshold` 的源；调高它可临时屏蔽一批低优先级中断。
- **context**：= (hart, 特权级)。qemu virt 上 hartN 占两个 context：`2N`=M 态、`2N+1`=S 态。中断最终是送给某个 context 的。

因为 PLIC 是 **per-context** 的，使能/阈值/claim/complete 都针对具体 context。而 `-smp 4` 下 OpenSBI 选哪个 hart 作启动 hart 是**不确定的**（每次可能不同）。所以内核必须用「自己正在运行的 hartid」算出 `S-context = 2*hartid+1`，去配置和服务自己这一份——这正是 xv6 `plicinithart()` 的做法。hartid 由 SBI 在跳入 S 态时经 `a0` 传入（`entry.S` 未触碰 a0，故 `kmain` 首句即可取到）。若写死 hart0 的 context，一旦启动 hart 不是 0，中断就既不会被路由到本核、claim 也读不到，全套失败。

## Q4. 现代「消息中断」（MSI / RISC-V AIA）相比线式 PLIC 有什么动机？

线式 PLIC 用专用中断线 + 中断号，源多了走线和路由表都膨胀，且 claim/complete 要 MMIO 往返、虚拟化时每次都要陷出。MSI（Message-Signaled Interrupt，PCIe 早已采用）改成**设备往一个约定内存地址写一个 word** 来「发」中断，省掉专用线，天然支持海量向量、易于按 CPU 路由。RISC-V 的 AIA 用 **APLIC**（线式设备的桥）+ **IMSIC**（每 hart 的消息中断收件箱）实现，中断直接以 CSR（`stopei` 等）领取、对虚拟化友好（可把 IMSIC 直通给客户机）。ARM 世界的对应物是 GIC（GICv3 的 ITS 即做 MSI 路由）。本实验先把最基础的线式 PLIC + claim/complete 打通，理解了这套语义，再看 MSI/AIA 就是「把同一套优先级/路由/握手搬到消息与 CSR 上」。
