# AI4OSE OSLAB

一套 RISC-V 操作系统实验课程：从「手搓一个块设备数组」到「在 QEMU 上跑起 20 阶段的真内核」。
**rustlings 式**——`labctl` 给你下一题、判你的答案、漏出提示，全过了再进下一题。

## 快速开始

```sh
# 1. 装工具链（见文末）。labctl 已预编译，也可自行 cargo build -p labctl
alias labctl=./labctl/target/debug/labctl

labctl next                 # 它告诉你下一题（从 improper/01 开始）
#   打开 exercises/<id>/，读 README，按 // TODO 填空
labctl run  <id>            # 判你的答案（跑所有变体）
labctl hint <id>            # 卡住了要提示（渐进）
labctl watch <id>           # 边写边判：存盘即自动重跑 + 刷新面板
#   全过 → labctl next 进下一题
labctl verify               # 全量记分板
```

## 三条赛道（学习顺序：不正经 → 正经 → 形态）

| 赛道 | 性质 | 学什么 | 数量 |
|---|---|---|---|
| **不正经 improper** | 心智模型（直觉） | 用最朴素的软件/硬件模型，把每个 OS 子系统的「本质」直接演给你看（块设备=数组，MMU=软件页表，特权级=几根线） | 18 |
| **正经 proper** | 工程落地 | 在 QEMU 上真刀真枪写 S 态内核，**rcore ch1-8 的节奏**一路到多核/SMP/虚拟化/微内核 | 20 |
| **形态 forms** | 入门科普 | 五大内核形态（宏/微/外/库/框）+ 混合的架构权衡，引 xv6 / seL4 / jos / unikraft / asterinas | 6 |

> 不正经传授**心智模型**，正经是**正儿八经的工程落地**，形态是**架构入门科普**。建议按上表顺序学：
> 先用直觉建立子系统的心智模型 → 再亲手把它们工程落地成真内核 → 最后回看「内核还能长成什么样」。

## 设计哲学：最小 + 留白 + 可扩展（教学为主）

- **最小依赖**：每题只实现「当下用得到」的——S8 只写 `sys_write`/`sys_exit`（**不是 360+ 个 syscall**）、
  S9 是迷你 libc（不是完整 musl）、S11 net 只做 ARP/IP/UDP（不是全栈）、S12 GUI 是软件 framebuffer（不接真 virtio-GPU）。
  像 rcore ch1-8，**逐阶段只补必需**，绝不一上来就全量。
- **复杂处给定**：trap 入口汇编、`__switch` 上下文切换、`minlibc` 本体、设备树解析等「过于复杂 / 与本题无关」的部件**已提供**，你只填核心 TODO。
- **留白 TODO**：每题把核心逻辑挖成 `// TODO` 或 `// TODO-or-else`（择一分支），由你补全。
- **可扩展**：每个 README 的「引申」节指出怎么把最小版长成完整版（真 SV39 分页 / 真 newlib·picolibc / fork-exec / 真 virtio-GPU…）。
  完成实验后，**你可以凭兴趣自行把它扩展成完整的 OS**——保证可扩展性。

## 实验怎么组织

```
exercises/<track>/<id>/      ← 题面：README + 含 // TODO 的源码（你写这里）
ans/<track>/<id>/            ← 配套答案：参考实现（labctl 验证用 + 你卡住时参考）
```

- 每题多变体：软件 **rust + c** 双版；有硬件的再加 **verilog + bsv** 双版；**essay** 思考题（写下你的理解、删掉未作答哨兵即判过）。
- `require=1`：**任一变体通过即算必修达成**；但鼓励多语言都做，体会同一思想的不同落地。
- 判题靠输出里的 `*_PASS` 串；占位代码能编译、但跑不出 `ALL_PASS`。

## labctl 命令

| 命令 | 作用 |
|---|---|
| `list` | 列出全部实验与通过状态 |
| `next` | 跳到下一个未通过的实验 |
| `run <id> [--solutions]` | 跑/判一题的所有变体（`--solutions` 用 `ans/` 自测） |
| `hint <id>` | 渐进提示 |
| `watch <id>` | 存盘即自动重跑并刷新伴侣面板 |
| `view <id>` | TUI 伴侣面板（拓扑 / 数据流 / 接口，静态） |
| `wave <id> [--gui]` | 看硬件波形（终端紧凑渲染或 gtkwave） |
| `diagram <id>` | 导出 Mermaid 拓扑/数据流图 |
| `verify [--solutions]` | 全量跑出记分板 |
| `score` | 必修分 + 辅助分（两本独立的账） |

## 工具链

| 用途 | 需要 |
|---|---|
| 软件（不正经/形态） | Rust（cargo）、`gcc` |
| 正经赛道内核 | `rustup target add riscv64gc-unknown-none-elf`、`riscv64-unknown-elf-gcc`、`qemu-system-riscv64`、`make` |
| 硬件变体 | `iverilog` + `vvp`（Verilog）、`bsc`（BSV）；`verilator`（选做 lint） |
| 设备树 | `dtc` |

labctl 会自动探测工具链；缺哪个，对应变体显示 **⊘ 不可用**（不报错、不影响其它变体）。

## 进一步

- `docs/DESIGN-E.md` —— 全部实验的细化设计 + 正经赛道路线图
- `DESIGN.md` —— 总体设计与理念
- 教师/进阶参考素材：覆盖 RISC-V 全栈的真实系统源码与演化笔记（内核/libc/fs/hal/net/gui/虚拟化…），各实验均可对照真实工业/学术实现。
