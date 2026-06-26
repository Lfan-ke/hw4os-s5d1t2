# hw4os 实验 harness 设计（`labctl`）

> 状态：设计已与课程设计者逐条对齐，待其评审 spec。
> 适用范围：本 spec 只定义**共享基建（Phase 0）**——所有实验共用的运行/判题/计分/可视化/引导骨架。
> 各具体实验的内容设计是后续 Phase 1（`docs/DESIGN-E.md`）的产物，不在本 spec 内。

## 1. 目标与定位

对标 rustlings 的逐题递进体验，做一套 OS 实验引导框架，核心理念：**让学生亲手对比"同一逻辑用软件 if-else 与用硬件状态机实现"的差异，建立软硬件协同的心智模型**。

- 两条赛道：`improper`（不正经，建立感性心智模型）与 `proper`（正经，rcore 基础上逐步搭真内核）。
- 每个实验可有**多个变体**，横跨两轴：软件 `{c, rust}` × 硬件 `{verilog, bsv}`。并非每个实验都四变体齐全——由各实验 `meta.toml` 声明。
- 判题"**M 选 N**"：通过路径数 ≥ `require`（默认 1 = 任一过即过）即视为完成；超出 `require` 的每条路径计**辅助分**，记在与总分独立的另一本账上。
- 纯 Rust runner（`labctl`）统管：编译、运行、判题、计分、进度、watch、可视化。
- ISA 基准：**RV64GC**。
- 硬件：iverilog/yosys/verilator/(vivado) 可识别、**0 warning**；可视化以**终端 TUI 优先**，保留 GUI/图表逃生舱。

### 非目标（YAGNI）

- 不做完整路由/MAC 学习/转发等网络功能（VLAN 实验只聚焦 Tag 插入/剥离/过滤）。
- 不做在线判题服务、账号系统、Web 后端。
- 不强求每个实验四变体齐全；缺的变体不存在即可。
- Phase 0 不实现任何具体实验内容（除唯一的"试金石"实验 `01-hw-vlan`，用于验证骨架）。

## 2. 名词与模型

| 名词 | 含义 |
| :-- | :-- |
| **track** | 赛道：`improper` / `proper` |
| **exercise** | 一个实验（一道题），位于 `exercises/<track>/<id>/` |
| **variant** | 一个实验的一条实现路径，按 `(axis, lang)` 标识：`sw-rust` / `sw-c` / `hw-v` / `hw-bsv` |
| **axis** | 变体所属轴：`software` / `hardware` |
| **require (N)** | 该实验"必修达成"所需的最少通过变体数；默认 1 |
| **必修分** | 实验完成（通过变体数 ≥ require）所得，计入总分 |
| **辅助分** | 超出 require 的每条通过路径所得，**独立账本**，不影响"是否完成" |

变体判定结果四态：`Pass | Fail | Unavailable | Error`。`Unavailable` = 缺工具链（既不计失败也不进辅助分分母）。

## 3. 仓库布局

