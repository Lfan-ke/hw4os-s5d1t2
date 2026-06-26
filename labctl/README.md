# labctl

AI4OSE OSLAB 的实验引导 / 编译 / 判题 / 计分 runner（对标 rustlings 体验）。

## 构建

```sh
cargo build --release --manifest-path labctl/Cargo.toml
# 可把 target/release/labctl 放进 PATH
```

前置工具（按需，缺哪个对应变体自动降级为「⊘ 不计分」）：
`rustc`(+`riscv64gc` target)、`gcc`、`qemu`、`iverilog`/`vvp`、`verilator`、`yosys`、`bsc`、`gtkwave`。

## 命令

| 命令 | 作用 |
| :-- | :-- |
| `labctl list` | 列出全部实验与状态 |
| `labctl run [id]` | 跑某实验所有可用变体并判定 |
| `labctl watch [id]` | 监视源文件，保存即自动重跑 + 刷新 TUI 伴侣面板 |
| `labctl hint [id]` | 看渐进提示 |
| `labctl verify [--solutions]` | 全量记分板（`--solutions` 用参考解自测，应全过） |
| `labctl score [--solutions]` | 分别打印必修分与辅助分 |
| `labctl view [id]` | 静态 TUI 面板（拓扑/数据流/接口） |
| `labctl wave [id] [--gui]` | 终端波形；`--gui` 调 gtkwave |
| `labctl diagram [id]` | 导出 Mermaid 拓扑/数据流图 |
| `labctl next` | 下一个实验 |

`id` 形如 `improper/01-hw-vlan`，省略则取第一个。在仓库内任意目录运行即可（向上找 `info.toml`），或用 `--root` 指定。

## 判题与计分

- **任一过即过**（`require=1`）；推广为 **M 选 N**（`require=N` → 通过变体数 ≥ N 算必修达成）。
- 超出 `require` 的每条通过路径计 **辅助分**，记在与必修分独立的账上。
- 硬件变体 **0-warning** 才算过；软件 C/Rust、硬件 V/BSV **任一过即算过**。
- 判题看运行输出：`expect` 子串全中且 `forbid` 子串不中即通过。

## 实验目录结构

```
exercises/<track>/<id>/
├── meta.toml     # 变体声明 + require + judge + 渐进提示
├── view.toml     # 拓扑/数据流/接口/波形信号（驱动 TUI 与 diagram）
├── README.md     # 指南 + DoD + 思考题
├── sw/{rust,c}/  hw/{v,bsv}/   # 各变体（学生填 // TODO）
solutions/<track>/<id>/...      # 参考解（同构，judge 自测用）
```

判题由 labctl **直接调** cargo/gcc/iverilog/bsc，不经 Makefile；硬件目录的薄 `Makefile`（`make sim/wave/synth/lint`）**仅供人**看波形/结构。
