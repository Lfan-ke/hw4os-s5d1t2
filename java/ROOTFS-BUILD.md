# Java rootfs 构建方法（debugfs -w 注入法）

所有 Java 用例的 rootfs 镜像都用同一种方法构建：**在未挂载的 ext4 镜像上用 `debugfs -w` 直接写入文件**，从不 `mount`、从不 `sync`。本文档说明该方法 + 各 `prep-*.sh` 脚本的作用，供 tgoskits 维护者复现。

> **为什么不 mount/sync**：本项目的构建环境是 WSL2，一次裸的全局 `sync` 会把宿主拖入 D-state 死锁。`debugfs -w` 直接操作 ext4 镜像的元数据/数据块，全程不挂载、不触发 sync，因此安全。即使你的环境没有这个限制，debugfs 注入法仍然更快、更可控（只动目标镜像、可幂等重跑）。
>
> QEMU **v10+** 是跑这些镜像的硬性前提（见仓库根 README）。

---

## 两类基座

| 基座 | 怎么来 | 谁用 |
|------|--------|------|
| `rootfs-<arch>-java.img`（约 3 GB） | 由 4 架构 musl OpenJDK17（`openjdk17/`）+ maven/gradle/kotlin（`toolchain/`）+ dod jars（`dod-frameworks/`）构建而成 | `openjdk17-0` 用例本身；`java-tail/` 的 ktor/quarkus/wildfly/sdkman 都在它上面做增量覆盖 |
| `rootfs-<arch>-alpine.img` | `cargo xtask starry rootfs --arch <arch>` 产出 | `jdk-multi/` 用例从它新建（并排装 4 个 JDK） |

`java-tail/` 的 prep 脚本把 `rootfs-<arch>-java.img` 复制为 `rootfs-<arch>-<app>.img`，（按需 resize）再只注入该 app 的 payload。`jdk-multi/` 从 alpine 基座新建 `rootfs-<arch>-jdk-multi.img`。

---

## prep-*.sh 通用流程（debugfs 注入）

每个 `prep-<app>-rootfs.sh <arch>` 大致做：

1. **复制基座** → `rootfs-<arch>-<app>.img`（落到 tgoskits `tmp/axbuild/rootfs/`）。
2. **按需 resize**（`resize2fs`，如 wildfly 5G / sdkman 4G / jdk-multi 6G）。
3. **准备 payload 暂存树**（`/tmp/<app>-stage-<arch>`）：解包 tar/zip、布置目录结构、预置离线状态文件。
4. **生成 debugfs 命令脚本**（`/tmp/<app>-debugfs-<arch>.cmds`）：深度优先 `mkdir` / `rm`+`write`（文件内容）/ `symlink`（符号链接），幂等（先 `rm` 再 `write`，可重跑）。
5. **回放到未挂载镜像**：`debugfs -w -f <cmds> <img>`。
6. **校验**：`e2fsck` + `debugfs -R "stat <path>"` 确认关键文件/符号链接落地。

> 注入逻辑曾在一个 32 MB 一次性 ext4 镜像上验证过（目录、文件内容、符号链接目标都正确往返）。debugfs 命令/日志落 `/tmp/<app>-debugfs-<arch>.{cmds,log}`。

---

## 各脚本注入的 payload

| 脚本 | 产出镜像 | resize | 注入内容 |
|------|----------|--------|----------|
| `java-tail/ktor/prep-ktor-rootfs.sh` | `rootfs-<arch>-ktor.img` | 无 | `ktor-demo.jar` → `/root/ktor/` |
| `java-tail/quarkus/prep-quarkus-rootfs.sh` | `rootfs-<arch>-quarkus.img` | 无 | `quarkus-cli-3.35.4` → `/opt/` |
| `java-tail/wildfly/prep-wildfly-rootfs.sh` | `rootfs-<arch>-wildfly.img` | 5 G | `wildfly-40.0.0.Final` → `/opt/` |
| `java-tail/sdkman/prep-sdkman-rootfs.sh` | `rootfs-<arch>-sdkman.img` | 4 G | `/bin/bash` + 闭包 + `/root/.sdkman/...` + 预置离线状态 |
| `jdk-multi/case/prep-jdk-multi-rootfs.sh` | `rootfs-<arch>-jdk-multi.img` | 6 G | `/opt/jdk{17,21,23,25}` + `jdk-current` 符号链接 + `/root/jdkm/*.java` + `~/.sdkman` candidates + gcompat(riscv/loong) |

> **payload 路径（已自包含）**：所有 prep 脚本已改为离线自包含——`ROOT` 由 `TGOSKITS_ROOT` 环境变量提供，软件包随各 app 目录 co-located（Git LFS）并由脚本相对路径定位，**无需任何开发机本机绝对路径**。共享大文件（JDK17/21/23/25 闭包、sdkman、gcompat）按相对路径引用一份不重复（如 jdk-multi 引用 `../../openjdk17/packages` 与 `../../java-tail/sdkman/package`）。维护者只需 `export TGOSKITS_ROOT=<本机 tgoskits 目录>` 再 `bash <app>/prep-*.sh <arch>`。

---

## 完整一次构建 + 跑（QEMU v10，以 wildfly x86_64 为例）

```sh
cd <tgoskits>
cargo xtask starry rootfs --arch x86_64                                 # 确保基座存在（一次）
bash <本仓库>/java/java-tail/wildfly/prep-wildfly-rootfs.sh x86_64       # debugfs 注入，产出 rootfs-x86_64-wildfly.img
cargo xtask starry test qemu --arch x86_64 -g stress -c wildfly-0       # 期望 WILDFLY_OK=1
```

4 架构把 `x86_64` 换成 `aarch64` / `riscv64` / `loongarch64` 重复即可（loongarch64 RAM>1G 必须 QEMU v10）。
