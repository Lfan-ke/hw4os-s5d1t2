# 16c · 核内中断：CLINT（per-hart timer + 软件中断 / IPI）

> 不正经赛道 · 第 16c 课 - 软件 host/qemu-user 直接跑；硬件走 iverilog/bsc 仿真。
> 一句话母题：**核内中断 = 每个 hart 私有、点对点、无需仲裁路由的中断源。**
> CLINT 给每 hart 两类核内中断源：① **timer**（`mtime` 自走 + 本 hart 的 `mtimecmp` 比较器）、
> ② **软件中断**（本 hart 的 `msip` 寄存器，写 1 即拉起，亦是核间中断 IPI 的底座）。
> 四语言跑**同一场景**，逐位一致打印 `TIMER_PASS` / `SOFT_PASS` / `ALL_PASS`。

## 0. 这节课在讲什么

上一课（16b）你把寄存器布局抄成四语同构的「寄存器模型」；这一课用同一套手法去碰**中断** - 
但先碰最简单、最贴近 CPU 的那一类：**核内（core-local）中断**。它的关键性质是「私有 + 点对点」：

- **timer 中断**：硬件里有个一直在走的 `mtime` 计数器，每个 hart 配一个**自己的** `mtimecmp`。
  比较器 `mtime >= mtimecmp` 一旦成立，就拉起本 hart 的 `MTIP`（`mip` CSR 的 bit7）。
  handler 处理后把 `mtimecmp` 往后挪一个周期（`+= PERIOD`），既清掉本次中断、又约定下一次到点 - 周期 timer。
- **软件中断 / IPI**：每 hart 一个 `msip` 寄存器，写 1 拉起 `MSIP`（`mip` 的 bit3），写 0 自清。
  「写**别的** hart 的 `msip`」就是给那个核投一发**核间中断（IPI）** - 多核唤醒 / TLB shootdown 的底座。

对应真实系统：RISC-V CLINT（QEMU virt 在 `0x0200_0000`）、`S02-trap-timer` 的 timer 中断、
Linux 的 `smp_call_function`/IPI；下一课 16d 讲**核外** PLIC（共享外设 IRQ 的优先级路由）。

## 1. CLINT 寄存器契约（toy）

toy 用紧凑偏移建模（真 QEMU virt：`msip@0x0000`、`mtimecmp@0x4000`、`mtime@0xBFF8`，皆 per-hart）：

| 偏移 | 名 | 访问 | 语义 |
| :-- | :-- | :-- | :-- |
| 0x00 | `MSIP`     | RW | bit0：写 1 拉起软件中断（IPI），写 0 自清 |
| 0x08 | `MTIMECMP` | RW | 64 位 per-hart 比较值；`mtime >= mtimecmp` 拉起 `MTIP` |
| 0x10 | `MTIME`    | RO | 64 位核内自走计数器（由 `tick` 推进；软件只读） |

中断挂起位（`mip` CSR）：`MSIP`=bit3、`MTIP`=bit7（都是核内）；对照核外的 `MEIP`=bit11（PLIC）。

固定场景（四语逐位对照同一组值）：`PERIOD=5`，跑 `NTICK=16` 个 tick →
timer 恰触发 **3** 次（`mtime`=5/10/15，每次 `mtimecmp += 5`）、终值 `mtime=16`；软件中断手动拉起 **2** 次。

## 2. 四语对位的「同一套机制」

| 机制 | Verilog | BSV | C（规矩） | Rust（花活） |
| :-- | :-- | :-- | :-- | :-- |
| timer 比较器 | `assign mtip=(mtime_r>=mtimecmp_r)` | `method Bool mtip=(mtimeR>=mtimecmpR)` | `r64(&mtime)>=r64(&mtimecmp)` | `c.mtime.get()>=c.mtimecmp.get()` |
| 软件中断挂起 | `assign msip=msip_r` | `method Bit#(1) msip=msipR` | `r32(&msip)&1` | `c.msip.get()&1!=0` |
| 寄存器写 | `if(we&&waddr==A_CMP) ...` | `method Action setCmp` | `volatile` 写 + `fence rw,rw` | `tock-registers` `.set()` |
| 中断挂起命名 | `mip` 位 | `mip` 位 | 比较结果 int | `bitflags!` 给 `MTIP/MSIP` 起名 |
| 寄存器图 | 端口 + reg | `interface`/`Reg` | `struct{volatile ...}` | `register_structs! Clint{...}` |

> **C 规矩（volatile + fence）**：`mtimecmp`/`msip` 不是普通内存，被硬件比较器/中断逻辑实时消费。
> `volatile` 禁止编译器缓存或合并访问，`fence rw,rw` 保证「写 cmp」对硬件「按 cmp 比较」全局可见 - 
> 与 16a 的「MMIO 必须直通」、16b 的「寄存器要 volatile」同根。
> **Rust 花活**：`bitflags!` 给 `mip` 的 `MTIP(bit7)`/`MSIP(bit3)` 起名，`tock-registers` 的
> `register_structs!` 摆出 CLINT 寄存器图（`msip@0x0`/`mtimecmp@0x8`/`mtime@0x10`）。

