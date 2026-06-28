# java/jdk-multi/ — 多版本 JDK（17/21/23/25）+ 版本切换用例

满足 #764 父项 **`jdk17+ <!-- openjdk - 17 21 23 25 - update-alternatives -->`**：在每个架构上对 **JDK 17 / 21 / 23 / 25** 各跑一组**版本专属的语言/标准库/语法特性测试**（先断言「跑起来的 JVM 确实是这个大版本」），再用**两种方式做版本切换**（update-alternatives 风格 + sdkman 风格）。聚合 gate `JDK_MULTI_OK=1` 仅当全部子套件通过才触发。

对应 tgoskits 用例：**`test-suit/starryos/stress/openjdk-multi-0/`**（其 4 架构 `qemu-<arch>.toml` + `build-*.toml` + `programs/Jdk{17,21,23,25}Features.java` 已在上游）。

> 状态：用例就绪 + host 全部验证通过；待 StarryOS guest 4 架构运行确认后给 #764 打勾。

---

## 目录内容

| 项 | 说明 |
|----|------|
| `SOURCES.md` | **逐包来源**（JDK25 loongarch64 缺口补齐结论 + 各 vendor + sha256）。权威来源文件 |
| `CASE-NOTES-jdk-multi.md` | 完整设计笔记：每版本测哪些特性、版本 gate、切换子测、per-arch musl/glibc 映射、debugfs 构建步骤、host 验证 |
| `packages/jdk21,jdk23,jdk25/` | 三个高版本 JDK 的 tar/apk（JDK17 复用 `../openjdk17/`） |
| `programs/Jdk{17,21,23,25}Features.java` | 4 个版本特性测试源码（与上游用例一致，留此备查/复现） |
| `case/` | 4 架构 `qemu-<arch>.toml` + `prep-jdk-multi-rootfs.sh` + `host-ref/`（host 期望输出 + host 仿真脚本） |

---

## 各版本测什么（详见 `CASE-NOTES-jdk-multi.md` §2）

每个测试先做**版本红线**：`Runtime.version().feature()` 必须等于期望大版本，否则抛异常（版本不对 = 硬失败，绝不静默通过）。

- **JDK17**（LTS，全 final）：records · sealed interface + permitted records · instanceof 模式匹配 · text blocks · switch 表达式 · `Stream.toList()`。
- **JDK21**（LTS，全 final）：**虚拟线程**（fan-out 1000）· record patterns · guarded switch（`when`）+ null/default · **sequenced collections** · `Math.clamp`。
- **JDK23**（preview，跑在 `--enable-preview --source 23`）：**Flexible Constructor Bodies**(JEP 482) · **Stream Gatherers**(JEP 473) · 稳定的嵌套 record pattern + `Stream.mapMulti`。
- **JDK25**（LTS）：**Scoped Values**(JEP 506, final) · **Module Import Declarations**(JEP 511, final) · **Compact Object Headers**(JEP 519, 二跑 `-XX:+UseCompactObjectHeaders` 验标志) · **Stable Values**(JEP 502, preview)。

## 版本切换（#764 「update-alternatives」意图）

4 个 JDK 并排装在 `/opt/jdk{17,21,23,25}`。

1. **update-alternatives 风格**（主路径）：重定向 `/opt/jdk-current` 符号链接 + 设 `JAVA_HOME`，`java -version` 必须报对应大版本 → `SWITCH ok=4/4`。（StarryOS rootfs 只有 busybox 无 `update-alternatives` 二进制，故直接做底层符号链接切换 —— 语义等价。）
2. **sdkman 风格**（离线）：预置 `~/.sdkman/candidates/java/{17,21,23,25}-open` 链接，重定向 `current`（即 `sdk use java <v>`）后再验 `java -version` → `SDK-SWITCH ok=4/4`。（`sdk install` 需网络，离线镜像只能预置 + 切换。）

## per-arch JDK 来源映射（musl vs glibc，详见 CASE-NOTES §5）

|         | JDK17 | JDK21 | JDK23 | JDK25 |
|---------|-------|-------|-------|-------|
| x86_64 | openjdk17 apk(musl) | BellSoft musl | BellSoft musl | BellSoft musl |
| aarch64 | openjdk17 apk(musl) | BellSoft musl | BellSoft musl | BellSoft musl |
| riscv64 | native-musl cross | BellSoft **glibc** | BellSoft **glibc** | BellSoft **glibc** |
| loongarch64 | openjdk17-loong apk(musl) | Loongson **glibc** | Loongson **glibc** | Alpine 原生 **musl**(C2 JIT 端口) |

> glibc 单元格（riscv 21/23/25；loong 21/23）通过 rootfs 内的 **gcompat** shim 桥接 glibc `libc.so.6`/`ld-linux`。若 gcompat 下 glibc JDK 仍失败，则为真实的架构覆盖缺口，据实记录。

`packages/jdk25/loongarch64-alpine-musl/` 含两变体：`openjdk25-loongarch-*`（25.0.1_p8，C2 JIT 原生端口，首选）+ `openjdk25-*`（25.0.3_p9，Zero 解释器兜底）。

---

## 怎么构建 + 跑（QEMU v10）

构建 `rootfs-<arch>-jdk-multi.img`（从 alpine 基础镜像新建，并排装 4 JDK；用 `debugfs -w`，不 mount 不 sync）：

```
cd <tgoskits>
cargo xtask starry rootfs --arch <arch>     # 确保 rootfs-<arch>-alpine.img 基础镜像存在
bash <本仓库>/java/jdk-multi/case/prep-jdk-multi-rootfs.sh <arch>
#   -> tmp/axbuild/rootfs/rootfs-<arch>-jdk-multi.img（约 6G；loong 用 8G mem 容得下）
#      装 /opt/jdk{17,21,23,25} + /opt/jdk-current 符号链接 + /root/jdkm/*.java
#      + ~/.sdkman candidates + gcompat(riscv/loong) + /etc/ld-musl-<arch>.path
```

跑（4 架构；x86_64 用 `-smp 1 -m 4096M`，见 `case/qemu-x86_64.toml`）：

```
cargo xtask starry test qemu --arch x86_64      -g stress -c openjdk-multi-0   # 期望 JDK_MULTI_OK=1
cargo xtask starry test qemu --arch aarch64     -g stress -c openjdk-multi-0
cargo xtask starry test qemu --arch riscv64     -g stress -c openjdk-multi-0
cargo xtask starry test qemu --arch loongarch64 -g stress -c openjdk-multi-0
```

聚合 gate：7 个子套件（JDK17/21/23/25 + JDK25-COMPACT + SWITCH + SDK-SWITCH）全 pass → `printf 'JDK_MULTI_OK=%s' 1`，`success_regex = ^JDK_MULTI_OK=1`。

### 手动验证单个版本（不跑整套）

在 guest shell 里：

```
export JAVA_TOOL_OPTIONS=-Xint
/opt/jdk21/bin/java -Xint -Xmx512m -Xms64m /root/jdkm/Jdk21Features.java     # 期望末行 JDK21_OK
/opt/jdk23/bin/java -Xint -Xmx512m -Xms64m --enable-preview --source 23 /root/jdkm/Jdk23Features.java   # JDK23_OK
```

期望输出对照 `case/host-ref/Jdk<N>Features.out`。
