# bluesv 测例 — 流水线与测试结果

对应 tgoskits 测例:`test-suit/starryos/stress/{bluesv-0, systemc-0}`。
#764 注释两半:**bluespec systemverilog**(core)+ **system c**(systemc),均 **4/4 starry 绿**。

## 测试结果(2026-05-30,StarryOS @ qemu-10,单核,抗篡改信道核验)

| 注释半边 | 测例 | x86_64 | aarch64 | riscv64 | loongarch64 |
|---|---|:--:|:--:|:--:|:--:|
| **bluespec systemverilog**(bsc→Verilog→静态 vvp + VCD 波形) | `bluesv-0` | √ | √ | √ | √ |
| **system c**(SystemC 2.3.4 离散事件内核,musl 静态) | `systemc-0` | √ | √ | √ | √ |

starry 运行 = **8/8**(两 case × 4 arch,各 `result: 1/1 case`)。门:bluesv→`BLUESV_OK=1`(含 VCD 波形检查),systemc→`SYSTEMC_OK_GATE=1`。stdout 与 rootfs 内黄金 `cmp` 精确匹配。

## 黄金
- bluesv:`BLUESV_COUNT=8` + `BLUESV_SIM_OK`(+ 生成 184 行 VCD 波形)。
- systemc:`SC count=0..7`(8 行)+ `SYSTEMC_OK`(qemu-user 4 架构 md5 全等 = `d21604ec`)。

## 流水线(每段精确匹配黄金)
host(bsc/SystemC 官方工具链出黄金)→ qemu-user Linux ×4 arch(交叉静态二进制 md5 全等黄金)→ starry ×4 arch。

## 复现
每目录:`build-<target>.toml`×4 + `qemu-<arch>.toml`×4 + `prep-*-rootfs.sh`(debugfs 注入二进制+黄金到 alpine rootfs)+ 源码 + `testbin/<arch 静态二进制>`。systemc 另含 `lib/libsystemc-<arch>.a` + 手写 `sc_main.cpp`(驱动 SystemC 2.3.4 内核;`gen/` 为空占位目录,本 case 不依赖 bsc→SystemC 生成物——Bluespec→Verilog 生成由 `core/` 演示)+ `systemc-2.3.4-starry.patch`(2 源码补丁 + 4 构建标志)+ `endian.hpp.patched` + `sc_nbdefs.h.patched`。
把 toml 放回 tgoskits 对应 case,prep 造 rootfs,qemu-10 四架构 starry 复跑。**aarch64 必须 `-cpu cortex-a72`;loongarch 必须 QEMU≥10**。