```
hw4os/                          # 仓库根（= 当前 repo）
├── Cargo.toml                  # workspace: labctl + 共享软件 crate
├── rust-toolchain.toml         # riscv64gc-unknown-none-elf + stable + 组件
├── info.toml                   # 有序总清单（顺序 + track 分组）
├── labctl/                     # 纯 Rust runner
│   ├── Cargo.toml
│   └── src/
│       ├── main.rs             # CLI 入口
│       ├── manifest.rs         # 解析 info.toml / meta.toml / view.toml
│       ├── toolchain.rs        # 探测工具链 + 降级
│       ├── judge.rs            # expect/forbid/warn_gate 判定
│       ├── score.rs            # M选N + 辅助分独立账 + 进度状态
│       ├── variant/            # 各变体 runner（直调工具，不依赖 Makefile）
│       │   ├── mod.rs
│       │   ├── sw_rust.rs      # cargo/rustc → host 或 qemu
│       │   ├── sw_c.rs         # riscv64 gcc → qemu（或 host gcc）
│       │   ├── hw_verilog.rs   # iverilog/verilator 仿真 + 0-warning 门
│       │   └── hw_bsv.rs       # bsc 仿真
│       ├── tui/                # 中心编辑伴侣 TUI
│       │   ├── mod.rs          # 布局/事件循环/watch 刷新
│       │   ├── topology.rs     # 拓扑 + 数据流（view.toml 驱动）
│       │   ├── wave.rs         # 终端波形（解析 .vcd）
│       │   └── iface.rs        # 接口/MMIO 面板
│       └── diagram.rs          # 逃生舱：导出 Mermaid / WaveDrom-SVG
├── common/                     # 所有实验共享，学生不动
│   ├── mk/
│   │   ├── verilog.mk          # sim/wave/synth/lint（iverilog/verilator/yosys/gtkwave）
│   │   └── bsv.mk              # sim/wave（bsc）
│   ├── hw/                     # 共享 testbench（tb_top）、帧驱动、MMIO BFM
│   ├── sw-rust/                # 共享 no-std/std 测试 crate（assert→PASS/FAIL、MMIO、qemu shim）
│   └── sw-c/                   # 共享 C harness（mmio.h、test runner、panic→exit）
├── exercises/
│   ├── improper/
│   │   └── 01-hw-vlan/
│   │       ├── meta.toml       # 变体声明 + require + judge + env
│   │       ├── view.toml       # 拓扑/数据流/波形可视化声明
│   │       ├── README.md       # 指南 + DoD + 思考题
│   │       ├── hw/v/  hw/bsv/  # 含 // TODO 的 DUT（tb 来自 common/hw）
│   │       └── sw/c/  sw/rust/ # 含 // TODO 的驱动/测试
│   └── proper/                 # 正经赛道（后续）
├── solutions/                  # 参考解（判题自测 / CI / labctl verify --solutions）
│   └── improper/01-hw-vlan/...
└── docs/
    ├── DESIGN-E.md             # Phase 1 产物：全部实验的细化设计
    └── superpowers/specs/2026-06-27-hw4os-harness-design.md   # 本文件
```

> 注：`solutions/` 与 `exercises/` 同构；学生区只含挖空版，参考解独立放置，避免泄题，同时供 CI 自测"参考解必过"。

## 4. 清单与元数据 schema

### 4.1 `info.toml`（总清单，精简）

```toml
[course]
name = "AI4OSE OSLAB"
isa  = "rv64gc"

# 有序推进清单；细节在各 exercise 的 meta.toml
order = [
  "improper/01-hw-vlan",
  # improper/02-... （Phase 1 后补全）
]
```

### 4.2 `meta.toml`（每实验）

```toml
id      = "01-hw-vlan"
title   = "硬件管理 · VLAN Tag 的插入/剥离/过滤"
track   = "improper"
require = 1                 # 必修阈值：通过变体数 ≥ require 即完成（默认 1）
env     = "qemu-virt"       # 软件变体运行环境：host | qemu-user | qemu-virt
weight  = 1                 # 必修分权重（旋钮）

[[variant]]
id        = "sw-rust"
axis      = "software"
lang      = "rust"
dir       = "sw/rust"
build     = "cargo"          # labctl 内置构建关键字

[[variant]]
id        = "sw-c"
axis      = "software"
lang      = "c"
dir       = "sw/c"
build     = "gcc-rv64"

[[variant]]
id        = "hw-v"
axis      = "hardware"
lang      = "verilog"
dir       = "hw/v"
build     = "iverilog"
warn_gate = true             # 0 warning 才算过

[[variant]]
id        = "hw-bsv"
axis      = "hardware"
lang      = "bsv"
dir       = "hw/bsv"
build     = "bsc"
warn_gate = true

[judge]                       # 所有变体统一对外行为
expect    = ["ACCESS_PASS", "TRUNK_PASS", "HYBRID_PASS"]  # 子串/正则，全中才过
forbid    = ["FAIL", "panic", "ERROR"]                    # 命中任一即判失败
timeout_s = 30

[[hint]]                      # 渐进提示（labctl hint 逐条揭示）
text = "Access 模式：无 Tag 插 PVID；有 Tag 剥离。先把这条路径跑通。"
[[hint]]
text = "EtherType 0x8100 标记 VLAN，VID 在低 12 位（rx_data[15:4]）。"
[[hint]]
text = "硬件路径填 vlan_port 的三个 always 块；软件路径填 port_send/recv。"
```

