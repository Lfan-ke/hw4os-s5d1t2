# lang/llvm22 — 软件包来源 (provenance)

## clang-22 / LLVM 22.1.6
- **LLVM 22.1.6 官方 release**(clang-22 toolchain):来自 LLVM 官方 GitHub release。
  - x86_64 / aarch64:官方 `LLVM-22.1.6-Linux-{X64,ARM64}.tar.xz`。
  - riscv64 / loongarch64:见 `<本机下载缓存目录>/llvm-bins/{riscv64,loongarch64}/`(社区/自编 chain)。
  - 逐文件 URL / 版本 / sha256:`<本机下载缓存目录>/llvm-bins/SOURCES.md`(下载侧权威记录)。
- 用法:clang-22 AOT codegen(交叉编译 C++23 测试为 4 arch 静态二进制),非 JIT。`testbin/llvm22-<arch>` 是产物;`prep-llvm22-rootfs.sh` 注入。
- 四架构覆盖与逐 arch 据实分析见 `../LANG-4ARCH-ANALYSIS.md`。
