# 12 · 地址空间：软件 MMU 与「偷梁换柱」的稀疏映射

> 不正经赛道 · 第 12 课 —— 纯软件建模、host 直接跑（用软件页表数组冒充物理内存）。
> 一句话母题：**用户程序眼里内存「无限大」，物理机却只有几帧**。你写一层软件 MMU 当中间人，只把真正用到的 `vpn` 偷偷接到几块真 `ppn` 上，对上层假装地址空间无边无际。

## 0. 这节课在讲什么

`translate(va) → pa` 不过是一张「虚拟页号 → 物理页号」的查表函数。把它写出来，你就理解了虚拟内存的「偷梁换柱」：戳 `0x0`、`0x100000`、`0x100000000000` 都行，可底下永远只有 8 帧物理内存。本课用一块软件数组冒充「物理内存」，用函数调用拦截访问来「软件模拟 MMU」——不碰真 `satp`/`sfence.vma`/硬件 PTW/TLB/缺页 trap。

对应真实系统：rcore `PageTable`/`MemorySet`/`map_area`/PTE/`satp`/SV39，xv6 `walk()`/`mappages()`/三级 SV39，以及 MMU/TLB/缺页中断、内核直接映射段（trampoline）、percpu 区、反置页表。

## 1. 五段递进（折叠在一个程序里，逐段点亮 PASS）

| 段 | 你要填的纯函数（`sw/rust` 或 `sw/c`）| 判据 |
| :-- | :-- | :-- |
| **E1 稀疏映射** | `map1` / `translate` | 三个巨址读写一致 **且** 实占帧数 ≤3 → `SPARSE_PASS` |
| **E2 拍卖行** | `route`（直接映射 identity vs 翻译）| 跨空间共享 → `DIRECT_PASS`；私有区隔离 → `EXCHANGE_PASS` |
| **E3 SMP 多槽** | `slot_addr` / `reduce_slots` | 每核错开一槽 → `SLOTS_PASS`；无锁归约求和 → `SMP_PASS` |
| **E4 两级映射** | `map2` / `walk2` | 读回一致 → `WALK_PASS`；二级表数受界 → `TWOLEVEL_PASS` |
| **E5 SV39 草图** | `sv39_walk`（三级 + 解析标志）| 读回一致 + 叶 PTE 标志齐 → `SV39_PASS` |

八个 `*_PASS` 全亮再打印 `ALL_PASS`。下方 harness（向量 + 校验 + PASS 打印）勿改。

```
labctl run improper/12-address-space     # 跑 C/Rust 两条路径
labctl watch                             # 边改边自动判定
labctl hint improper/12-address-space    # 卡住看提示
```

## 2. 几个关键约定

- 地址拆位：`vpn = va >> 12`，`off = va & 0xfff`，`pa = ppn << 12 | off`。
- 物理内存：`NFRAMES = 8` 帧，每帧 512 个 `u64`；`alloc()` 顺序发帧。
- E2 直接映射窗口 `[0x8000_0000, 0x8000_1000)`：identity（`pa == va`），背后是一块两个空间共享的「同一帧」。
- E4：`va = l1(10) | l2(10) | off(12)`，二级表按需新建（用到哪张建哪张）。
- E5 SV39：`va = vpn2(9) | vpn1(9) | vpn0(9) | off(12)`；非叶 PTE 仅 `V`、ppn 字段存下一张表下标；叶 PTE 带 `V/R/W/X/U`、ppn 字段存物理页号。

可二选一的分支：E1 缺页处理 `// TODO[a]` 按需分配 / `// ELSE[b]` 报 fault 由上层 map；E2 `route` 的直接映射 vs 翻译；E4 `walk2` 命中 `// TODO[a]` / 缺失 fault `// ELSE[b]`。

## 3. 完成标准 (DoD)

- [ ] `SPARSE_PASS`：三个巨址读写一致且实占帧数 ≤3（证明巨址没被物理铺开）。
- [ ] `DIRECT_PASS` + `EXCHANGE_PASS`：直接映射区跨空间共享、私有区互不串扰。
- [ ] `SLOTS_PASS` + `SMP_PASS`：多槽无锁归约，求和无丢失。
- [ ] `WALK_PASS` + `TWOLEVEL_PASS`：两级 walk + 按需建表，中间表数量受界。
- [ ] `SV39_PASS` + `ALL_PASS`：三级 walk 取回正确 pa 与叶 PTE 标志位。
- [ ] C/Rust 任一条全过（必修）；另一条也过、essay 提交各计辅助分。

## 4. 思考题（`essay/THINKING.md` 作答即可，反置页表为主线）

1. **反置页表 vs 多级页表**：va 巨大而稀疏、物理内存很小时，两者内存占用与查找成本各如何？反置页表为何用 `hash(pid,vpn)`、牺牲了什么（共享 / 碰撞链）、真实系统里谁用过？
2. **为什么公共交换区要放直接映射段**（`va==pa`）而非各自的虚拟地址？联系 trampoline、E3 的 SMP 槽位。
3. **「偷梁换柱」省了什么**：E1 若真按 `0x100000000000` 物理铺开需多少帧？为什么 NoMMU 机器上仍能靠软件垫片虚拟出一个 MMU？（选答：SV39 为何 39 位/三级/每级 9 位，换 SV48 要改哪里？）