### 4.3 `view.toml`（每实验可视化声明，驱动 TUI 与 diagram 导出）

```toml
# 拓扑节点
[[node]]
id = "rx"
label = "rx"
[[node]]
id = "parse"
label = "parse 8100?"
[[node]]
id = "access"
label = "access(mode)"
[[node]]
id = "insert"
label = "insert 8100/PVID"
[[node]]
id = "fifo"
label = "rx_fifo"

# 拓扑连线
[[edge]]
from = "rx"
to   = "parse"
[[edge]]
from = "parse"
to   = "access"
[[edge]]
from = "access"
to   = "insert"
[[edge]]
from = "insert"
to   = "fifo"

# 一条数据流场景（TUI 动画 / diagram 高亮）
[[flow]]
name = "access-untagged"
path = ["rx", "parse", "access", "insert", "fifo"]
note = "无 Tag 帧 → 插入 PVID=5"

# 波形面板关注的信号
[wave]
signals = ["clk", "rst_n", "rx_valid", "rx_last", "tx_valid", "tx_last"]
```

> `view.toml` 由设计者编写，保证**教学准确**；`labctl diagram --check` 可把它与 `yosys` 导出的 netlist JSON 对照，校验真实结构是否一致。

## 5. `labctl` 命令

| 命令 | 行为 |
| :-- | :-- |
| `labctl list` | 列出全部实验与状态：必修 ✓/✗、辅助分计数、各变体 Pass/Fail/Unavailable |
| `labctl run [id]` | 跑当前/指定实验的**所有可用变体**，逐个判定，刷新进度与计分 |
| `labctl watch` | rustlings 式：监视文件，存盘自动重跑当前实验；必修达成自动跳下一题；`h` 看提示，`n` 跳过 |
| `labctl hint [id]` | 揭示下一条渐进提示 |
| `labctl next` | 跳到下一个未完成实验 |
| `labctl verify [--solutions]` | 全量跑出记分板（必修总分 + 辅助分总分）；`--solutions` 用参考解自测（应全过） |
| `labctl score` | **分别**打印必修总分与辅助分总分（两本独立的账） |
| `labctl wave [id] [--gui]` | 终端波形；`--gui` 调 gtkwave 精确调试 |
| `labctl diagram [id] [--check]` | 导出 Mermaid/SVG（拓扑/数据流/接口）；`--check` 对照 yosys netlist |

## 6. 判题与计分

### 6.1 工具链探测与降级（`toolchain.rs`）

启动时探测：`cargo/rustc`(+riscv target)、`riscv64-*-gcc`、`qemu-riscv64`/`qemu-system-riscv64`、`iverilog`/`verilator`、`yosys`、`gtkwave`、`bsc`。某变体所需工具缺失 → 该变体记 `Unavailable`（TUI 灰显），**既不算失败也不进辅助分分母**——学生不因没装 bsc/verilator 被惩罚。

### 6.2 单变体判定（`variant/*` + `judge.rs`）

1. 构建（直调对应工具，不经 Makefile）。硬件变体若 `warn_gate=true`，编译输出含 warning ⇒ `Fail`。
2. 运行（软件按 `env` 选 host / qemu-user / qemu-virt；硬件跑 `common/hw` 的 tb）。
3. 判定：捕获 stdout/stderr，`expect` 全部命中且 `forbid` 一个都不命中 ⇒ `Pass`，否则 `Fail`；超 `timeout_s` ⇒ `Error`。

### 6.3 M 选 N + 辅助分（`score.rs`）

```text
for each exercise E:
    results = { v -> run_or_unavailable(v) for v in E.variants }
    passed  = [ v for v,r in results if r == Pass ]
    E.required_done = len(passed) >= E.require          # 必修达成
    E.bonus         = max(0, len(passed) - E.require)   # 每多一条 +1（旋钮）

required_total = Σ E.weight  for E where E.required_done   # 计入总分
bonus_total    = Σ E.bonus   for all E                     # 独立账本
```

