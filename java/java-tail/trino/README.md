# Trino 435 (StarryOS — java/java-tail/trino)

Trino 是分布式 SQL 查询引擎。这个用例在 StarryOS 上跑 **`io.trino.testing.LocalQueryRunner`**：和真 Trino 集群同一份引擎（parser → analyzer → planner → optimizer → executor）+ 内置 TPCH connector，但去掉 HTTP coordinator/worker 集群，全部嵌入单 JVM。3 条查询：`SELECT 1`、`SELECT count(*) FROM region`、`SELECT count(*) FROM nation WHERE regionkey=1`。

成功标志：`TRINO_RESULT ok=3 fail=0` + `TRINO_DONE` → `TRINO_OK=1`。

## 为什么选 Trino 435（不是 server-476）

Trino **436 起要求 Java 22+**；**435** 是最后一个 JDK17 兼容版本。StarryOS 上的 musl OpenJDK17 base image（`rootfs-<arch>-java.img`，见 [../../openjdk17/](../../openjdk17/)）可直接复用。完整 `trino-server-476` coordinator 在 -Xint 单核 emulated arch 上不现实（多 GB 堆 + Discovery HTTP 集群），但用 `LocalQueryRunner` 跑同一引擎是真实的。`package/trino-server-476.tar.gz` 和 `package/trino-cli-481-executable.jar` 作为完整服务器版本归档保留。

## OSHI/JNA 路径说明（影响交付选项）

`io.trino.util.MachineInfo` → OSHI → JNA 加载 `libjnidispatch.so` 用于探测物理 CPU 数（用于 `TaskManagerConfig` 的 writer 线程池 sizing 启发式）。两条路径：

* **完全忠实路径**：用 Alpine `java-jna-native-5.15.0-r0` 的 musl-built `libjnidispatch.so` 替换 trino-435 自带的 glibc-built 版本，同时把 `jna-5.13.0.jar` 替换为 `jna-5.15.0.jar`（host 验证 OSHI 6.4.8 + trino 435 + jna 5.15 drop-in 兼容 `ok=3 fail=0`）。
  - 真 musl Linux 4 架构（qemu-system + Alpine boot）全部运行通过 `ok=3 fail=0 TRINO_DONE`。
  - **StarryOS 上挂死 32+ min**（musl JNA dlopen/JNI 路径在 starry 触发某个 syscall hang，**疑似 `mprotect PROT_NONE` round-trip 或 futex/clear_child_tid wake**；tracking alongside 上游 #206）。
  - 资源已就位：[`jna/`](./jna/) 下有 `jna-5.15.0.jar` + 3 arch（x86_64/aarch64/loongarch64）的 musl `libjnidispatch.so`。**riscv64 Alpine 没打 musl 包**，需自行从 JNA 源码编译。
* **当前交付路径（MachineInfo shim）**：[`trino-machineinfo-shim.jar`](./trino-machineinfo-shim.jar)（745B）是 `io.trino.util.MachineInfo` 的最小替身（同包名同方法），返回 `Runtime.getRuntime().availableProcessors()` ——**正是 trino 自己 MachineInfo 的 `Math.min` 上界**。Host 验证：0 个 oshi/jna 类加载，`ok=3 fail=0`。SQL 引擎完全不动。所有 4 架构 starry 上都用这条路径运行通过：x86_64 / aarch64 / riscv64 / loongarch64 全 `TRINO_OK=1 SUCCESS PATTERN MATCHED`。

> 当 starry musl JNA 挂死修复后，把 toml classpath 从 `trino-machineinfo-shim.jar:...trino-libs/*` 切回 `... trino-libs/*` + 加 `-Djna.boot.library.path=/root/trino/jna -Djna.nounpack=true`，并把 `prep-trino-rootfs.sh` 的 jna-5.13→5.15 swap 启用即可。

## 怎么手动在 qemu-10 StarryOS 4 架构运行

### 0. 前置（一次性）

1. **QEMU ≥ 10**（硬性）+ tgoskits 源码（参考顶层 [../../../README.md](../../../README.md) 环境段）。
2. **构建 trino-435 maven artifacts**（需要联网，~100 MB 下载到 `~/.m2/`）：
   ```
   cd package
   mvn -f pom.xml package -DskipTests
   ```
   产物：`target/trino-demo.jar`（demo class）+ `target/trino-libs/`（194 个运行时 jar，含 `trino-main-435` test-jar 内的 `LocalQueryRunner` + `trino-tpch-435` + 依赖）。Trino 435 来源见 [SOURCES.md](./SOURCES.md)。
