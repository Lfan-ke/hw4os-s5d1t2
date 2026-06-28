# 16c · 核内中断 思考题（参考答案）

## 题：核内（core-local）中断 vs 核外（platform-level）中断；为什么 timer 属核内？

围绕 CLINT（Core-Local Interruptor）回答三件事：
1. 「核内」与「核外」中断各指什么，分界线在哪；
2. 为什么 **timer**（mtime/mtimecmp）和 **软件中断**（msip/IPI）天然属于核内；
3. 它和下一课的核外 PLIC（外设 IRQ 路由）有什么本质区别。

---

## 答

### 1. 分界线：中断源「归谁私有」，要不要仲裁与路由

- **核内（core-local）**：中断源是**每个 hart 各一套**、只送达**本 hart**。不存在「该送给哪个核」的问题，
  因此**无需路由、无需多核仲裁、无需 claim/complete 握手**。RISC-V 里由 **CLINT** 提供两类：
  - **timer 中断**（`mtip`，挂在 `mip` CSR 的 bit7）：来自 `mtime` 计数器与本 hart 的 `mtimecmp` 比较器；
  - **软件中断**（`msip`，挂在 `mip` 的 bit3）：来自本 hart 的 `msip` 寄存器，写 1 即拉起。
- **核外（platform-level）**：中断源是**全平台共享的外设**（UART、网卡、磁盘……），物理上只有一条线，
  却可能要送给**任意一个** hart-context，于是需要一个**仲裁/路由器** - RISC-V 的 **PLIC**：
  `priority`（每源优先级）→ `enable`（每 context 使能位图）→ `threshold`（每 context 阈值过滤）
  → `claim/complete`（取走最高优先级源、处理完回写完成）。外部中断挂在 `mip` 的 `meip`（bit11）。
- 一句话分界：**核内 = 私有、点对点、无仲裁；核外 = 共享、需路由、要 claim/complete**。
  这正好对上 `mip` 的三类位：`MSIP`/`MTIP` 是核内、`MEIP` 是核外。

### 2. 为什么 timer 属核内

- **每 hart 一套 `mtimecmp`**：比较器 `mtime >= mtimecmp` 是 per-hart 的 - hart0 的 timer 到点只该打断
  hart0。把它放在共享路由器里反而要回答「这次到点该中断谁」，而答案恒为「拥有这个 `mtimecmp` 的那个核」，
  路由是多余的。所以硬件上就把比较器做进 core-local 单元（CLINT），中断线直连本 hart 的 `mip.MTIP`。
- **与本 hart 的调度时钟强绑定**：操作系统用 timer 中断驱动**本核的**时间片轮转 / 时钟节拍。
  每个核独立调度，就各自需要一个独立、可独立装填下一次到点（`mtimecmp += PERIOD`）的比较器 - 
  这天然是 per-core 私有状态，不该被别的核共享或抢占。
- **无仲裁、无 claim/complete**：本课代码里，handler 收到 `mtip` 后只做两件事 - 计数、把 `mtimecmp`
  往后挪一个 `PERIOD`（这一步**同时**清掉本次 `MTIP`，因为 `mtime` 一时追不上新的 `cmp`），
  就构成周期 timer。没有「向谁申领、处理完通知谁」的握手 - 这正是核内中断的轻量之处。
  （反面：若忘了重装填 `mtimecmp`，`mtime` 永远 `>=` 旧 `cmp`，`MTIP` 一直挂起 → 中断风暴。）

### 3. 软件中断 / IPI：为何也是核内，却能跨核

- `msip` 也是**每 hart 一个**寄存器，拉起的 `MSIP` 只送本 hart - 所以它是**核内**中断源。
- 但「写哪个 hart 的 `msip`」是发起方决定的：核 A 写**核 B 的** `msip` 地址，就给核 B 投了一发
  **核间中断（IPI）**。可见 IPI = 「用核内的软件中断寄存器做跨核投递」 - 
  寄存器本身核内私有，投递动作可跨核。多核唤醒、TLB shootdown、停核都靠它。
- 本课用单 hart 给自己投 2 发 IPI（写 `msip=1` → 处理 → 写 `msip=0` 自清）建立心智模型；
  真正的多核 IPI 与 PLIC 多 context claim 仲裁留到 proper `S06e-ipi`。

### 4. 为什么这些寄存器必须 volatile + fence（回链 16a/16b）

`mtimecmp`/`msip` 不是普通内存：它们的值被**硬件比较器/中断逻辑**实时消费。
若编译器把「写 `mtimecmp`」缓存在寄存器里、或与后续读重排，硬件就可能看不到新的到点值。
所以 C 侧一律 `volatile` 写 + `fence rw,rw`：`volatile` 禁止编译器省略/合并访问，
`fence` 保证「写 cmp」在「硬件按 cmp 比较」之前全局可见 - 这与 16a 的「MMIO 必须直通/uncached」、
16b 的「寄存器模型要 volatile」是同一根因。四语对位：
Verilog `assign mtip=(mtime_r>=mtimecmp_r)` / BSV `method Bool mtip=(mtimeR>=mtimecmpR)` 就是
C 比较表达式、Rust `clint.mtime.get()>=clint.mtimecmp.get()` 的硬件源头；Rust 用 `bitflags` 给
`mip` 的 `MTIP`/`MSIP` 起名、用 `tock-registers` 摆出 CLINT 寄存器图。

### 5. 一句话收束

CLINT 是**核内**中断单元：timer 因「每核私有的 `mtimecmp` + 绑定本核调度时钟 + 无需仲裁路由」而属核内，
软件中断因「每核私有的 `msip`」属核内、又因「可写别核的 `msip`」成为 IPI 的底座。
核外的共享外设 IRQ 才需要 PLIC 的 `priority/threshold/claim/complete` - 那是下一课 16d 的主题。
对标 proper：本课的 timer 是 `S02-trap-timer` 真 CLINT timer 的心智前传，软件中断/IPI 是 `S06e-ipi` 的前传。
