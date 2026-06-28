# jdk25 packages — 来源说明

| 文件 | 来源 | 版本 | sha256 |
|:--|:--|:--|:--|
| bellsoft-jdk25+37-linux-{x64,aarch64}-musl.tar.gz | BellSoft Liberica musl | 25+37 | 见 .gitattributes(LFS) |
| bellsoft-jdk25+37-linux-riscv64.tar.gz | BellSoft Liberica glibc | 25+37 | — |
| loongarch64-alpine-musl/openjdk25-loongarch-* | Alpine edge/community (C2 JIT 原生端口, 首选) | 25.0.1_p8-r1 | — |
| **openjdk25-riscv64-musl-srcbuild.tar.gz** | **从源码交叉编译** —— 见同目录 `setup-rv-jdk25.sh` + `rv-jdk25-musl-port.patch`。上游 `openjdk/jdk25u` tag `jdk-25.0.4+5` → riscv64-linux-musl(native server VM)。**原因**: 预构建 riscv64 JDK25 server VM 在 RV64GC baseline 上发出保留压缩指令 `C.LUI x5,0`(0x6281)→ IllegalInstruction;patch 给 `lui()→c_lui` peephole 补回 `(imm & 0xfff)==0` 守卫,消除该指令。 | jdk-25.0.4+5 | 42bb25a018faf3bba253bffc9b0f18964bb231504e3fe61c77b32e4268b3e847 |