3. **构建 musl OpenJDK17 4-arch base rootfs**（参考 [../../openjdk17/README.md](../../openjdk17/README.md) → 得 `rootfs-<arch>-java.img`）。

### 1. 构建 trino rootfs（每架构）

```bash
# 在本目录:
bash prep-trino-rootfs.sh x86_64
bash prep-trino-rootfs.sh aarch64
bash prep-trino-rootfs.sh riscv64
bash prep-trino-rootfs.sh loongarch64
```

每个调用：复制 `rootfs-<arch>-java.img` → `rootfs-<arch>-trino.img`，通过 `debugfs -w`（**不挂载、不 sync**，避开 WSL2 ext4 sync 死锁）把 `target/trino-demo.jar` + `target/trino-libs/*` + 本目录的 `trino-machineinfo-shim.jar` 注入 `/root/trino/`。

### 2. 把 4 个 `qemu-<arch>.toml` + 4 个 `build-<target>.toml` 拷到 tgoskits 测试目录

```bash
mkdir -p <tgoskits>/test-suit/starryos/stress/trino-0/trino-0
cp qemu-*.toml <tgoskits>/test-suit/starryos/stress/trino-0/trino-0/
cp build-*.toml <tgoskits>/test-suit/starryos/stress/trino-0/
```

### 3. 跑（每架构，**qemu-10 必须**，需 `cargo xtask` 在 tgoskits 根）

```bash
cd <tgoskits>
source .starry-env.sh   # 设置 PATH=/opt/qemu-10.2.1/bin:$PATH 等
cargo xtask starry test qemu --arch x86_64      -g stress -c trino-0
cargo xtask starry test qemu --arch aarch64     -g stress -c trino-0
cargo xtask starry test qemu --arch riscv64     -g stress -c trino-0
cargo xtask starry test qemu --arch loongarch64 -g stress -c trino-0
```

每架构期望输出末尾：
```
TRINO_RESULT ok=3 fail=0
TRINO_DONE
TRINO_OK=1
=== SUCCESS PATTERN MATCHED: (?m)^TRINO_OK=1 ===
  PASS trino-0 (<time>s)
result: 1/1 case(s) passed
```

wall-clock（参考）：
- x86_64：~3-5 分钟
- aarch64：~20-25 分钟（emulated cortex-a72 + -Xint）
- riscv64：~25-30 分钟
- loongarch64：~20 分钟

guest 内存 **8 GB**（trino-libs 是 134 MB jar，加 JVM heap 512m + metaspace 256m + 文件页缓存 + starry kernel overhead，4 GB 上 aarch 出现内核 page allocator OOM）。

## 文件清单

| 路径 | 说明 |
|---|---|
| `qemu-{x86_64,aarch64,riscv64,loongarch64}.toml` | StarryOS qemu 配置（含 shim classpath、内存 8G、`-Xmx512m`） |
| `build-{x86_64-unknown-none,aarch64-unknown-none-softfloat,riscv64gc-unknown-none-elf,loongarch64-unknown-none-softfloat}.toml` | 对应 cargo target 配置 |
| `prep-trino-rootfs.sh <arch>` | 用 `debugfs -w` 把 trino artifacts 注入 `rootfs-<arch>-java.img` → `rootfs-<arch>-trino.img` |
| `trino-machineinfo-shim.jar` | 745B 替身 `io.trino.util.MachineInfo`，避开 OSHI/JNA native（详见上节） |
| `jna/jna-5.15.0.jar` | Alpine-兼容版 JNA，留作未来切换忠实路径 |
| `jna/libjnidispatch-{x86_64,aarch64,loongarch64}-musl.so` | Alpine 5.15 musl-built native lib（riscv64 Alpine 未打包，需自编） |
| `package/TrinoDemo.java` + `package/pom.xml` | 测试驱动源码 + maven 构建配置（trino 435 LocalQueryRunner + TPCH） |
| `package/trino-server-476.tar.gz` | Trino 完整服务器版本归档（启发：JDK24+ 需 server-476；当前用 435 LocalQueryRunner） |
| `package/trino-cli-481-executable.jar` | Trino CLI 18 MB executable jar（Java 11 兼容；可在 musl JDK17 跑 `--version`） |

所有文件的**来源、版本、sha256、获取方式**见 [SOURCES.md](./SOURCES.md)。
