# 14 · 三态转换：特权级，不过是几根线（软硬同构）

> 不正经赛道 · 第 14 课 —— 你以为"内核态/用户态"是高深的护城河？
> 把硬件扒开看：`cur_priv` 不过是两个触发器存的一个小数字，"有没有权限"就是一个比较器
> `cur_priv >= 需要的等级` 拉出来的**一根线**，"切换特权"就是给触发器写个新值，"开启某功能"
> 就是再点亮一个使能位。本课让你亲手把这几根线连出来——从此对 M/S/U、`sret`、`sstatus.SPP`
> 不再有神秘感：操作系统的"权力"是**物理的**，是**连线**决定的。

## 0. 这节课在讲什么

对标 RISC-V 的 M/S/U 三特权级：**A=最高（≈M）、B（≈S）、C=最低（≈U）**。
你要实现一个纯函数 / 一块组合逻辑 `step(csr, op) → (csr', trap)`：

- `cur_priv` ↔ 当前特权态；
- 比较器 ↔ 取指 / CSR 访问的权限检查（非法即 illegal-instruction 陷入，正是 rcore `03priv_inst` /
  `04priv_csr` 里 U 态执行 `sret` / 写 `stvec` 被杀的那条链路）；
- `saved_priv` ↔ `sstatus.SPP` / `mstatus.MPP`；
- `ECALL` / `XRET` ↔ `ecall` 陷入 + `sret`/`mret` 返回；
- `feat_en` ↔ "置位即开功能"的 CSR 使能（如 `satp` 开 MMU、`mstatus.SUM`）。

## 1. 状态字 `csr` 与操作字 `op`（与 01-hw-vlan 的"包字"同构）

```
 csr[4:0]:  [ 4:3 ] saved_priv   [ 2 ] feat_en   [ 1:0 ] cur_priv
 op:        kind(3b)  arg_priv(2b)  arg_en(1b)
```

| 名称 | 含义 |
| :-- | :-- |
| `cur_priv`   | 当前特权态：`A=2`（最高）、`B=1`、`C=0`（最低）。数值越大权限越高。 |
| `feat_en`    | 功能使能位：1=该类被门控能力已开启。 |
| `saved_priv` | 陷入时保存的前态（三态需 2 位）。 |
| `kind`       | 操作种类（见下表）。 |
| `arg_priv`   | NORMAL/USEFEAT 需要的等级；DROP 的目标等级。 |
| `arg_en`     | SETFEAT 要写入 `feat_en` 的值。 |

## 2. 你要实现的真值表 `step(csr, op) → (csr', trap)`

| `kind` | 语义 | 规则 | `trap` 条件 |
| :-- | :-- | :-- | :-- |
| `NORMAL`(0)  | 执行需 `arg_priv` 权限的普通指令 | 通过则 `csr` 不变 | `cur_priv < arg_priv` |
| `DROP`(1)    | 主动下放到 `arg_priv`            | 置 `cur_priv=arg_priv` | `arg_priv > cur_priv`（不许直接提权）|
| `ECALL`(2)   | 陷入提权（合法）                | `saved_priv=cur_priv; cur_priv=A` | 无 |
| `XRET`(3)    | 从处理程序返回                  | `cur_priv=saved_priv` | `cur_priv != A`（非最高态不得 xret）|
| `SETFEAT`(4) | 置/清功能使能位                 | `feat_en=arg_en` | `cur_priv < B` |
| `USEFEAT`(5) | 用被门控的功能                  | 通过则不变 | `cur_priv < arg_priv` **或** `feat_en==0` |

> 约定：**任何 `trap` 都不改 `csr`**（操作被拒，状态字保持原样，只把 `trap` 线拉高）。
> 真实硬件陷入还会跳 `mtvec`、写 `mcause`，本课把这些抽象掉，只留一根 `trap` 线。

## 3. 五个子实验（逐题递进，同一个 `step` / `priv_gate`）

| 子实验 | 你要填的分支 | 点亮 |
| :-- | :-- | :-- |
| 1 · 特权比较器 | `NORMAL`：`trap = cur_priv < arg_priv` | `CMP_PASS` |
| 2 · 向下放权 = 写低位 | `DROP`：合法才写 `cur_priv`，上行非法则 `trap` | `DROP_PASS` |
| 3 · 陷入提权 + 返回 | `ECALL`（存前态、进 A）+ `XRET`（恢复前态、非 A 则 `trap`） | `TRAP_PASS` |
| 4 · 开功能也是置位 | `SETFEAT`（`cur<B` 则 `trap`）+ `USEFEAT`（特权够**且**使能亮） | `FEAT_PASS` |
| 5 · 三态贯通（capstone） | 不填新逻辑，harness 把前四段拼成一条轨迹逐位校验 | `CAPSTONE_PASS` |

第 5 个的轨迹：**A 启动 → DROP 到 B → SETFEAT 开功能 → DROP 到 C →
C 态 USEFEAT 触发 `trap`（被"内核"接住）→ ECALL 提权处理 → XRET 返回**。

## 4. 四条实现路径（任一把 1–5 全跑出即过；多过加辅助分）

| 路径 | 目录 | 你要填的 TODO |
| :-- | :-- | :-- |
| 软件 · Rust    | `sw/rust/src/main.rs` | `step()` 各 `kind` 分支 |
| 软件 · C       | `sw/c/priv.c`         | `step()` 各 `kind` 分支 |
| 硬件 · Verilog | `hw/v/priv_gate.v`    | 组合逻辑 `priv_gate` 的 `case (kind)` |
| 硬件 · BlueSpec| `hw/bsv/PrivGate.bsv` | 函数 `priv_step` 的 `case (kind)` |

