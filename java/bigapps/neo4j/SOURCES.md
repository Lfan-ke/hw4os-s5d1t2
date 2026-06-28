# neo4j 交付物来源清单（provenance + sha256）

本目录是 #764「linux 大应用选取」neo4j 项的**离线可复现**交付包（StarryOS × 4 架构，qemu-10 单核，4/4 通过）。所有 payload 与其上游出处、校验值如下，便于维护者核对与重新获取。

---

## 1. neo4j 发行 tarball

| 项 | 值 |
|----|----|
| 文件 | `packages/neo4j-community-2026.04.0-unix.tar.gz` |
| 大小 | 235062100 字节（≈235 MB，Git LFS，`.gitattributes` 已 track `*.tar.gz`） |
| 版本 | Neo4j Community **2026.04.0**（CalVer，最新；5.26.x 为 LTS 备选） |
| 上游 URL | `https://dist.neo4j.org/neo4j-community-2026.04.0-unix.tar.gz` |
| 重新下载 | `curl -L https://dist.neo4j.org/neo4j-community-2026.04.0-unix.tar.gz -o packages/neo4j-community-2026.04.0-unix.tar.gz` |
| sha256 | `fd750466b1247c0d1ef09a84c614f7e045793b30dfa277148e8da71646598820`（= 官方 `.sha256` √） |

> 架构无关：neo4j-community 是纯 Java 发行（仅 `*.jar` + shell 脚本，无 per-arch 原生二进制），同一份 tar 四架构通用。prep 只抽取 `lib/*.jar`（235 个，≈139 MB）注入 `/opt/neo4j/lib`。

---

## 2. musl libjnidispatch.so（JNA-on-musl 关键修复，**可复用于所有 JNA 应用**）

JNA 的 jar 内置 `libjnidispatch.so` 是 **glibc** 构建的，在 musl 上 `SIGSEGV`。riscv64 上游无 Alpine apk 可取，故交叉自建 musl 版本注入 `/opt/jna/libjnidispatch.so`。

| 文件 | 大小 | sha256 | 构建来源 |
|------|------|--------|----------|
| `jna/libjnidispatch-riscv64-musl.so` | 104592 | `591c7458d32eda5aa2bef9ee88bc4b0e7120da65fed98e55af1149fecc076c66` | 自 JNA 5.15.0 源码用 `riscv64-linux-musl-cross` 交叉编译（静态 libffi） |
| `jna/libjnidispatch-loongarch64-musl.so` | 100552 | `53bc2a313c307d5a1d2428538618e74adc244ea209d62605d419c28bd0094f69` | musl 构建（loongarch64） |

> x86_64 / aarch64 不在此目录：它们的 musl jnidispatch 已由 `jdk-multi` 基础镜像（`java-jna-native` apk）内置，prep 对这两架构不做注入（见 prep 的 `case "$ARCH"` 分支）。

---

## 3. 测试用例 / 驱动 / 构建脚本（本仓库自产）

| 文件 | 说明 | 来源 |
|------|------|------|
| `Neo4jEmbeddedSmoke.java` | 嵌入式图数据库内核冒烟驱动（11 条 Cypher 断言） | 本仓库自写 |
| `Neo4jEmbeddedSmoke.class` | host 预编译（JDK21，class-major 65） | 由上面的 `.java` 编译 |
| `prep-neo4j-rootfs.sh` | 构建 `rootfs-<arch>-neo4j.img`（debugfs 注入，无 mount/sync） | 本仓库自写（路径已参数化，不含开发机绝对路径） |
| `case/build-<target>.toml` ×4 | starry 各架构构建配置 | `tgoskits/test-suit/starryos/stress/neo4j-0/`（已含修复，逐字拷贝） |
| `case/neo4j-0/qemu-<arch>.toml` ×4 | starry 各架构运行配置（**含 JNA + Zero VM 修复**） | 同上（逐字拷贝，未改动） |

qemu toml 中的修复点（已 baked 进拷贝文件）：
- `qemu-riscv64.toml`：java 启动加 `-Djna.boot.library.path=/opt/jna -Djna.nounpack=true`。
- `qemu-loongarch64.toml`：上面那行 **外加** `ld-musl-loongarch64.path` 里追加 `%s/lib/zero`（loong openjdk21 用 Zero VM，`libjvm.so` 在 `lib/zero/` 而非 `lib/server/`）。

---

## 4. 基础镜像（不入交付包，由 tgoskits 构建）

prep 基于 `$TGOSKITS_ROOT/tmp/axbuild/rootfs/rootfs-<arch>-jdk-multi.img`（Alpine musl + `/opt/jdk{17,21,23,25}`，ld-musl 路径已为四 JDK 接好）。**JDK 不入此交付目录**——用例运行时取 `/opt/jdk21`（neo4j 核心 jar 是 class-major 65 = Java 21，OpenJDK17 会 `UnsupportedClassVersionError`，neo4j 自己的 launcher 也拒绝低于 Java 21）。
