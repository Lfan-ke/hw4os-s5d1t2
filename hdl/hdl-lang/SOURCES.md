# hdl/hdl-lang — 资产来源与构建说明

HDL 工具链语言级地毯式测试，统一为 6-leg app（SystemC 离散事件内核 case 见同级 `../bluesv/systemc`）。两类 leg：on-target（StarryOS 上运行的静态产物，与宿主黄金逐字节比对）+ host-exclusive（综合/STA/CLI 宿主黄金）。

## on-target：6 条 leg（StarryOS 上运行，与宿主黄金逐字节 cmp）

| leg | 工具(宿主) | on-target 产物 | 黄金 |
|--|--|--|--|
| Verilator | verilator 5.008 | verilate→C++→`*-linux-musl-g++` 交叉编译静态 sim | `golden/verilator.golden.txt` |
| Icarus Verilog | iverilog v12.0 (`-g2012`) | `tb_ivl.vvp` 由 `vendor/vvp/vvp-<arch>` 执行 | `golden/iverilog.golden.txt` |
| GNU Make | GNU Make 4.4.1 | `vendor/make/make-<arch>` 执行 `src/make/Makefile` | `golden/make.golden.txt` |
| Bluespec SV | bsc | `.bsv`→Verilog→`.vvp` | `golden/bsv.golden.txt` |
| Bluespec Haskell | bsc | `.bs`(BH 前端)→Verilog→`.vvp` | `golden/bh.golden.txt` |
| yosys 综合 | yosys 0.58 | 通用综合流→门级网表→后综合网表 sim | `golden/yosys.golden.txt` |

- 源：`src/`（`rtl/` RTL + `tb/` testbench + `LangBSV.bsv`/`LangBH.bs` + `make/` + `yosys/` 综合脚本 + `run-hdl.sh` 聚合 runner）。
- 静态运行时：`vendor/vvp/vvp-{x86_64,aarch64,riscv64,loongarch64}`（Icarus 运行时，musl 静态）、`vendor/make/make-<arch>`（GNU Make，musl 静态）——综合/编译工具在宿主执行，其产物在 StarryOS 上执行。
- 门控：`src/run-hdl.sh` 逐 leg cmp 黄金，`HDL_RESULT pass=6 total=6` 时输出 `TEST PASSED`（staged 脚本，避免 `shell_init_cmd` 回显自匹配 `success_regex`）。
- prebuild：`prebuild.sh` 在宿主 verilate/iverilog/bsc/yosys 各 leg、捕获黄金、经 overlay 注入 rootfs。`build-*.toml`/`qemu-*.toml` ×4（aarch64 用 `-cpu cortex-a72`）。

## host-exclusive：`host-carpets/`（宿主黄金）

EDA 工具的 `--help`/子命令地毯 + ASIC 综合/STA/功耗流程，以宿主官方工具为权威黄金（无可在 StarryOS 上执行的运行时产物）：

- `yosys-cli-carpet.sh` —— `yosys --help` + `yosys -p 'help'`(135 命令)逐命令 help。
- `yosys-sta-carpet.sh` —— 逐字复刻 OSCPU/yosys-sta `scripts/yosys.tcl`：`synth -flatten`→`dfflibmap`→`abc -D -constr`→`write_verilog` + `synth_stat`(面积)/`synth_check`(DRC) 的 ASIC 综合/STA/功耗(PPA)流程。
- `verilator-cli-carpet.sh` / `iverilog-cli-carpet.sh` / `bsc-cli-carpet.sh` —— 各工具逐 `--help`/选项地毯。
- 可移植性：假定 `yosys`/`verilator`/`iverilog`/`bsc` 在 PATH（本机官方安装）。

## 四架构（qemu-10 单核 StarryOS）

aarch64 / riscv64 / loongarch64 各 `HDL_RESULT pass=6 total=6` + `TEST PASSED` + `SUCCESS PATTERN MATCHED`；x86_64 经上游 CI。

## 权威依据

Verilog/SV = IEEE 1800；Verilator = github.com/verilator/verilator；Icarus = github.com/steveicarus/iverilog；GNU Make 手册；Bluespec = github.com/B-Lang-org/bsc；yosys = github.com/YosysHQ/yosys；yosys-sta = github.com/OSCPU/yosys-sta。

## 复现

```sh
cargo xtask starry app qemu -t hdl-lang --arch x86_64    # aarch64 / riscv64 / loongarch64
```
（把本目录作为 `apps/starry/hdl-lang` 放入 StarryOS 工作区。对应上游 PR #1285。）
