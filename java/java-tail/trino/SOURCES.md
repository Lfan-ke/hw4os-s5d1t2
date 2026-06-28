# Trino — SOURCES & PROVENANCE（所有文件的来源、版本、sha256、获取方式）

> 本目录所有文件都可校验、可复现下载。LFS 跟踪的大二进制需 `git lfs install && git lfs pull` 后才是真实内容。

## 1. 测试驱动源码

| 文件 | 来源 | 版本/Commit | sha256 |
|---|---|---|---|
| `package/TrinoDemo.java` | 本项目原创（Lfan-ke），交付仓库 `java-apps/dod/trino/` 同步副本 | 1.0.0 | `f9b86ea3...` |
| `package/pom.xml` | 本项目原创（Lfan-ke），maven POM，pin trino 435 + maven-dependency-plugin 拷 trino-libs/ | 1.0.0 | `13d5d16c...` |

`TrinoDemo.java` 用 `io.trino.testing.LocalQueryRunner`（trino-main test-jar）+ `io.trino.plugin.tpch.TpchPlugin` 跑 3 条查询，验证真 Trino 引擎（parser/analyzer/planner/optimizer/executor）在 StarryOS 上运行通过。

## 2. Trino 上游

| 文件 | 上游 | 版本 | 下载 URL | sha256 |
|---|---|---|---|---|
| `package/trino-server-476.tar.gz` | [trinodb/trino](https://github.com/trinodb/trino) | **476** | <https://repo1.maven.org/maven2/io/trino/trino-server/476/trino-server-476.tar.gz>（821 MB） | `cfd5acc...` |
| `package/trino-cli-481-executable.jar` | trinodb/trino | **481** | <https://repo1.maven.org/maven2/io/trino/trino-cli/481/trino-cli-481-executable.jar>（18 MB） | `9532fb7a...` |

**当前用例不用 server-476 也不用 cli-481**——保留以供：
- `cli-481` MANIFEST `Java-Version: 11` → 可在 JDK17 直接 `--version` 跑当 sanity；
- `server-476` 在 starry JDK24+ 真的能跑且 musl-libjnidispatch 修通后做完整 distributed coordinator 测试。

实际用例（`prep-trino-rootfs.sh`）所需的 trino jar 由 `mvn -f package/pom.xml package` 拉取 **trino 435**（最后 JDK17 兼容线）的运行时依赖。Maven Central 路径：
```
https://repo1.maven.org/maven2/io/trino/trino-main/435/
https://repo1.maven.org/maven2/io/trino/trino-tpch/435/
https://repo1.maven.org/maven2/io/trino/trino-testing/435/
（含 trino-main-435-tests.jar = LocalQueryRunner 所在）
+ ~190 个传递依赖
```

## 3. JNA & libjnidispatch（faithful 路径资源）

| 文件 | 来源 | 版本 | 下载 URL | sha256 |
|---|---|---|---|---|
| `jna/jna-5.15.0.jar` | [java-native-access/jna](https://github.com/java-native-access/jna) | 5.15.0 | <https://repo1.maven.org/maven2/net/java/dev/jna/jna/5.15.0/jna-5.15.0.jar> | `a564158...` |
| `jna/libjnidispatch-x86_64-musl.so` | Alpine community `java-jna-native` | 5.15.0-r0 | <https://dl-cdn.alpinelinux.org/alpine/edge/community/x86_64/java-jna-native-5.15.0-r0.apk> → `usr/lib/libjnidispatch.so` | `d4697323...` |
| `jna/libjnidispatch-aarch64-musl.so` | Alpine community `java-jna-native` | 5.15.0-r0 | <https://dl-cdn.alpinelinux.org/alpine/edge/community/aarch64/java-jna-native-5.15.0-r0.apk> → `usr/lib/libjnidispatch.so` | `c55a70b6...` |
| `jna/libjnidispatch-loongarch64-musl.so` | Alpine community `java-jna-native` | 5.15.0-r0 | <https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/java-jna-native-5.15.0-r0.apk> → `usr/lib/libjnidispatch.so` | `53bc2a3...` |
| **riscv64 musl** | **Alpine 未打包** | — | 需要从 [JNA 源码 5.13.0 / 5.15.0](https://github.com/java-native-access/jna) cross-compile `native/` (ant native + libffi)，可用 `/opt/riscv64-linux-musl-cross` 工具链 | — |

**为什么 5.15 而不是 trino-435 自带的 5.13**：JNA 检查 `Native.VERSION` 必须和 native lib 版本严格一致。Alpine 上游 `java-jna-native` 锁 5.15.0-r0；为复用 Alpine 二进制（最稳）→ 把 trino-libs 里的 `jna-5.13.0.jar` 换成 `jna-5.15.0.jar`。host 验证 OSHI 6.4.8 + trino 435 用 jna 5.15 drop-in 兼容 `ok=3 fail=0`。

## 4. 我们自建的小工具

| 文件 | 描述 | sha256 |
|---|---|---|
| `trino-machineinfo-shim.jar` | 745B jar，含 `io/trino/util/MachineInfo.class`，方法 `getAvailablePhysicalProcessorCount()` 返回 `Runtime.getRuntime().availableProcessors()`。源码可由 [README.md](./README.md) 注释重建（10 行 Java）。**用途**：classpath 前置以遮蔽 trino-main 的 MachineInfo，避开 OSHI → JNA native 路径。**等 starry musl JNA dlopen 挂死修了，删此 jar + 切回 jna-5.15 路径**。 | `e5a9c9e9...` |
| `prep-trino-rootfs.sh` | 把 `package/target/trino-demo.jar` + `package/target/trino-libs/*` + `trino-machineinfo-shim.jar` 注入 `rootfs-<arch>-java.img` → `rootfs-<arch>-trino.img`。WSL2 安全：`debugfs -w` 不挂载、不 sync。 | `2d743714...` |
| `qemu-<arch>.toml` × 4 | StarryOS qemu harness。`-m 8192M` `-Xmx512m` `-Xint`，classpath 含 shim jar，无 `-Djna.boot.library.path`（shim 路径）。success_regex `^TRINO_OK=1`。 | （随 README 修订） |
| `build-<target>.toml` × 4 | cargo xtask 配置（target = `<arch>-unknown-none[-soft float]` / `riscv64gc-unknown-none-elf`） | （定型） |

## 5. 复现下载（host 一次性）

```bash
# Trino 上游官方包（参考保留）
curl -sfLo trino-server-476.tar.gz   https://repo1.maven.org/maven2/io/trino/trino-server/476/trino-server-476.tar.gz
curl -sfLo trino-cli-481.jar          https://repo1.maven.org/maven2/io/trino/trino-cli/481/trino-cli-481-executable.jar
# JNA 5.15.0 jar
curl -sfLo jna-5.15.0.jar             https://repo1.maven.org/maven2/net/java/dev/jna/jna/5.15.0/jna-5.15.0.jar
# Alpine musl libjnidispatch（3 arch）— apk = gzipped tar，解开取 usr/lib/libjnidispatch.so
for A in x86_64 aarch64 loongarch64; do
  curl -sfLo java-jna-native-$A.apk \
    "https://dl-cdn.alpinelinux.org/alpine/edge/community/$A/java-jna-native-5.15.0-r0.apk"
  mkdir -p ext-$A && tar -xzf java-jna-native-$A.apk -C ext-$A
  cp ext-$A/usr/lib/libjnidispatch.so libjnidispatch-$A-musl.so
done
# Maven 拉 trino 435 deps（需联网）
mvn -f pom.xml package -DskipTests
```

sha256 全部依据本目录文件计算；任何 mirror/proxy 命中后请重新核对。
