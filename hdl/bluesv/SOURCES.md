# bluesv (Bluespec) — 来源 / 构建 provenance

为 [#764](https://github.com/rcore-os/tgoskits/issues/764)「`bluesv <!-- bluespec systemverilog, system c -->`」项。
注释点名的两半都在 StarryOS **单核四架构**(x86_64 / aarch64 / riscv64 / loongarch64)运行通过:
**① Bluespec SystemVerilog → Verilog 仿真**(core)、**② SystemC**(systemc)。

流水线:**host(官方工具链出黄金)→ qemu-10 用户态 Linux ×4 arch(交叉静态二进制,md5 全等黄金)→ qemu-10 StarryOS ×4 arch 单核**,各段精确匹配黄金。

## 工具链
- **bsc**(Bluespec Compiler 2026.01,`/usr/local/bsc/bin/bsc`,`BLUESPECDIR=/usr/local/bsc/lib`)。
- **Accellera SystemC 2.3.4**(github tag),交叉编译为 musl 静态 `libsystemc-<arch>.a`。
- **Icarus Verilog v12_0 静态 vvp**(内嵌 system VPI + VCD,见 `../iverilog/`)做 core 的仿真器。
- musl 交叉:`/opt/{x86_64,aarch64,riscv64}-linux-musl-cross`(GCC 11.2.1)、`/opt/loongarch64-linux-musl-cross`(GCC 13.2.0)。

## 1. core — Bluespec SystemVerilog(`core/`)
- `Tb.bsv`:`interface`/`module`/`method`/`rule`/`Reg`(mkCounter + mkTb 测试台,确定性 `$display`)。
- `bsc -verilog -g mkTb -u Tb.bsv` → `mkTb.v`+`mkCounter.v`;`bsc -verilog -e mkTb -vsim iverilog` → `bluesv.vvp`(把 `:vpi_module` 改为只引用 `system`)。
- **iverilog 替 bluesim 当仿真器**:静态 vvp 跑 `bluesv.vvp +bscvcd` → `BLUESV_COUNT=8`/`BLUESV_SIM_OK` + 生成 **VCD 波形**(`dump.vcd`,184 行)。门 `BLUESV_OK=1`(含波形检查)。

## 2. systemc — SystemC(`systemc/`)
交叉编译 Accellera SystemC 2.3.4 musl 静态 4-arch。**2 源码补丁 + 4 构建标志**(详见 `systemc/systemc-2.3.4-starry.patch`):
1. **boost endian.hpp 源码补丁**(真因 A):`src/sysc/packages/boost/detail/endian.hpp` 只在 `__GLIBC__` 下用 `<endian.h>` 判字节序;**musl 非 __GLIBC__** + boost 架构列表缺 aarch64/riscv64/loongarch64 → `#error ... set up for your CPU`。修 = 加 `__BYTE_ORDER__`(编译器内建)兜底分支。
2. **sc_nbdefs.h int64 源码补丁**(真因 B):`int64` 仅对 x86_64/aarch64 typedef 成 `long long`;其余走 `int64_t`,在 riscv64/loongarch64 = `long`,与 `to_value(long)`/`sc_bit(long)` 重载冲突(`sc_bit.h:114 cannot be overloaded`)。修 = 把 `__riscv||__loongarch__` 加进 `long long` 分支(均 LP64,`long long` 是 64 位且与 `long` 不同类型)。
3. **`-DSC_USE_PTHREADS`**(`-DENABLE_PTHREADS=ON`):QuickThreads 无我们 4 架构汇编 → pthread 协程后端。
4. **`-std=c++14`**(lib 与 sc_main 一致):`sc_api_version` 符号带标准标签(`..._cxx201402L`)。
5. riscv64/loongarch64 链接 `-no-pie -fno-pie`,x86_64/aarch64 `-fPIE`。
6. **`make -j1`**:`-j4` 生成头竞态导致假失败,串行稳定。
- 产物 `libsystemc-<arch>.a`(x86 4.5M / aarch 4.6M / riscv 8.5M / loong 7.9M,nm 均含 6× `sc_api_version_2_3_4_cxx201402L`)。
- `sc_main.cpp`:`sc_module`+`sc_clock`+`SC_METHOD`+`SC_THREAD`+`sc_signal`+`wait`+`sc_start`+`sc_stop`(离散事件内核核心),确定性 `SC count=0..7` + `SYSTEMC_OK`(qemu-user 4 架构 md5 全等 `d21604ec`)。门 `SYSTEMC_OK_GATE=1`。
- `gen/`:空占位目录(预留 `bsc -systemc` 生成物)。本 systemc case 实际用手写 `sc_main.cpp` 直接驱动 SystemC 2.3.4 内核(验证内核在 starry 上运行),不依赖 bsc→SystemC 代码生成;Bluespec→Verilog 代码生成由 `core/`(bsc `-verilog`)演示。
- `endian.hpp.patched` / `sc_nbdefs.h.patched`:打好补丁的两个头,供直接替换。

## toml 关键点
aarch64 `qemu-*.toml` 必须 `-cpu cortex-a72`(否则 virt 默认 AArch32,64位内核不启动);loongarch64 需 QEMU≥10 + `-cpu la464` + `-machine virt` + `to_bin=true`。

## 备注
DPI-C(verilator)、BDPI、bsc `.sched` 为额外深度,非注释点名项,本交付未含(后续可补)。本交付聚焦注释明确要求的两半(SV + SystemC),均 **4/4 starry 绿 = 8/8**。