`required_total` 与 `bonus_total` 互不干扰，`labctl score` 分开呈现。

## 7. 软件变体约定

- 共享 harness（`common/sw-rust`、`common/sw-c`）提供：MMIO 读写、`check!/assert_*` 宏（成功打印 `*_PASS`、失败打印 `*_FAIL` 供 runner 解析）、`_start` 与 panic→exit shim。
- 纯软件实验（如进程调度）可 `env="host"` 直接在主机跑（std），免 qemu，最快出成果。
- 需要设备/裸机语义的实验用 `qemu-virt`（带 MMIO 设备模型）或 `qemu-user`。
- C 与 Rust 任一过即满足软件路径；两者都过为辅助分（受 `require` 与轴权重约束）。

## 8. 硬件变体约定

- **判题**：`labctl` 直调 `iverilog -g2012 -Wall`（或 `verilator -Wall -Werror`）编译 + 跑 `common/hw` 的 `tb_top`，0 warning 且 tb 打印 `*_PASS` 才算过。判题**不依赖 Makefile**。
- **看波形/结构（仅供人用）**：每个硬件变体目录放一个**薄 Makefile**，`include` `common/mk/verilog.mk`（或 `bsv.mk`）：
  - `make sim` —— 同判题口径跑一遍仿真
  - `make wave` —— 产 `.vcd` 并用 gtkwave 查看
  - `make synth` —— `yosys` 综合 + `show`/stat 查看硬件结构
  - `make lint` —— verilator/yosys 静态检查，强制 0 warning
- 共享 `tb_top` + 帧/MMIO BFM 在 `common/hw`，学生只填 DUT 里的 `// TODO`/`// ELSE`。

## 9. 可视化（TUI 优先 + 逃生舱）

**默认**：`labctl` 终端 TUI 作为"中心编辑"的伴侣面板（学生在编辑器改代码，TUI 在侧）：

```
┌ labctl ▸ improper/01-hw-vlan ───────────── ACCESS · PVID=5 ┐
│ 拓扑 / 数据流   (帧: 无Tag → 插PVID)                        │
│   rx ▶[parse 8100?N]▶[access mode=0]▶[insert 8100/VID5]▶fifo│
│ 波形 (最近一次 sim)                                         │
│   clk   ▁▔▁▔▁▔▁▔     rx_valid ▁▔▔▔▔▁▁                       │
│   tx_valid ▁▁▁▔▔▔▔▔▁                                        │
│ 接口 MMIO  0x00 PORT_CTRL  0x20 TX_DATA  0x30 RX_DATA ...   │
│ 变体  ✓ sw-rust   · hw-v FAIL   ⊘ hw-bsv(无 bsc)            │
└ [h]提示 [r]跑 [w]波形放大 [d]导出图 ───────────────────────┘
```

- 拓扑/数据流由 `view.toml` 驱动，可随 `watch` 在编辑后刷新、按 `flow` 播放数据流动画。
- 波形由 runner 解析 `.vcd` 在终端渲染（Unicode），免 X11。
- **逃生舱**（要精确/好看时一键升级）：`labctl wave --gui`→gtkwave；`labctl diagram`→Mermaid/WaveDrom-SVG（repo 已用 mermaid，可入 README/浏览器/VSCode 预览）；`make wave/synth` 仍保留给硬件老手。

## 10. 引导机制（rustlings 对齐）

学生源文件内标记语法：

```c
// TODO: <要填的逻辑说明>                 —— 必填空（单一解）
// HINT: <行内提示>                       —— 就近提示
// TODO[a]: <方案A>                        —┐ 变体内"分支择一"：同一条路径的
// ELSE[b]: <方案B>                        —┘ 多种可接受解法，学生择一实现
// I AM NOT DONE                          —— （可选）门控；删除以示完成，watch 不提前跳题
```

