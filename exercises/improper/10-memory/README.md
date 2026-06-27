# 10 · 内存管理：分层、Swap 与统一地址空间

> 不正经赛道 · 第 10 课 —— 软件 host 直接跑；硬件做 10.5 的地址译码器。
> 一句话母题：**内存只是一层会"骗人"的缓存，慢设备才是真身。**

## 0. 这节课在讲什么

你有两块"硬盘"：一块小而快、一块大而慢却断电不丢。本课把**同样两块设备**（= 两个软件数组），按三种"想要"拼出三种内存系统：

```
struct Dev { data: [u64; N], per_access, cost }   // 1 块 = 1 个 u64
FAST: 小 / 快(每次访问 +1)  / 断电清零(易失)
SLOW: 大 / 慢(每次访问 +10) / 断电保留(持久)
```

| 场景 | 想要 | 怎么拼 | 判据 |
| :-- | :-- | :-- | :-- |
| 一 | 速度 + 持久 | 工作集搬到 FAST 上算，结果持久回 SLOW | `SCENARIO_A_PASS` |
| 二 | 容量 + 持久 | 小 FAST 当帧、SLOW 当 swap，缺页换入换出 + 脏页回写 | `PAGEIN_PASS` `SWAPOUT_PASS` `SYNC_PASS` |
| 三 | 够大 | 两块焊成一条平坦大内存，线性地址译码到 (dev,off) | `UNIFIED_PASS` / 硬件 `DECODE_PASS` |

对应真实系统：rcore `frame_allocator`/`PageTable`、xv6 `kalloc`/`walk`、Linux swap 分区与 `kswapd`/`vmscan` 换页、`fsync`/ordered journaling、以及 NUMA/CXL 分层内存。

## 1. 你要填的 5 处核心逻辑

软件（`sw/rust/src/main.rs` 或 `sw/c/mem.c`）：

| # | 函数 | 要求 | 判据 |
| :-- | :-- | :-- | :-- |
| ① | `blk_read` / `blk_write` | 累加访问代价后读/写块；容量=数组长度 | `DEV_PROBE_PASS` |
| ② | `stage_in` / `stage_out` | SLOW↔FAST 搬数据；计算只碰 FAST | `SCENARIO_A_PASS` |
| ③④ | `translate` / `sync_all` | 缺页换入；帧满 FIFO 换出+脏页回写；同步刷盘 | `PAGEIN_PASS` `SWAPOUT_PASS` `SYNC_PASS` |
| ⑤ | `addr_route` | `la<fast_size`→FAST，否则 SLOW（off=la-fast_size） | `UNIFIED_PASS` |

硬件（`hw/v/mem_decode.v` 与 `hw/bsv/MemDecode.bsv`）：填组合逻辑 `mem_decode` / `decode`，把 `la` 译码成 `{cs_fast, cs_slow, local_off}`，与软件 `addr_route` 逐位等价 → `DECODE_PASS`。

五处全过，软件打印 `ALL_PASS`；硬件译码全对也打印 `ALL_PASS`。

```
labctl run improper/10-memory      # 跑全部变体
labctl watch                       # 边改边判
labctl hint improper/10-memory     # 卡住看提示
```

## 2. 关键简化（建立心智模型，不是真实现）

- 用"访问代价计数器"建模快/慢，不做真实时序——速度差异可被打印验证。
- 单级页表（数组式 PTE），无多级、无 TLB；`translate()` 是 NoMMU 上的**软件 MMU 垫片**，由 harness 显式调用。
- swap 槽号 = `vpn` 直接映射，省去槽分配器/空闲链。
- 1 页 = 1 块；帧极少（4 帧）、工作集 = 8 页，几次访问即触发换出。
- 淘汰到 FIFO（`// TODO[a]`）/ Clock（`// ELSE[b]`）即可，单 dirty 位，无精确 LRU。
- 场景三只做两段拼接，不做交织/条带。

## 3. 完成标准 (DoD)

- [ ] `DEV_PROBE_PASS`：容量读数正确且 `cost(FAST) < cost(SLOW)`（分层成立）。
- [ ] `SCENARIO_A_PASS`：结果持久（reboot 后从 SLOW 读回正确）且计算全程在 FAST 上。
- [ ] `PAGEIN_PASS`：缺页被正确换入。
- [ ] `SWAPOUT_PASS` + `SYNC_PASS`：工作集 >> 帧数仍读写正确；reboot 从 SLOW 恢复一致。
- [ ] `UNIFIED_PASS` / `DECODE_PASS` + `ALL_PASS`；硬件 0 warning。
- [ ] C/Rust 任一全过即必修达成；另一条与硬件、essay 多过计辅助分。

## 4. 引申：从最小分层到真实内存子系统

本课为建立心智把"快/慢两块设备 + 几行换页"压到极简。想更接近真实内存子系统，可沿这些方向深入：

1. **真页替换算法**：把单 dirty 位的 FIFO/Clock 换成精确 LRU、Second-Chance、WSClock 或 Clock-Pro（access+dirty 两位），再对照 Linux 的 active/inactive 双 LRU 链表 + `kswapd` 水位线（low/high watermark）触发回收，量化各算法在本课工作集下的缺页率差。
2. **真 swap 槽分配器**：把 `slot = vpn` 直映射换成空闲槽位图/空闲链，支持多 swap 区、swap 满处理、swap readahead 预读与簇式（cluster）回写，理解为何 Linux swap 用 cluster 而非逐页。
3. **多级页表 + 软件 TLB**：把数组式单级 PTE 升级成 SV39 三级表（衔接 12 课），在 `translate` 前加一层软件 TLB 缓存并统计命中率，看 TLB 如何摊薄走表成本。
4. **异步块层 + page cache**：把同步 `blk_read/write` 换成带 DMA/中断的块设备，`stage_in/out` 走 page cache + 后台 writeback 线程，显式区分 clean/dirty/writeback 三态（衔接驱动/fs 课）。
5. **崩溃一致性**：把 `sync_all` 拆成"数据屏障 + 元数据屏障"两步，做 ordered/journaled 回写，在两步之间注入掉电点，验证恢复后是否一致——这正是 `fsync` 与 ordered journaling 要解决的问题。
6. **N 级分层内存**：把两块设备扩成 L1/L2/DRAM/CXL/NVMe 多级并填真实延迟数，做 NUMA/CXL 的冷热页迁移与 `numa_balancing` 式自动 promote/demote，理解"分层内存"在数据中心的现实意义。

## 5. 思考题（essay · `essay/THINKING.md` 作答即可通过）

1. 工作集略大于帧数 vs 远大于帧数，性能差异为何是"断崖"（**thrashing**）？联系 `swappiness`/OOM 与"内存只是慢设备缓存"。
2. 为什么"持久"必须 `sync`/write-back，且回写存在**顺序**要求（数据页 vs 元数据）？`sync_all` 前掉电会怎样？联系 `fsync`/ordered journaling。
3. 三场景分别对应真实系统的什么？为什么不"全用快的"也不"全用慢的"？软件 `translate()` 垫片相比硬件地址译码器各省/费了什么？
