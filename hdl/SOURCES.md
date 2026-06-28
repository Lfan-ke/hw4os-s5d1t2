# HDL / 仿真工具链 — 来源 / 构建 provenance

为 [#764](https://github.com/rcore-os/tgoskits/issues/764) 「`verilog <!-- verilator, iverilog, gnumake -->`」项。
在 StarryOS **单核四架构**(x86_64 / aarch64 / riscv64 / loongarch64)上跑真实的 Verilog 仿真,
两种主流开源仿真器 **verilator** 与 **iverilog** 均通过,并附带 **GNU Make** 本体运行通过(标识构建系统已适配)。

全部走同一条流水线:**host(官方工具链跑出黄金)→ qemu-10 用户态 Linux ×4 arch(交叉编译二进制)→ qemu-10 StarryOS ×4 arch 单核**,三段 stdout 全部精确匹配黄金。

## 1. verilator(`verilog/`)
- **Verilator 5.008**(host `apt`)。把综合 SystemVerilog 设计 `top.sv` verilate 成 C++,配 testbench `sim_main.cpp`。
- 用 musl 交叉工具链(`/opt/<arch>-linux-musl-cross`)把生成的 C++ 静态编译成 `testbin/vsim-<arch>`(CGO 无关,纯静态)。
- 设计覆盖:参数化 ALU(8 op + carry)、寄存器堆(memory array)、同步计数器、4 态枚举 FSM、generate 循环、packed enum、`always_comb`/`always_ff`。

## 2. iverilog(`iverilog/`)— 静态 vvp + 内嵌 system VPI
- **Icarus Verilog v12_0**(github `steveicarus/iverilog` tag `v12_0`,经 `codeload` tar.gz 取得)。
- host `iverilog -g2012 tb.v dut.v` 把共享的简单设计编译成 `dut.vvp`(bytecode);其 `:vpi_module` 指令被重写为只引用 `"system"`(去掉写死的 host 绝对路径 + 无关模块)。
- 运行时 `vvp` 用 musl 交叉工具链静态编译为 `testbin/vvp-<arch>`。**关键改造**:平常 `vvp` 靠 `dlopen("system.vpi")` 取 `$display/$finish/...`,但**静态 musl 不能 dlopen** → 把 system VPI 模块**静态内嵌**进 vvp:
  - 补丁 `vvp/vpi_modules.cc`(`-DVVP_STATIC_SYSTEM`):`system` 模块直接调链接进来的 `vlog_startup_routines[]`,绕开 dlopen;
  - 精简 system 表 `vpi/sys_table_static.c`:只注册文本 I/O 系统任务,**砍掉波形模块**(VCD/LXT/FST → 否则拖入 bzip2/zlib/nexus/线程,静态链接不进来);`readline_stub.c` + config.h 关 `HAVE_LIBREADLINE`。
- `dut.v` + `tb.v` 是**两个仿真器共享的一份源码**:host 上 iverilog(vvp)与 verilator(`--binary`)输出**字节级一致**,取 15 行核心 trace 为黄金。

## 3. GNU Make(`gnumake/`)
- **GNU Make 4.4.1**(`ftp.gnu.org`),`./configure --host=<arch>-linux-musl LDFLAGS=-static` 交叉编译为 `testbin/make-<arch>`(静态)。
- `Makefile` 练:变量(`:=`)、函数(`$(addprefix)`/`$(words)`/`$(sort)`)、pattern rule + 自动变量(`$*`)、先决/排序、**recipe fork `/bin/sh`** 执行(rootfs 内 busybox sh)。
- 标识:整条 iverilog/make 构建本就 autotools+make 驱动;且 make 本体也在 StarryOS 四架构运行通过。

## 交叉工具链
`/opt/{x86_64,aarch64,riscv64}-linux-musl-cross`(GCC 11.2.1)、`/opt/loongarch64-linux-musl-cross`(GCC 13.2.0)。
统一 `-static`;riscv64 早期需 `-no-pie -fno-pie`(只读段动态重定位)。

## aarch64 toml 关键点
`qemu-aarch64.toml` 的 `args` **必须含 `"-cpu", "cortex-a72"`** —— `qemu-system-aarch64 -machine virt` 默认核是 AArch32 的 cortex-a15,64 位内核根本起不来(无串口 → 超时)。loongarch64 需 `-cpu la464` + `-machine virt` + `to_bin=true` + **QEMU≥10**。
