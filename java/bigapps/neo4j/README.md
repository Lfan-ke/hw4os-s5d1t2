# neo4j — 嵌入式图数据库内核冒烟（StarryOS × 4 架构，4/4 通过）

#764「linux 大应用选取」neo4j 项的交付包。**Neo4j Community 2026.04.0** 嵌入式图数据库内核冒烟，在 qemu-10 单核 StarryOS 上 **x86_64 / aarch64 / riscv64 / loongarch64 全 11/11 通过**（四架构 4/4）。

## 跑的是什么

不是重型的 `neo4j console` HTTP server（Bolt :7687 + HTTP :7474 + system DB bootstrap，host 上 >5 min 无输出、starry 强制 `-Xint` 更不可行），而是用 `DatabaseManagementServiceBuilder` 在单个 JVM 里跑**真正的图数据库内核**——即 server 所包裹的同一套内核：page cache + record/token stores + 事务日志(WAL) + Cypher 解析/规划/执行。

驱动 `Neo4jEmbeddedSmoke.java` 含 **11 条 Cypher 断言**（CREATE 节点+关系、节点计数、关系遍历 Alice-KNOWS->Bob、关系属性 since、关系计数、属性 UPDATE、属性查询、DELETE 后计数、内核版本 2026.04.0、默认 DB available 等），全 round-trip 才记 pass。门控锚点 `^NEO4J_OK=1` 仅在 shell 独立确认驱动自报 `pass=total` 且 `total>=11` 时打印（防 success_regex 假阳性）。

底层依赖 mmap-EOF 内核修复（rcore-os/tgoskits#1164）：neo4j page cache 会 mmap store 文件，无 EOF 边界时超大映射会耗尽 RAM。

## 关键修复一：JNA-on-musl（**可复用于所有用 JNA 的应用**）

JNA 的 jar 内置 `libjnidispatch.so` 是 **glibc** 构建的，在 musl libc 上直接 `SIGSEGV`（尤其 riscv64——上游没有可取的 Alpine apk）。修法：

1. 把 musl 构建的 `libjnidispatch.so` 注入镜像 `/opt/jna/`（prep 据架构选 `jna/libjnidispatch-riscv64-musl.so` 或 `jna/libjnidispatch-loongarch64-musl.so`，经 `debugfs -w` 写到 `/opt/jna/libjnidispatch.so`）。
2. 启 java 时加 `-Djna.boot.library.path=/opt/jna -Djna.nounpack=true`（已写进 qemu toml）——让 JNA 直接用我们这份 musl `.so`，不再从 jar 解包它自带的 glibc 版本。

riscv64 那份是从 **JNA 5.15.0 源码**用 `riscv64-linux-musl-cross` 交叉编译（静态 libffi）得到。x86_64 / aarch64 不需注入——其 musl jnidispatch 已由 `jdk-multi` 基础镜像（`java-jna-native` apk）内置。

## 关键修复二：loongarch openjdk21 用 Zero VM

loong 的 openjdk21 是 **Zero VM** 构建——`libjvm.so` 在 `$JAVA_HOME/lib/zero/`，不在 `lib/server/`。因此 `ld-musl-loongarch64.path` 必须包含 `$JAVA_HOME/lib/zero`，否则 `libjimage` 报 *"Unable to load jimage library"* 起不来。`qemu-loongarch64.toml` 已在 ld-musl 路径行追加 `%s/lib/zero`。

## 目录结构

```
neo4j/
├── case/
│   ├── build-<target>.toml (4)         # starry 各架构构建配置
│   └── neo4j-0/qemu-<arch>.toml (4)     # 运行配置（含上面两个修复）
├── Neo4jEmbeddedSmoke.java / .class     # 11 断言驱动（.class = host 预编译 JDK21）
├── prep-neo4j-rootfs.sh                  # 构建 rootfs-<arch>-neo4j.img（portable）
├── jna/
│   ├── libjnidispatch-riscv64-musl.so    # JNA-on-musl 修复（注入 /opt/jna）
│   └── libjnidispatch-loongarch64-musl.so
├── packages/
│   └── neo4j-community-2026.04.0-unix.tar.gz  # 235 MB，Git LFS
├── README.md
└── SOURCES.md                            # 出处 + sha256
```

JDK 不入此目录——用例运行时取基础镜像 `rootfs-<arch>-jdk-multi.img` 的 `/opt/jdk21`（neo4j 核心 jar 是 class-major 65 = Java 21；OpenJDK17 会 `UnsupportedClassVersionError`）。

## 复现步骤

prep 脚本路径已参数化（不含开发机绝对路径）：tarball 取自本目录 `packages/`，musl jnidispatch 取自本目录 `jna/`，基础镜像目录由 `TGOSKITS_ROOT` 指定。

```bash
export TGOSKITS_ROOT=$HOME/tgoskits        # 你的 tgoskits checkout

# 1. 先有 jdk-multi 基础镜像（含 /opt/jdk{17,21,23,25} musl）
cargo xtask starry rootfs --arch <arch> ...   # 产出 rootfs-<arch>-jdk-multi.img

# 2. 注入 neo4j jars + 驱动 + (riscv64/loongarch64) musl jnidispatch
bash prep-neo4j-rootfs.sh <arch>              # arch ∈ x86_64|aarch64|riscv64|loongarch64
                                              # 产出 rootfs-<arch>-neo4j.img

# 3. 跑用例（每架构一次；必须 source .starry-env.sh 用 qemu-10）
cargo xtask starry test qemu -c neo4j-0       # 门控锚点：^NEO4J_OK=1
```

DoD = 四架构各 `NEO4J_OK=1`（驱动 `pass=11 total=11`），xtask `rc=0` + log `SUCCESS PATTERN MATCHED`。
