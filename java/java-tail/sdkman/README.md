# java-tail/sdkman/ — SDKMAN! 5.23.0（JVM 的 SDK 管理器）

**marker `SDKMAN_OK`** · 非 server（bash 框架离线 surface）· `timeout=1800` · #764 `sdkman` !

## 内容

```
sdkman/
├── qemu-{x86_64,aarch64,riscv64,loongarch64}.toml   harness（4 个离线断言 + gate）
├── prep-sdkman-rootfs.sh                             构建 rootfs-<arch>-sdkman.img（resize 4G）
└── package/
    ├── sdkman-5.23.0-linuxx64.zip / sdkman-5.23.0-exotic.zip   SDKMAN 框架
    ├── apks/<arch>/                                  22-apk bash 闭包（bash/curl/zip/unzip/readline/ncurses/...）
    └── candidates-all.csv                            离线 candidate 缓存（82 个候选）
```

**这个用例的区别性依赖**：SDKMAN 是纯 bash —— `sdk` 是从 `/root/.sdkman/bin/sdkman-init.sh` source 出来的 shell **函数**（再 source 每个 `src/sdkman-*.sh` 模块），用到 bash 数组 / `[[ ]]`，**busybox ash 跑不了**。所以 prep 注入两样：(a) 真 `/bin/bash` + 闭包（从 `package/apks/<arch>/`，4 架构齐）；(b) SDKMAN 框架到 `/root/.sdkman/{bin,src,libexec,etc,var,...}` + 预置离线状态（`var/version=5.23.0`、`var/platform`、`var/candidates`、`etc/config` 设 `sdkman_offline_mode=true` 等）。

> prep 脚本通过 `TGOSKITS_ROOT` 环境变量定位 tgoskits 工作区，bash 闭包与框架包随脚本同目录的 `package/` 随附。复用本仓库时设置 `export TGOSKITS_ROOT=<本机 tgoskits 路径>` 即可。

## 断言（gate：`PASS==TOTAL && TOTAL==4`，全离线）

`sdk` 是函数，故 harness 用 `/bin/bash -c 'source $SDKMAN_DIR/bin/sdkman-init.sh; sdk <cmd>'` 调用（没有独立 `sdk` 二进制）：

0. `/bin/bash` 能在 starry 跑（`BASH_5...`）—— 本用例的区别性依赖
1. `sdk version` → 精确 `SDKMAN 5.23.0`（in-gate 版本断言，读 `var/version`，匹配 `^SDKMAN 5\.23\.0$`）
2. `sdk help` → `Usage: sdk <command>` banner + 真子命令 `install   or i`（证明每个 `src/*.sh` 都 source 了、函数 dispatcher 工作）
3. candidate 缓存已装载：source 后 `${#SDKMAN_CANDIDATES[@]}` 非空且含 `java`（证明离线 candidate 缓存解析进数组）

## 诚实缺口（不在 gate）

`sdk install/list/update/selfupdate` 都打 `api.sdkman.io` / `broker.sdkman.io` + 候选下载 URL —— 离线 guest 不可达。本用例证明「SDKMAN 运行时在 starry 工作」（bash 框架装载 + dispatch + 读离线状态），不是实时 SDK 安装。

## 怎么手动在 qemu v10 里跑

```
cd <tgoskits>
bash <本仓库>/java/java-tail/sdkman/prep-sdkman-rootfs.sh x86_64    # 产出 rootfs-x86_64-sdkman.img（4G）
cargo xtask starry test qemu --arch x86_64 -g stress -c sdkman-0   # 期望 SDKMAN_OK=1
```

进 guest shell 手动验：

```sh
export SDKMAN_DIR=/root/.sdkman
/bin/bash -c 'echo BASH_$BASH_VERSION'                                   # 期望 BASH_5...
/bin/bash -c 'source $SDKMAN_DIR/bin/sdkman-init.sh; sdk version'        # 期望 SDKMAN 5.23.0
/bin/bash -c 'source $SDKMAN_DIR/bin/sdkman-init.sh; sdk help'           # 期望 Usage: sdk <command> + install or i
/bin/bash -c 'source $SDKMAN_DIR/bin/sdkman-init.sh; echo ${#SDKMAN_CANDIDATES[@]}; case " ${SDKMAN_CANDIDATES[*]} " in *" java "*) echo HAS_JAVA;; esac'
```
