# 多版本 JDK 下载来源清单（方案二 · #764 · java/jdk 收官子课题）

适配目标：StarryOS × 4 架构（x86_64 + aarch64 + riscv64 + loongarch64）。
跟踪 issue：rcore-os/tgoskits#764。

> 2026-05-24 web-research 补齐 **JDK25 loongarch64**。
> 既有：JDK21/23/25 的 x86_64+aarch64（BellSoft Liberica musl）+ riscv64（BellSoft glibc），及 JDK21/23 loongarch64（Loongson `loongsonNN-fx-jdk` glibc 整包）。**缺口 = JDK25 loongarch64**。

---

## 0. JDK25 loongarch64 缺口补齐结论

| 来源候选 | 结果 |
|----------|------|
| Oracle / BellSoft Liberica / Temurin / Microsoft | × 均无 loongarch64（任何版本） |
| Alibaba Dragonwell | × 仅 x86_64 + aarch64，**无 loongarch64**（官方 README 明确只 Linux/x86_64+aarch64） |
| Loongson 官方 release（github loongson/jdk、build-tools） | × `loongson/jdk` 是源码仓**无 release**；build-tools releases 无 JDK 资产；ftp.loongnix.cn/toolchain/java 只到 openjdk6/8/13 |
| **Alpine Linux edge/community（loongarch64，musl）** | √ **有 openjdk25！两个变体**（见下） |

**关键发现**：Alpine `edge/community/loongarch64` 仓库已构建 **OpenJDK 25 的 musl 版 loongarch64 二进制**，正好匹配 Alpine-musl StarryOS 环境（比既有 Loongson glibc 整包更贴合）。两个变体：

| 变体 | 版本 | JIT | libjvm | 说明 |
|------|------|-----|--------|------|
| `openjdk25-loongarch-*` | **25.0.1_p8-r1** | **C2 server JIT** | `lib/server/libjvm.so` | **LoongArch 原生端口**（含 JIT，高性能，首选） |
| `openjdk25-*`（loongarch64 arch 构建） | 25.0.3_p9-r1 | Zero 解释器 | `lib/zero/libjvm.so` | 纯解释器、无 JIT、可移植兜底 |

两变体均 musl 链（`libc.musl-loongarch64.so.1`），均落盘于 `jdk25/loongarch64-alpine-musl/`，gzip 完整性 + 内含 `bin/java`+`libjvm.so` 已核对。

---

## 1. 来源 / 镜像

Alpine apk pool（任一镜像可替换 host）：
```
https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/<pkg>-<ver>.apk
```
apk = gzip-压缩 tar；落 rootfs 用 `apk add` 或手动 `tar xzf` 解到 `/usr/lib/jvm/java-25-openjdk/`。运行期依赖 musl + 各 apk 互相声明的 so 依赖（解 APKINDEX 依赖链；典型需 java-common / libstdc++ / zlib / freetype（非 headless 图形栈）等，全在 Alpine loongarch64 仓库可得）。

---

## 2. SHA256（2026-05-24 核对）

LoongArch 原生 JIT 端口（25.0.1_p8-r1，首选）：
```
42a6d9a8dafa885c37a763a5b70814915bb73879bc8249222566f175d6cd2772  jdk25/loongarch64-alpine-musl/openjdk25-loongarch-25.0.1_p8-r1.apk
4e1c4d1aada0a4f524ec5200ec705cb0c7769fe77e0f5d7ccf998becafce61df  jdk25/loongarch64-alpine-musl/openjdk25-loongarch-jdk-25.0.1_p8-r1.apk
a8ab4d738501c5ff3a8257d2c9e85684200dd5275b097efe0cadaa80693dddb6  jdk25/loongarch64-alpine-musl/openjdk25-loongarch-jre-25.0.1_p8-r1.apk
28e19f2c14d8137d9e767347ef61953af99fec263a1374e6221d4d22b8ef3796  jdk25/loongarch64-alpine-musl/openjdk25-loongarch-jre-headless-25.0.1_p8-r1.apk
cb6886b8f3f5306f2f509c542d94a1adba523e08881a1fc73a762da7ce3b5fc0  jdk25/loongarch64-alpine-musl/openjdk25-loongarch-jmods-25.0.1_p8-r1.apk
```
Zero 解释器兜底（25.0.3_p9-r1）：
```
d80c211912e7319b51753e3abc4ed84995aaee5c74244b70affcb393a1edccbe  jdk25/loongarch64-alpine-musl/openjdk25-25.0.3_p9-r1.apk
38bf9508ba53fa604f7169510fd9c8b72672871ff76415634c80eaee902f3ea3  jdk25/loongarch64-alpine-musl/openjdk25-jdk-25.0.3_p9-r1.apk
1aba2d825748b5913d56d8856f31830481fc786107f04dd3a8c7f61ec8bd518f  jdk25/loongarch64-alpine-musl/openjdk25-jre-25.0.3_p9-r1.apk
41b736535311500e42a2c2f949c38e5f0bb65c20d02158dc4fa382690f9bb914  jdk25/loongarch64-alpine-musl/openjdk25-jre-headless-25.0.3_p9-r1.apk
81b2786e32e9ee889c9d28efecc522c330b97c9a8f6922fab077566d25c57b9c  jdk25/loongarch64-alpine-musl/openjdk25-jmods-25.0.3_p9-r1.apk
```

---

## 3. 适配提示

- **首选 `openjdk25-loongarch-*` (25.0.1_p8)**：带 C2 server JIT 的 LoongArch 原生端口，与 java app 最相关（JIT 触发的 mmap/mprotect/icache flush 是 StarryOS 内核压力点）。Zero 变体仅作无 JIT 兜底对照。
- musl 链 → 与 Alpine StarryOS rootfs 直接兼容，无需 glibc 兼容层（优于既有 Loongson glibc 整包 jdk21/23）。
- 既有 4-arch JDK25 全家福（补齐后）：x86_64 musl(BellSoft) + aarch64 musl(BellSoft) + riscv64 glibc(BellSoft) + **loongarch64 musl(Alpine OpenJDK25)** √。
