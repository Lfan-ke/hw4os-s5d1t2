# java/openjdk17/ — OpenJDK 17 musl 运行时（4 架构）+ openjdk17-0 用例基座

这是整个 Java 子课题的**运行时基座**：4 个 CPU 架构各自的 musl-libc OpenJDK 17（JRE/JDK），加上 riscv64 的 glibc 回退方案与 gcompat 兼容层。它对应 tgoskits 的 **`test-suit/starryos/stress/openjdk17-0/`** 压力用例 —— 该用例在 4 架构上验证 javac/maven/gradle/kotlin + 十几个框架 demo（见 `../dod-frameworks/`）。

适配目标：**类 Alpine Linux OS（musl libc）× 4 架构**。

---

## 目录内容

| 项 | 说明 |
|----|------|
| `SOURCES.md` | **逐包来源清单**（Alpine CDN URL / 镜像 / 版本 / 依赖闭包 / riscv64 三种部署路径）。**这是来源说明的权威文件** |
| `THANKS.txt` | 上游致谢 |
| `packages/` | 实际的 `.apk` / `.tar.gz` / `.deb` 包，按架构分目录（见 `packages/README.md`） |

---

## 4 架构来源速览（详见 `SOURCES.md`）

| 架构 | 状态 | 主源 | 备注 |
|------|------|------|------|
| `x86_64` | √ 完整 musl APK + gcompat | Alpine v3.22 community | `openjdk17-{jdk,jre,jre-headless,jmods}-17.0.18_p8-r0` + 32 包依赖闭包 |
| `aarch64` | √ 完整 musl APK + gcompat | Alpine v3.22 community | 同 x86_64，URL `<ARCH>` 替换 |
| `loongarch64` | √ 完整 musl APK + **LoongArch 原生变体** + gcompat | Alpine edge community | 含 `openjdk17-loongarch-*`（LA64 ISA 补丁变体） |
| `riscv64` | ! 无官方 musl APK | Adoptium / BellSoft / Debian / 源码 backport | 见下「riscv64 特殊情况」 |

### riscv64 特殊情况（关键）

OpenJDK 上游的 RISC-V port **在 JDK 21 才合并，JDK 17 没有官方 RISC-V 支持**，任何 vendor 都没发布 musl+riscv64+JDK17 预编译产物。本目录因此提供三条路径（推荐度降序，详见 `SOURCES.md` §三）：

1. **gcompat + glibc tar**（已全部下载，立即可用）：`bellsoft-jdk/jre17.0.19+11-linux-riscv64.tar.gz`、`OpenJDK17U-jdk_riscv64_linux_hotspot_17.0.19_10.tar.gz`、`debian-glibc/*.deb`，配 `gcompat-1.1.0-r4.apk`（+ `libucontext`/`musl-obstack` 依赖）在 musl 上运行 glibc 二进制。
2. **直接用 openjdk21 musl APK**（Alpine 官方原生 musl，但是 JDK 21 不是 17）。
3. **源码编译 musl 版 openjdk17-riscv64**（最末选择；3.3 GB 自举构建临时树未纳入本仓库，需要时按 `SOURCES.md` §三里的 `BUILD_FROM_SOURCE.md` 指引重建）。另已收一份 `openjdk17-riscv64-musl-NATIVE-cross.tar.gz`（真 musl 原生交叉产物）。

> 说明：`packages/riscv64/` **不含** 原始的 3.3 GB `source-build/` 自举临时树（纯构建临时产物，非交付物），其来源与重建方法见 `SOURCES.md`。其余所有 riscv64 包（glibc tar/deb、native-cross、gcompat、依赖 apk）都已纳入。

---

## 怎么用（构建 `rootfs-<arch>-java.img` 基座）

这些包被构建进基础镜像 `rootfs-<arch>-java.img`（再叠加 `../toolchain/` 与 `../dod-frameworks/` 的内容）。落地方式 = `apk add`（musl 架构）或手动 `tar xzf` 解到 `/usr/lib/jvm/java-17-openjdk/`，riscv64 走 gcompat + glibc tar。

JVM 入口在 guest 内统一是 `/usr/lib/jvm/java-17-openjdk/bin/java`；musl 加载器路径通过

```
printf '/lib\n/usr/lib\n%s/lib\n%s/lib/server\n' "$JAVA_HOME" "$JAVA_HOME" > /etc/ld-musl-<arch>.path
```

写好（每个 `qemu-<arch>.toml` 的 `shell_init_cmd` 开头都做这件事）。

---

## 对应 tgoskits 用例：`openjdk17-0`

该用例的 4 架构 `qemu-<arch>.toml` + `build-*.toml` + `programs/`（Fib/HelloWorld/IOTest/Sieve/ThreadTest）**已在上游 tgoskits 仓库**（`test-suit/starryos/stress/openjdk17-0/`）。它的 `shell_init_cmd` 是一个聚合 gate：

* `JAVAC` —— `javac` 编译 5×（全成才记 pass）
* `CDEMO` —— 跑 `../complex-demo/` 的 jar 5×（grep `CDEMO_DONE`）
* `MVN` / `GRADLE` / `KOTLIN` —— 构建工具（见 `../toolchain/`）
* `JSE ...` —— 15 个 JSE 标准库子套件（见 `../dod-frameworks/jse-suite/`）
* `EE-JETTY / EE-SPRING / EE-NETTY / EE-SQLITE / EE-MYBATIS / EE-LOMBOK / EE-HIBERNATE / EE-SPRINGDATA / EE-SPRINGKT / EE-UNDERTOW / EE-R2DBC / EE-COROUTINES / EE-EXPOSED` —— 框架 demo（见 `../dod-frameworks/`）

聚合规则：`PASS==TOTAL` 才 `printf 'OPENJDK17_OK=%s\n' 1`，`success_regex = ^OPENJDK17_OK=1`。任一子套件 `ok=0` → `PASS<TOTAL` → 失败（无静默通过）。

手动运行（QEMU v10）：

```
cd <tgoskits>
cargo xtask starry rootfs --arch x86_64
cargo xtask starry test qemu --arch x86_64 -g stress -c openjdk17-0    # 期望 OPENJDK17_OK=1
# aarch64 / riscv64 / loongarch64 同理
```
