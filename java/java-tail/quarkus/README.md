# java-tail/quarkus/ — Quarkus CLI 3.35.4（supersonic-subatomic-Java）

**marker `QUARKUS_OK`** · 非 server（CLI 离线 surface）· `timeout=1800` · #764 `quarkus` !

## 内容

```
quarkus/
├── qemu-{x86_64,aarch64,riscv64,loongarch64}.toml   harness（3 个离线断言 + gate）
├── prep-quarkus-rootfs.sh                            构建 rootfs-<arch>-quarkus.img
└── package/quarkus-cli-3.35.4.tar.gz                 官方 CLI 发行包（20 MB）
```

**payload**：官方 `quarkus-cli-3.35.4.tar.gz` 解到 guest `/opt/quarkus-cli-3.35.4`。`bin/quarkus` 是 `#!/usr/bin/env sh` 包装，execs `$JAVA_HOME/bin/java io.quarkus.cli.Main`（从 `lib/quarkus-cli-3.35.4-runner.jar`）。来源 + sha256（`4ce2ed59...`，与官方 `checksums_sha256.txt` 一致）见 `../../bigapps/SOURCES.md`。

## 断言（gate：`PASS==TOTAL && TOTAL==3`，全离线）

1. `quarkus --version` → 精确打印 `3.35.4`（in-gate 版本断言，匹配 `^3\.35\.4$`）
2. `quarkus --help` → picocli usage banner（`Usage: quarkus`）+ 真子命令 `create`（证明完整 CLI 运行时 + Quarkus bootstrap 启动）
3. `quarkus version`（子命令）→ 再次 `3.35.4`（第二条独立路径）

## 诚实缺口（不在 gate）

`quarkus create`（项目脚手架）和 `quarkus dev`/`build` 会从 Maven Central + `registry.quarkus.io` 拉平台 BOM + 扩展注册表 —— 离线 guest 不可达。要在 starry 上跑完整 Quarkus **app**，走 `../dod-frameworks/`（若有预构建 RESTEasy uber-jar 的进程内路径）。本用例专测 **CLI 二进制**的自包含离线面。

## 怎么手动在 qemu v10 里跑

```
cd <tgoskits>
bash <本仓库>/java/java-tail/quarkus/prep-quarkus-rootfs.sh x86_64    # 产出 rootfs-x86_64-quarkus.img
cargo xtask starry test qemu --arch x86_64 -g stress -c quarkus-0    # 期望 QUARKUS_OK=1
```

进 guest shell 手动验：

```sh
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk
printf '/lib\n/usr/lib\n%s/lib\n%s/lib/server\n' "$JAVA_HOME" "$JAVA_HOME" > /etc/ld-musl-x86_64.path
export JAVA_TOOL_OPTIONS="-Xint -Xmx256m -Xms32m"
QK=/opt/quarkus-cli-3.35.4/bin/quarkus
sh "$QK" --version       # 期望仅 3.35.4
sh "$QK" --help          # 期望 "Usage: quarkus ..." + create 子命令
sh "$QK" version         # 期望 3.35.4
```
