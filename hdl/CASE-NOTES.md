# HDL 测例 — 流水线与测试结果

对应 tgoskits 测例:`test-suit/starryos/stress/{verilog-0, iverilog-0, gnumake-0}`。

## 流水线(三段,每段精确匹配黄金)
1. **host**:官方工具链跑出确定性黄金输出。
   - verilator:`verilator --binary --timing tb.v dut.v` 与 iverilog 输出比对一致 → 黄金。
   - iverilog:`iverilog -g2012 -o dut.vvp tb.v dut.v && vvp dut.vvp`。
   - gnumake:host `make -s` 跑 `Makefile`。
2. **qemu-10 用户态 Linux ×4 arch**:`qemu-<arch> <静态二进制>` 跑出与黄金一致(交叉编译正确性参照)。**4/4**。
3. **qemu-10 StarryOS ×4 arch 单核**:`cargo xtask starry test qemu --arch <a> -g stress -c <case>`,
   binary 在 starry 上跑完 + stdout 与 rootfs 内黄金 `cmp` 一致 → 门 `*_OK=1`。

## 测试结果(2026-05-30,StarryOS @ qemu-10,单核)

| 工具 | 测例 | x86_64 | aarch64 | riscv64 | loongarch64 |
|---|---|:--:|:--:|:--:|:--:|
| **verilator** 5.008 | `verilog-0` | √ | √ | √ | √ |
| **iverilog** v12_0(静态 vvp) | `iverilog-0` | √ | √ | √ | √ |
| **GNU Make** 4.4.1 | `gnumake-0` | √ | √ | √ | √ |

全部 host √ + qemu-Linux 4/4 √ + starry 4/4 √ = **12/12**。每个 case 的 `*_OK=1` 门:
verilator→`VERILOG_OK=1`,iverilog→`IVERILOG_OK=1`,gnumake→`GNUMAKE_OK=1`。

## 黄金输出
- verilator/iverilog 共享(`shared` 设计 dut.v+tb.v):8×ALU + COUNT + 5×FSM + `VERILOG_SIM_OK`(15 行)。
  verilator 综合版(top.sv+sim_main.cpp)另有更全的 ALU sweep / RF_SUM / FSM trace。
- gnumake:`make: building {alpha,beta,gamma}` + `MAKE_NAMES=3` + `MAKE_SORTED=...` + `GNUMAKE_BUILD_OK`。

## 复现
每个目录:`build-<target>.toml`×4 + `qemu-<arch>.toml`×4 + `prep-*-rootfs.sh`(debugfs 注入二进制+黄金到 alpine rootfs)+ 源码 + `testbin/<arch 静态二进制>`。
把 `qemu-*.toml`/`build-*.toml` 放回 tgoskits `test-suit/starryos/stress/<case>/`,`prep` 造 rootfs,即可在 qemu-10 四架构 starry 复跑。**aarch64 toml 必须 `-cpu cortex-a72`**;loongarch 必须 QEMU≥10。
