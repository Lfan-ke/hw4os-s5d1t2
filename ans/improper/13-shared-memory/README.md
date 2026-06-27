# 13 · 共享内存：一份物理字节的两个主人（进程↔进程 · 设备↔OS）

> 不正经赛道 · 第 13 课 —— 软件 host 直接跑；硬件走 iverilog/bsc。
> 一句话母题：**让两个映射指向同一块物理字节——一处写、另一处立刻看见**。
> 这条心智模型从「进程↔进程」一路推到「设备↔OS（MMIO）」：本质相同——
> **同一份物理字节 + 一套不踩脚的协调协议**。

## 0. 这节课在讲什么

上一课（地址空间）你把虚拟页映射到物理页。这一课的「啊哈」：**两个 PTE 指向同一个 PPN**，
就是共享内存的本质（页表别名）。把它推到极致——进程与进程能这样共享，CPU 上的 OS 与一个
硬件设备也能这样共享（因为「OS 所在的 CPU 本身就是一台物理设备」）。

对应真实系统：POSIX `shm_open`/`mmap(MAP_SHARED)`、System V `shmget`、virtio 的 vring、
网卡/DMA 描述符环、SMP 直接映射区的核间共享槽。

> 简化取舍：页表用扁平数组 `[Pte; N]` 建模，不做真 Sv39/satp/TLB（借地址空间课的结论）；
> 「进程」= 一对（页表，帧视图）；邮箱用回合制驱动使竞态确定可复现；硬件 mailbox 是定深 RAM 环、
> 轮询 `avail`，无中断、无真 DMA。完整版（真 shm/vring/带中断描述符环/多核缓存一致性）留作引申。

## 1. 五段逐题递进

| # | 子实验 | 你填什么 | 判据 |
| :- | :-- | :-- | :-- |
| 1 | 页表别名 | `map()` + 把 `vpn_a`/`vpn_b` 映射到同一 `ppn` | 经 va_a 写、va_b 读到 → `ALIAS_PASS`；不同 ppn 互不可见 → `ISOLATED_PASS` |
| 2 | mmap 共享/私有 | `do_mmap()` 的 SHARED / PRIVATE 分支 | 同 key SHARED 互见 → `SHARED_PASS`；PRIVATE 只见零页 → `PRIVATE_PASS` |
| 3 | 邮箱握手 | `producer_step()` / `consumer_step()` | 置位后才读到完整 payload、无撕裂读 → `MAILBOX_PASS` |
| 4 | 共享环（结构体） | `ring_push()` / `ring_pop()` | 按序排空、满拒绝/空 None、环绕正确 → `RING_PASS` |
| 5 | 共享环（MMIO 语义） | 复用上面的 push/pop | 设备↔OS 收发一致 → `MMIO_SHM_PASS` |

软件（rust/c）覆盖 1–5；硬件（verilog/bsv）覆盖 4–5 的共享环（同一套逻辑写成时序 `always` /
纯函数）。全部通过再打印 `ALL_PASS`。

## 2. 数据模型

```
struct Pte { valid: bool, ppn }                 // 页表项；别名 = 两个 PTE 同一 PPN
translate(pt, va) = pt[va/PAGE].ppn*PAGE + va%PAGE   // 已给
World { phys[], next_ppn, reg: key->ppn }        // 帧池 + 命名段注册表
Mailbox { data[N], ready }                        // 握手：写在前、置 ready 在后
Ring { buf[CAP], head, tail, count }              // count 法判空/满，%CAP 环绕
```

包语义（硬件 ring_mbox）：`push_en/push_data` 设备写、`pop_en/pop_data` OS 读、
`avail=count>0`、`full=count==CAP`。

## 3. 关键真值（共享环 count 法）

```
push(x): count==CAP → 拒绝(false)；否则 buf[tail]=x; tail=(tail+1)%CAP; count++
pop()  : count==0   → 空(None)；   否则 v=buf[head]; head=(head+1)%CAP; count--; → v
avail  = count>0      full = count==CAP
```

握手次序（载荷点）：生产者**先写满 data，最后才置 `ready`**；消费者**仅当 `ready` 才拷贝**。
颠倒次序会被探针抓到（置位前不得被读到）。

## 4. 怎么跑

```
labctl run improper/13-shared-memory     # 跑 rust/c/verilog/bsv/essay
labctl watch                             # 边改边自动判定
labctl hint improper/13-shared-memory    # 卡住看提示
```

手动（与判题同口径）：

```
# rust
cd sw/rust && cargo run -q
# c
gcc -Wall -Wextra -O2 sw/c/shm.c -o /tmp/shm && /tmp/shm
# verilog（0 warning）
iverilog -g2012 -Wall -o /tmp/s hw/v/*.v && vvp /tmp/s
# bsv（0 Warning）
cd hw/v && make sim     # 或 cd hw/bsv && make sim
```

## 5. 完成标准 (DoD)

- [ ] `ALIAS_PASS`：一处写、另一处读到（同一物理字节）；`ISOLATED_PASS`：不同 PPN 互不可见。
- [ ] `SHARED_PASS` / `PRIVATE_PASS`：区分 MAP_SHARED 可见与 MAP_PRIVATE 隔离。
- [ ] `MAILBOX_PASS`：消费者只在标志置位后读到完整 payload，无撕裂读。
- [ ] `RING_PASS` / `MMIO_SHM_PASS`：head/tail 环绕与空/满判定正确；硬件 0-warning 且行为逐条一致。
- [ ] `ALL_PASS`：任一变体全过（必修）；多过计辅助分。
- [ ] 能一句话说清「进程间共享」与「设备-OS 共享」是同一模型（思考题）。

## 6. 思考题（`essay/THINKING.md` 作答即可通过）

1. 共享内存（零拷贝）vs 消息传递（拷贝）：各自的同步复杂度、失败模式与适用场景，各举一例说明谁更划算。
2. 论证「OS 所在的 CPU 也是一台物理设备」，从而进程↔进程共享与设备↔OS（MMIO）共享是同一模型；
   两种场景里的「协调方/仲裁者」分别是谁？
3. 两个 CPU 核、或 CPU 与 DMA 设备共享同一物理内存时，cache 一致性与 DMA 一致性会出什么问题？
   为什么 SMP 多核的「拍卖行」槽位要放在直接映射区，而不是各自的虚拟地址空间里？
