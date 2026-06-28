# java/java-tail/ — 收官中的「java 尾巴」用例：ktor / quarkus / wildfly / sdkman / **trino**

#764 Java 子课题的最后收官项的 StarryOS 压力用例。全部 4 架构 starry 通过（`SUCCESS PATTERN MATCHED`，#764 jdk17+ 整段 `[x]`，包括 wildfly + trino）。trino 用 MachineInfo shim 路径绕开 starry 一个未修的 musl JNA dlopen 挂死（详见 `trino/README.md`），SQL 引擎完全不动。整体设计笔记见 `CASE-NOTES-javatail.md`（每个 app 的可行性 / JDK / 断言 / 已知缺口）。

## 共同设计

* **基座** = `rootfs-<arch>-java.img`（已含 4 架构 musl OpenJDK17 + maven/gradle/kotlin + dod jars）。每个用例是薄覆盖，prep 脚本把基座复制成 `rootfs-<arch>-<app>.img` 后**只注入该 app 多出来的 payload**，用 `debugfs -w` 写入未挂载的 ext4（**不 mount、不 sync**，避开 WSL2 D-state 死锁）。
* **`-Xint`** 强制在每个 JVM（JIT 仍不稳定，rcore-os/tgoskits#206）。
* **防假阳性**：成功 token `<APP>_OK=1` 只出现在最后 `printf` 行 + `success_regex`；内部记账用 `PASS/TOTAL/SRV_READY`。`V=1` 当且仅当 `PASS==TOTAL && TOTAL==期望数`（server 用例还要 `SRV_READY==1`）。版本断言 in-gate（版本不对即失败）。断言查真实输出（状态行+body / 版本串 / banner），不信 exit-0。

## 用例一览

| 用例 | app | marker | server? | timeout | #764 项 |
|------|-----|--------|---------|---------|---------|
| `ktor/` | Ktor 2.3.x（Kotlin/Netty async web） | `KTOR_OK` | 是（:18082） | 3000s | ktor ! |
| `quarkus/` | Quarkus CLI 3.35.4 | `QUARKUS_OK` | 否（CLI 离线） | 1800s | quarkus ! |
| `wildfly/` | WildFly 40.0.0.Final（完整 EE app server） | `WILDFLY_OK` | 是（:8080，最重） | 4500s | wildfly ! |
| `sdkman/` | SDKMAN! 5.23.0（JVM SDK 管理器） | `SDKMAN_OK` | 否（bash 框架离线） | 1800s | sdkman ! |
| `trino/` | Trino 435 LocalQueryRunner + TPCH（分布式 SQL 引擎嵌入式跑 3 查询） | `TRINO_OK` | 否（in-process） | 4500s | **trino √** |

每个子目录含 `qemu-{x86_64,aarch64,riscv64,loongarch64}.toml` + `prep-<app>-rootfs.sh`（+ `package/` 里的上游包，ktor 复用 `../dod-frameworks/jars/ktor-demo.jar`）。

## 镜像名 / 资源映射

| 用例 | 产出镜像 | resize | payload |
|------|----------|--------|---------|
| ktor | `rootfs-<arch>-ktor.img` | 无 | `../dod-frameworks/jars/ktor-demo.jar` → `/root/ktor/` |
| quarkus | `rootfs-<arch>-quarkus.img` | 无 | `quarkus/package/quarkus-cli-3.35.4.tar.gz` → `/opt/quarkus-cli-3.35.4` |
| wildfly | `rootfs-<arch>-wildfly.img` | 5 G | `wildfly/package/wildfly-40.0.0.Final.tar.gz` → `/opt/wildfly-40.0.0.Final` |
| sdkman | `rootfs-<arch>-sdkman.img` | 4 G | `sdkman/package/{sdkman-5.23.0-*.zip, apks/<arch>/*}` → `/root/.sdkman` + `/bin/bash` 闭包 |

> **prep 脚本路径注意**：各 prep 脚本通过 `TGOSKITS_ROOT` 环境变量定位 tgoskits 工作区，payload 则与脚本同目录随附（ktor 复用 `java/dod-frameworks/jars/ktor-demo.jar`，sdkman 用 `java/java-tail/sdkman/package/`，quarkus/wildfly 用各自 `package/`）。复用本仓库时设置 `export TGOSKITS_ROOT=<本机 tgoskits 路径>` 即可，无需改脚本。

## 集成 + 跑（QEMU v10）

1. 在 tgoskits 建 `test-suit/starryos/stress/{ktor,quarkus,wildfly,sdkman}-0/`（镜像现有 `ktor-0` 布局：用例根 4 个 `build-<target>.toml` + 内层 `<case>-0/qemu-<arch>.toml`×4）。`build-*.toml` 从现有 `ktor-0`/`openjdk17-0` 原样复制（app 无关：`features=["qemu"]`、`plat_dyn=false`、per-arch target triple）。
2. 把各子目录的 `qemu-<arch>.toml` 复制进对应内层目录。
3. 用各 `prep-<app>-rootfs.sh` 构建 4 架构 `rootfs-<arch>-<app>.img`。
4. 跑（建议代价升序）：quarkus + sdkman（快 CLI）→ ktor（server，中）→ wildfly（启动最重，先盯 loongarch/riscv64 超时）：

   ```
   cargo xtask starry test qemu --arch x86_64 -g stress -c quarkus-0   # 期望 QUARKUS_OK=1
   # 其余 app / arch 同理
   ```
5. 打勾前防假阳性复核：确认 `<APP>_OK=1` 来自真正的 `PASS==TOTAL`（harness 在 gate 前会 echo `<APP>_RESULT pass=N total=M`，核对 N==M==期望，不只看 success_regex 命中）。

## 已知限制（据实记录）

* **quarkus / sdkman** 的离线 gate **故意排除**网络功能（`quarkus create/dev`、`sdk install/list/update`）——这些要 Maven Central / `registry.quarkus.io` / `api.sdkman.io`，离线 guest 不可达；已在 CASE-NOTES 标为 out-of-scope，不伪造。
* **wildfly** 在 -Xint + 模拟 riscv64/loongarch 上的启动耗时是主要排期风险；超 `timeout=4500` 会带启动日志尾巴据实报告失败（不发 `WILDFLY_OK=1`）。调 timeout 或 toml `-m` 可为启动提供更多 wall-clock 余量。