## 3. 里程碑与判题 (DoD)

每个变体跑同一场景，逐位一致打印：

- `sw-c` / `sw-rust`：`TIMER_PASS fires=3 mtime=16` · `SOFT_PASS ipi=2` · `ALL_PASS`。
- `hw-v` / `hw-bsv`：同上三条，另打印 `DEV_PASS`（设备级：比较器在 `mtime>=mtimecmp` 时恰拉 `MTIP`、
  `msip` 寄存器写 1 挂起 / 写 0 清零），且 **0 warning**。

统一 `expect = ["TIMER_PASS","ALL_PASS"]`，`forbid = ["FAIL","panic","ERROR"]`；`SOFT_PASS` 为各变体附加里程碑。

```
labctl run improper/16c-clint    # 跑 sw-c/sw-rust/hw-v/hw-bsv/essay
labctl hint improper/16c-clint   # 卡住看分级提示
```

- [ ] timer 比较器 `mtime >= mtimecmp` 在 16 个 tick 内恰触发 3 次 → `TIMER_PASS`。
- [ ] handler 每次触发后 `mtimecmp += PERIOD` 重装填（同时清本次 `MTIP`），构成周期 timer。
- [ ] 软件中断：写 `msip=1` 拉起、handler 写 `msip=0` 自清，2 发 → `SOFT_PASS`。
- [ ] 硬件 `mtimecmp` 比较器 + `msip` 寄存器 RTL，tb 跑过 → `DEV_PASS`（0 warning）。
- [ ] 软件任一语言全过（必修）；另一语言、两条硬件路径多过计辅助分（按轴）。
- [ ] `essay/THINKING.md`：核内 vs 核外、为何 timer 属核内。

## 4. 关键决策

1. **单 hart 建模，IPI 用「给自己投递」演示**。improper 重在建立核内中断的心智模型：
   timer 比较器 + 软件中断寄存器。真正的多核 IPI 投递与 PLIC 多 context claim 仲裁放到 proper `S06e-ipi`、
   多核 PLIC 放到 16d/16e。
2. **C 走 `gcc-rv64`（真 RISC-V + qemu-user）**：`volatile` 写编译成真 `sw`、`fence rw,rw` 是真内存屏障 - 
   这才讲清「寄存器为何要 volatile+fence」。与 16b 同构。
3. **四语逐位一致**：四变体跑同一场景（`PERIOD=5`/`NTICK=16`），都得到 `fires=3`/`mtime=16`/`ipi=2`，
   打印完全相同的里程碑子串；各语言附加细节（`DEV_PASS` 等）算 bonus、不进统一 `expect`。

## 5. 简化取舍（留完整版作引申）

- toy CLINT 用紧凑偏移（`0x0/0x8/0x10`），真 QEMU virt 偏移（`0x0/0x4000/0xBFF8`）见 §1 与 essay；
  紧凑偏移让 `tock-registers` 的 `register_structs!` 栈上缓冲不至于占几十 KB。
- 单 hart：不建模多核仲裁 / `mtime` 跨核共享时钟域 / 真实 trap 向量 - 那些在 proper `S02`/`S06e`/`S13`。
- `mtime` 由 tb/engine 的 `tick` 推进（软件里是循环里 `mtime+=1`），不接真实晶振；只考比较器与中断语义。
- 软件中断用轮询 `poll_mip` 模拟「取中断挂起位」，不建模真实 trap 进入/`mret` 返回（见 14-privilege / S02）。

## 6. 思考题（`essay/THINKING.md` 作答）

核内（core-local）中断 vs 核外（platform-level）中断：分界线在哪？为什么 **timer**（mtime/mtimecmp）
与**软件中断**（msip/IPI）天然属核内、而共享外设 IRQ 要走核外的 PLIC？为什么 `mtimecmp`/`msip` 必须 volatile+fence？

## 7. 引申（想多走一步）

- **真 CLINT**：把 toy 偏移换成 QEMU virt 的 `0x0200_0000` 基址 + `0x4000`/`0xBFF8`，在 `S02-trap-timer`
  里接真实 trap 向量、`mtvec`/`mstatus.MIE`，看 timer 中断真正打断执行流。
- **多核 IPI**：`-smp 4` 下，hart0 写 hart1/2/3 的 `msip` 投 IPI，被投核在 trap handler 里认领并自清 - 
  这是 `S06e-ipi` 与 16e 中断聚合的核心。
- **中断风暴反例**：故意不重装填 `mtimecmp`，观察 `MTIP` 持续挂起、handler 被反复打断 - 
  理解「周期 timer 为何必须在 handler 里推进 `mtimecmp`」。
