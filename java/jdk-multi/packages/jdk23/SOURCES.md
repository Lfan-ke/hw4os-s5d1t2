# jdk23 packages — 来源说明

| 文件 | 来源 | 版本 | sha256 |
|:--|:--|:--|:--|
| bellsoft-jdk23.0.2+9-linux-{x64,aarch64}-musl.tar.gz | BellSoft Liberica musl | 23.0.2+9 | 见 .gitattributes(LFS) |
| bellsoft-jdk23.0.2+9-linux-riscv64.tar.gz | BellSoft Liberica glibc(配 Debian glibc 运行时桥接) | 23.0.2+9 | — |
| loongson23.1.17-fx-jdk23_37-linux-loongarch64.tar.gz | Loongson glibc(旧 abi, 仅备查) | 23 | — |
| **openjdk23-loongarch64-musl-srcbuild.tar.gz** | **从源码交叉编译** —— 见同目录 `setup-loong-jdk23.sh` + `loong-jdk23-musl-port.patch`。上游 `loongson/jdk` tag `jdk-23+25-ls-0` → loongarch64-linux-musl(native)。**原因**: 上游/Alpine 无 musl JDK23 for loong,Loongson glibc 为旧 abi1.0 不兼容。 | jdk-23+25-ls-0 | 74ef309f3a18a1954a4f0b36146b987e4924e1da5de0310fccad3fa38c0c5ef2 |
