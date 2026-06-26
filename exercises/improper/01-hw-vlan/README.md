# 01 · 硬件管理：VLAN Tag 的插入 / 剥离 / 过滤（软硬同构）

> 不正经赛道 · 第 1 课 —— OS 的起源之一：**软件能做的，硬件也能做**。
> 本课你将用**四种写法**实现**同一个逻辑**，亲手对比"软件 if-else"与"硬件状态机/组合逻辑"的差异与成本。

## 0. 这节课在讲什么

给定一个交换机端口，它对进出的网络"包"做三件事之一：

- **插入 Tag**（给包打上 VLAN 标签）
- **剥离 Tag**（去掉标签）
- **过滤**（标签不在允许名单 → 丢弃）

到底插还是剥还是丢，取决于端口的**模式**与**配置**。这套逻辑，用软件几行 `if-else` 能写，用硬件几个门电路也能连出来——这正是本课要你体会的：**软件是硬件的配置文件，软硬件逻辑一致**。

> 我们把真实以太网帧**抽象简化**成一个 32-bit "包字"，聚焦 Tag 的插/剥/滤核心逻辑，不掺路由、查表、MAC 学习。完整以太网帧处理留作引申思考。

## 1. 包字（packet word）格式

```
 bit  31     30      29     28        21..16        15..0
     ┌─────┬────────┬──────┬──────┬─────────────┬───────────────┐
     │VALID│ HAS_TAG│ DROP │ DIR  │   VID (6b)  │   PAYLOAD(16b) │
     └─────┴────────┴──────┴──────┴─────────────┴───────────────┘
```

| 字段 | 含义 |
| :-- | :-- |
| `VALID`   | 恒为 1，表示这是一个有效包 |
| `HAS_TAG` | 是否带 VLAN Tag |
| `DROP`    | **仅输出**：1 = 被过滤丢弃 |
| `DIR`     | **仅输入**：0 = 收包(ingress)，1 = 发包(egress) |
| `VID`     | 6-bit VLAN ID（0..63，对应 64-bit 允许位图） |
| `PAYLOAD` | 16-bit 不透明载荷（原样保留） |

## 2. 配置（MMIO 寄存器，对应硬件端口配置）

| 名称 | 含义 |
| :-- | :-- |
| `mode`  | 端口模式：0=Access，1=Trunk，2=Hybrid |
| `pvid`  | Access 收包时插入的默认 VID（6-bit） |
| `allow` | 64-bit 位图，`allow[vid]=1` 表示该 VID 允许通过（Trunk/Hybrid） |
| `untag` | 64-bit 位图，`untag[vid]=1` 表示 Hybrid 发包时剥离该 VID 的 Tag |

## 3. 你要实现的逻辑 `process(mode, pvid, allow, untag, in) → out`

| 模式 | 收包 ingress (DIR=0) | 发包 egress (DIR=1) |
| :-- | :-- | :-- |
| **Access** | 有 Tag → **剥离**；无 Tag → **插入 PVID** | **剥离**所有 Tag |
| **Trunk**  | 无 Tag → **丢弃**；VID 不在 `allow` → **丢弃**；否则**原样通过** | **保留**（原样发送） |
| **Hybrid** | 同 Trunk 收包 | `untag[vid]=1` → **剥离**；否则**保留** |

四个基本操作（建议先写成辅助函数/子表达式）：

```
strip(in)        = VALID | (in & 0xFFFF)                                  // 去 tag/vid，留 payload
insert(in, pvid) = VALID | HAS_TAG | ((pvid & 0x3F) << 16) | (in & 0xFFFF) // 打 tag，vid=pvid
keep(in)         = VALID | (in & (HAS_TAG | (0x3F<<16) | 0xFFFF))         // 保留 tag/vid/payload
drop()           = VALID | DROP                                           // 丢弃
```

## 4. 四条实现路径（任一过即过；多过加辅助分）

| 路径 | 目录 | 你要填的 TODO |
| :-- | :-- | :-- |
| 软件 · Rust | `sw/rust/src/main.rs` | `process()` 函数体 |
| 软件 · C    | `sw/c/test_vlan.c`    | `process()` 函数体 |
| 硬件 · Verilog | `hw/v/vlan_proc.v`  | 组合逻辑模块 `vlan_proc` |
| 硬件 · BlueSpec | `hw/bsv/VlanProc.bsv` | 函数 `process` |

每条路径的**测试向量与 PASS 打印是给好的**（在各自的 harness/testbench 里），你只需填 `// TODO` 处的核心逻辑。四条路径的输出必须**逐位一致**。

通过判据：输出依次出现 `ACCESS_PASS`、`TRUNK_PASS`、`HYBRID_PASS`、`ALL_PASS`，且不出现 `FAIL`。

```
labctl run improper/01-hw-vlan     # 跑所有可用变体
labctl hint improper/01-hw-vlan    # 卡住了看提示
labctl watch                        # 边改边自动重跑 + TUI 拓扑/波形
```

## 5. 看硬件（波形 / 结构 / 接口）

```
make -C hw/v sim     # 跑仿真（与判题同口径，0 warning）
make -C hw/v wave    # gtkwave 看波形
make -C hw/v synth   # yosys 看综合后的硬件结构
```

或用 `labctl` 终端伴侣面板（免 X11）：拓扑/数据流/波形/接口一屏看全；`labctl wave --gui` 一键升级到 gtkwave。

## 6. 思考题（`THINKING.md` 里作答，写下你的理解即可）

1. 同一个 Tag 逻辑，软件实现与硬件实现各自的**成本**在哪？给一个"软件成本更低"的场景与一个"硬件成本更低"的场景。
2. 为什么"一个大型 Web 项目完全可以解耦成数字逻辑、流片成 ASIC，插电即提供 CRUD"——但现实中我们几乎不这么做？（联系 QEMU/模拟器、OpenCL、通用 vs 专用）
3. 我们把以太网帧简化成了 32-bit 包字。若要支持真实的 12-bit VID + 变长帧 + FIFO 流式处理，硬件与软件分别要多付出什么？

## 7. 完成标准 (DoD)

- [ ] 至少一条路径 `*_PASS` + `ALL_PASS`（必修）。
- [ ] （选做/辅助分）软/硬其余路径也通过，输出逐位一致。
- [ ] 硬件路径 0 warning。
- [ ] 能说出"同一逻辑、软硬两种实现"的差异与成本（思考题）。