每条路径的**测试向量与 PASS 打印是给好的**（在各自 harness/testbench 里），你只填 `// TODO` 处的
核心逻辑。四条路径输出必须**逐位一致**。通过判据：依次出现
`CMP_PASS`、`DROP_PASS`、`TRAP_PASS`、`FEAT_PASS`、`CAPSTONE_PASS`、`ALL_PASS`，且不出现 `FAIL`。

部分分支给了 `// TODO[a] … // ELSE[b] …` 两条等价写法，择一即可：
- `NORMAL`：`[a]` 一行比较器 `cur < arg`；`[b]` 把 3×3 等级关系展开成显式真值表 case。
- `XRET`：`[a]` 用 2 位 `saved_priv` 存完整前态；`[b]` 仿真实 `sstatus.SPP` 只用 1 位
  （只区分"是否来自最高态"），并在 `THINKING.md` 讨论三态下信息为何不足。

```
labctl run improper/14-privilege     # 跑所有可用变体
labctl hint improper/14-privilege    # 卡住了看提示
labctl watch                          # 边改边自动重跑 + TUI 拓扑/波形
```

## 5. 看硬件（波形 / 结构 / 接口）

```
make -C hw/v sim     # 跑仿真（与判题同口径，0 warning）
make -C hw/v wave    # gtkwave 看波形：盯 csr / kind / csr_o / trap
make -C hw/v synth   # yosys 看综合后的硬件结构——哪根线是比较器、哪些触发器是 cur_priv
make -C hw/bsv sim   # BSV 仿真
```

或用 `labctl` 终端伴侣面板（免 X11）：拓扑 / 数据流 / 波形 / 接口一屏看全。

## 6. 思考题（`essay/THINKING.md` 里作答，写下理解即可）

1. 为什么"向下放权"硬件允许直接写寄存器，"向上提权"却必须经 `ECALL` 这道门？若允许 C 态直接把
   `cur_priv` 写成 A 会发生什么？联系：为什么 `sret`/`mret` 只能"恢复"`SPP`/`MPP` 而不能"任意指定"目标特权。
2. 真实 `sstatus.SPP` 只有 1 位（只够区分 S/U 两态），可我们三态需要 2 位 `saved_priv`。由此推断：
   特权级数量与"保存前态所需位宽""陷入处理复杂度"之间是什么关系？H 扩展再加一层虚拟化特权时，硬件多付出了什么？
3. "开启功能 = 置一个使能位"——举一个真实例子（如 `satp` 开 MMU、`mstatus.SUM`、PMP 锁定位），说明为什么把
   "能力"做成"特权够 **且** 使能位亮"两道与门，比只看特权级更安全 / 更灵活。

## 7. 完成标准 (DoD)

- [ ] 至少一条路径把子实验 1–5 全打出 `*_PASS` 且收尾 `ALL_PASS`，无 `FAIL`（必修）。
- [ ] 硬件变体 0 warning，且其 `priv_gate` 输出与软件逐位一致。
- [ ] 能在波形 / 拓扑里指出哪根线 = 特权比较器、哪些触发器 = `cur_priv`/`saved_priv`/`feat_en`。
- [ ] 能口述"下放权=写低位（自由）、提权=必须经 ECALL 门（陷入）、开功能=置使能位"三句话。
- [ ]（essay）三道思考题写进 `THINKING.md`。

## 8. 引申：从「几根线」到真实特权架构

本课把陷入抽象成单根 `trap` 线、三态状态字只用 5 位，刻意抽掉了 `mtvec`/`mcause`/委托/中断。把这几根线"接全",可往这些方向深入：

1. **补全陷入机制**：把单 `trap` 线扩成"跳 `mtvec` + 写 `mcause`/`mepc`/`mtval` + 按 `medeleg`/`mideleg` 委托到 S 态"，实现真正的分级陷入下放（衔接正经赛道 S02 trap 内核）。
2. **CSR 文件化**：把 5 位状态字铺成真实 `mstatus`/`sstatus` 位域（`MIE`/`SIE`/`MPP`/`SPP`/`MPRV`/`SUM`/`MXR`），实现 `mret` 时 `MIE←MPIE`、`MPP←U` 的中断使能栈与降权语义。
3. **加第四级 + 虚拟化**：按 RISC-V H 扩展加 `VS`/`VU` 态与 `hstatus`、二级地址翻译，亲手验证"特权级数量↑ → 保存前态位宽↑、陷入处理复杂度↑"（呼应思考题 2）。
4. **门控具体化**：把抽象 `feat_en` 落成真实使能——`satp` 开 MMU、PMP/PMA 物理内存保护——做成"特权够 **且** 区域允许"的双重与门，对照 seL4 的 capability 检查。
5. **嵌进取指-执行核**：把 `step` 放进一个最小取指-译码-执行循环，让 `ecall`/`sret` 成为真指令、U 态非法指令真正打 illegal-instruction（直接对照 rcore `03priv_inst`/`04priv_csr`）。
6. **中断与抢占**：在比较器之外加中断 pending/enable 矩阵与优先级、`WFI`，做时钟中断驱动的抢占式切换，看"特权 + 中断"如何共同构成内核的控制权。
