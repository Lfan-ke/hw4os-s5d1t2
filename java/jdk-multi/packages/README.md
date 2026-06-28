# jdk-multi/packages/ — JDK 21 / 23 / 25 发行包（JDK17 复用 ../../openjdk17/）

多版本用例并排装的高版本 JDK。**JDK17 不在这里**（复用 `../../openjdk17/` 的 musl apk）。每包来源 / vendor / sha256 见 `../SOURCES.md`；per-arch musl vs glibc 映射见 `../CASE-NOTES-jdk-multi.md` §5。

```
packages/
├── jdk21/    BellSoft musl(x64/aarch64) + BellSoft glibc(riscv64) + Loongson glibc(loongarch64)
├── jdk23/    同上结构（BellSoft 23.0.2 + Loongson 23）
└── jdk25/    BellSoft musl(x64/aarch64) + BellSoft glibc(riscv64) + loongarch64-alpine-musl/（Alpine 原生 musl）
    └── loongarch64-alpine-musl/   openjdk25-loongarch-*（25.0.1_p8, C2 JIT 原生端口, 首选）
                                   + openjdk25-*（25.0.3_p9, Zero 解释器兜底）
```

落地：musl tar/apk 直接解到 `/opt/jdk<N>`；glibc 单元格（riscv 21/23/25；loong 21/23）配 rootfs 内的 gcompat shim 桥接 glibc 引用（gcompat apk 取自 `../../openjdk17/packages/<arch>/`）。`prep-jdk-multi-rootfs.sh` 自动处理。

> **关键发现（见 `../SOURCES.md`）**：JDK25 loongarch64 一度是缺口（Oracle/BellSoft/Temurin/Dragonwell/Loongson 都无）；最终在 Alpine edge/community loongarch64 仓库找到 **openjdk25 musl 版**（两变体），正好匹配 Alpine-musl StarryOS 环境，比 Loongson glibc 整包更贴合。