**两层"选择"，互不等同（消歧）：**
- **变体级 M 选 N**：由 `meta.toml` 的 `require=N` 控制——一题的 4 个变体里至少通过 N 个才算完成（§6.3）。这是**计分**口径。
- **变体内分支择一**（`// TODO[a] … // ELSE[b]`）：同一条变体路径里提供多种可接受写法，学生择一即可；判题**只看统一输出**（`expect/forbid`），不关心走了哪条分支——体现"多种可接受解法"的开放性，本身不单独计分。

其余：
- 渐进提示存于 `meta.toml` 的 `[[hint]]`，由 `labctl hint`/watch 的 `h` 逐条揭示。
- 参考解放 `solutions/`，供 `labctl verify --solutions` 与 CI 自测。

> 旋钮（§14）：是否对"分支择一"做更细的分支级 M 选 N 计分（实现多个分支加辅助分），待 Phase 1 逐题斟酌；Phase 0 先按"分支不单独计分"。

## 11. 试金石实验：`improper/01-hw-vlan`

为直面"基建太抽象、不可验证"的顾虑：Phase 0 在搭骨架的同时，把 `01-hw-vlan` 做成**参考解（`solutions/`）+ 学生填空版（`exercises/`）**，四变体（sw-rust/sw-c/hw-v/hw-bsv）齐全，端到端证明骨架可用：

- OR / M 选 N 计分、辅助分独立账；
- 0-warning 门、缺工具降级；
- TUI 拓扑 + 数据流 + 终端波形 + 接口面板；
- `wave/synth/diagram` 逃生舱。

实验内容沿用 DESIGN.md：三端口模式（Access/Trunk/Hybrid）的 Tag 插入/剥离/过滤，MMIO 寄存器映射与 DESIGN.md 一致。可进一步抽象为单/双字节处理以降低学生负担（完整 Tag 作为引申思考）。

## 12. 硬约束（不可违反）

- 提交：`git commit -s -S`（GPG 密码 `360123`），**绝不出现任何 agent 署名/名字/"Generated with"**。
- **先不 `push`**。
- ISA 基准 RV64GC。
- 硬件 0 warning。
- 软硬件"任一过即过"（推广为 M 选 N）；多过计独立辅助分。

## 13. 路线图

- **Phase 0（本 spec）**：harness 骨架 + `common/` 共享件 + `labctl`（核心 + TUI）+ 试金石 `01-hw-vlan` 端到端跑通。
- **Phase 1**：参考 rcore/xv6，梳理 DESIGN.md 全部实验的意境/拆解/顺序/变体/DoD/思考题，反填充 `docs/DESIGN-E.md`。
- **Phase 2**：按 `DESIGN-E.md` 实现全部"不正经"入门实验。
- **Phase 3**：正经赛道，rcore 基础上引导用户逐步完成"相对完整"的 OS。

## 14. 待定旋钮（soft，先给默认，后续可调）

- 辅助分权重：默认每多一条通过路径 +1；是否按轴加权（跨软/硬轴额外奖励）待定。
- 各实验 `env` 默认值、`require` 取值由 Phase 1 逐题确定。
- 终端波形渲染精度 vs. 直接转 gtkwave 的阈值。
- 是否启用 `// I AM NOT DONE` 门控（逐题或全局）。

## 15. 验收标准（harness DoD）

1. `cargo build` 整个 workspace 通过，`labctl --help` 可用。
2. `labctl list` 正确显示 `01-hw-vlan` 及其四变体状态。
3. `labctl verify --solutions` 下，参考解四变体全 `Pass`，`labctl score` 必修分满、辅助分正确（= 多过路径数）。
4. 故意改坏一个变体 → 该变体 `Fail`，但只要 ≥`require` 条仍过，实验仍判完成。
5. 卸载/隐藏 `bsc` → `hw-bsv` 记 `Unavailable`，不影响必修达成，不计入辅助分分母。
6. 硬件变体引入一个 warning → `warn_gate` 使其 `Fail`。
7. `labctl watch` 编辑学生文件后自动重跑并刷新 TUI 拓扑/波形。
8. `make -C exercises/improper/01-hw-vlan/hw/v wave` 能产出可查看的波形；`make synth` 能查看结构。
9. 任一提交均为 `-s -S` 且无 agent 署名。
