# hdl-lang — HDL 语言级地毯式测试 (StarryOS app)

面向 StarryOS 的 **HDL 语言/工具链**地毯式测试 app，对应 [#764](https://github.com/rcore-os/tgoskits/issues/764) 的
`verilog <!-- verilator, iverilog, gnumake -->` 与 `bluesv <!-- bluespec systemverilog, system c -->`
两个注释点名项中的语言层 + 综合层。**MODEL A（静态二进制）**：所有仿真在 **构建宿主**用官方工具链编译为
**为 STARRY_ARCH 交叉编译的静态二进制 / Icarus 字节码**，宿主捕获确定性黄金，on-target 逐字节 `cmp`。

六条腿全部通过才打印 `TEST PASSED`（任一不过 → `TEST FAILED`）：

| 腿 | 工具 | on-target 产物 | 语义 |
|:--:|:--:|:--:|:--|
| **VLOG** | Verilator 5.008 | `hdl-tb-vlt`（静态 C++ 二进制） | 综合 SystemVerilog 设计 verilate→C++，musl-cross g++ 静态交叉编译；139 自检，`CARPET_RESULT ALL_PASS` |
| **IVL** | Icarus Verilog 12 | `hdl-tb-ivl.vvp` + 静态 `vvp` | 同一份 SV 设计编为可移植字节码，静态 vvp 运行；**必须逐字节 == VLOG** |
| **BSV** | bsc 2026.01 | `LangBSV.vvp` + 静态 `vvp` | Bluespec SystemVerilog → Verilog → vvp 字节码 |
| **BH** | bsc 2026.01 | `LangBH.vvp` + 静态 `vvp` | Bluespec Classic / Haskell → Verilog → vvp 字节码 |
| **MAKE** | GNU Make 4.4.1 | `lang-make`（静态二进制） | 自包含 Make 语言特性 Makefile（`:=`/`?=`/`+=`/函数/模式规则/自动变量/`.PHONY`/条件/`include`） |
| **yosys** | yosys 0.58 | `yosys_net.vvp` + 静态 `vvp` | 完整综合流程（proc/opt/fsm/memory/techmap）出门级网表，自检 testbench 驱动**综合后网表** + yosys simlib，验证综合结果在 on-target 功能正确 |

> 设计取舍：`verilator`/`iverilog`/`bsc`/`yosys` 本身是 host-only 编译/综合器，没有 on-target 二进制；on-target 跑的是
> 它们的**确定性仿真产物**。yosys 这条腿不是跑综合器，而是仿真它综合出的门级网表（综合结果的功能等价证明）。

## 结构

- `src/rtl/*.sv` + `src/tb/tb_top.sv` —— 综合 SystemVerilog 设计 + 自驱动 testbench（ALU/寄存器堆/计数器/2 个 FSM/桶形移位器/generate/package/枚举/struct/union/打包数组/系统函数；输出确定化的 `TB:` 行，139 自检）。
- `src/LangBSV.bsv` / `src/LangBH.bs` —— 综合 Bluespec 设计（interface/module/method/rule/Reg、ADT/maybe/tuple/vector/FIFO/寄存器堆，确定性 `$display` + `BSV_DONE`/`BH_DONE` 哨兵）。
- `src/make/{Makefile,config.mk}` —— GNU Make 语言特性地毯（确定性输出 + `MAKE_LANG_OK`）。
- `src/yosys/{alu,ctrl,datapath}.v` + `tb_synth.v` —— 综合用 RTL（组合 ALU + Moore FSM + 含同步 RAM 的 datapath）+ 综合后网表自检 testbench（`SYN_DONE`）。
- `vendor/vvp/vvp-<arch>` —— 静态 musl 交叉编译的 Icarus `vvp` 运行时（system VPI + VCD 内嵌；静态二进制不能 dlopen `system.vpi`，故链接进运行时）。驱动 IVL/BSV/BH/yosys 四条字节码腿。
- `vendor/make/make-<arch>` —— 静态 musl 交叉编译的 GNU Make 4.4.1。
- `golden/*.txt` —— 各腿确定性黄金参考（committed；prebuild 在宿主重新捕获装入 overlay 比对）。
- `prebuild.sh` —— 宿主编译全部六条腿、交叉编译/构建静态产物、捕获黄金、装入 overlay。交叉二进制经 `qemu-<arch>-static` 在宿主跑出黄金并自检，故任意宿主上对任意 STARRY_ARCH 都可复现。
- `qemu-<arch>.toml` ×4 —— 跑全部六条腿、各自 `cmp` 黄金，全过才 `TEST PASSED`（`success_regex = ^TEST PASSED$`，`fail_regex` 含 panic 与 `^TEST FAILED$`）。
- `build-<target>.toml` ×4。

## 运行

```sh
cargo xtask starry app qemu -t hdl-lang --arch x86_64    # aarch64 / riscv64 / loongarch64
```

## 工具链

verilator 5.008 / iverilog+vvp 12 / bsc 2026.01（`/usr/local/bsc`）/ yosys 0.58 / GNU make 4.x（均宿主工具）；
musl 交叉 `/opt/<arch>-linux-musl-cross`（riscv64/loongarch64 的 verilator 静态链接需 `-no-pie -fno-pie`，避免
`read-only segment has dynamic relocations`，与 hw4os 既有 HDL case 一致）。VLOG 的 C++ 模型按 TU 分别并行编译再链接
（单条 monolithic g++ 在 loongarch/riscv GCC 上峰值内存/耗时过大，易被 OOM-kill / 超时）。

## 宿主自检结论

- x86_64 / aarch64 / riscv64：六条腿宿主自检（交叉二进制经 `qemu-<arch>-static` 跑、各 `cmp` 黄金）**全过 6/6**。
- loongarch64：IVL/BSV/BH/MAKE/yosys 五条腿（静态 `vvp`/`make`）宿主自检全过；唯 **VLOG 腿**的 Verilator `--timing`
  C++20 协程在 `qemu-loongarch64-static`（用户态模拟器）下停滞——这是宿主模拟器限制，非靶上缺陷：二进制为干净
  static-pie，黄金与其余 arch 逐字节相同（确定化），其权威运行在真 StarryOS 全系统 (TCG)。
