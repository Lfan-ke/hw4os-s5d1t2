# Redis 8.8.0 — StarryOS (java/dod-frameworks/redis)

In-memory KV/datastore, loopback `:6379`. Validates: musl ELF launch + TCP bind + write/read roundtrip + INCR + SHUTDOWN. Marker `REDIS_OK=1`.

**配置 #764 子项** `<!-- - [x] redis -->`（java 子依赖, 完成已 tick）。

## 来源 & 版本

| 文件 | 上游 | 版本 | 下载 URL | sha256 |
|---|---|---|---|---|
| `apks/x86_64/redis.apk` | Alpine edge community | 8.8.0-r0 | <https://dl-cdn.alpinelinux.org/alpine/edge/community/x86_64/redis-8.8.0-r0.apk> | `4836ec82...` |
| `apks/aarch64/redis.apk` | Alpine edge community | 8.8.0-r0 | <https://dl-cdn.alpinelinux.org/alpine/edge/community/aarch64/redis-8.8.0-r0.apk> | `5e703268...` |
| `apks/riscv64/redis.apk` | Alpine edge community | 8.8.0-r0 | <https://dl-cdn.alpinelinux.org/alpine/edge/community/riscv64/redis-8.8.0-r0.apk> | `c1db5ab3...` |
| `apks/loongarch64/redis.apk` | Alpine edge community | 8.8.0-r0 | <https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/redis-8.8.0-r0.apk> | `1e4e889c...` |
| `case/prep-redis-rootfs.sh` | 本项目 | — | — | `da73234d...` |

Refetch一次性脚本:
```bash
for A in x86_64 aarch64 riscv64 loongarch64; do
  mkdir -p apks/$A
  curl -sfLo apks/$A/redis.apk \
    "https://dl-cdn.alpinelinux.org/alpine/edge/community/$A/redis-8.8.0-r0.apk"
done
```

## 怎么在 qemu-10 StarryOS 4 架构运行

### 前置
1. **QEMU ≥ 10** + tgoskits 源码 (参考顶层 [../../../README.md](../../../README.md))。
2. 4 架构 alpine base rootfs: `rootfs-<arch>-alpine.img` (来自 [../../openjdk17/](../../openjdk17/) 同基座, redis 不需要 JDK)。

### 1. 注入 redis-server + redis-cli (本仓库的 prep 脚本)
```bash
cd case
# 脚本已自包含: 从同目录随附的 ../apks/<arch>/redis.apk 取(Git LFS), 仅需 export TGOSKITS_ROOT
export TGOSKITS_ROOT=$HOME/tgoskits
bash prep-redis-rootfs.sh x86_64
bash prep-redis-rootfs.sh aarch64
bash prep-redis-rootfs.sh riscv64
bash prep-redis-rootfs.sh loongarch64
```
脚本用 `debugfs -w` 把 `/usr/bin/redis-server` + `/usr/bin/redis-cli` (chmod 0755) 注入到 `rootfs-<arch>-alpine.img` 拷贝出的 `rootfs-<arch>-redis.img`（不 mount, 不 sync, WSL2 安全）。

### 2. 把 4 toml 拷到 tgoskits
```bash
cp case/qemu-*.toml  <tgoskits>/test-suit/starryos/stress/redis-0/redis-0/
cp case/build-*.toml <tgoskits>/test-suit/starryos/stress/redis-0/
```

### 3. 跑
```bash
cd <tgoskits>
source .starry-env.sh
cargo xtask starry test qemu --arch x86_64      -g stress -c redis-0
cargo xtask starry test qemu --arch aarch64     -g stress -c redis-0
cargo xtask starry test qemu --arch riscv64     -g stress -c redis-0
cargo xtask starry test qemu --arch loongarch64 -g stress -c redis-0
```

期望输出末尾:
```
OK   server ready (loopback :6379)
OK   PING -> PONG
OK   SET/GET roundtrip = bar
OK   INCR counter 41->42
REDIS_RESULT pass=4 total=4
REDIS_OK=1
=== SUCCESS PATTERN MATCHED: (?m)^REDIS_OK=1 ===
  PASS redis-0 (<time>s)
result: 1/1 case(s) passed
```

4 架构 starry 运行 x86_64 PASS 9.97s（其他 arch 为类似的 emulated 时长）。

## 断言（gate `REDIS_OK=1` ⟺ SRV_READY && PASS==TOTAL==4）

1. server ready (TCP bound :6379)
2. PING → PONG (协议握手)
3. SET foo bar; GET foo → "bar" (数据面 roundtrip)
4. SET counter 41; INCR counter → 42 (原子计数)

关停 = `redis-cli SHUTDOWN NOSAVE` + 兜底 `kill $RPID`。

## 文件清单

| 路径 | 说明 |
|---|---|
| `apks/<arch>/redis.apk` | Alpine 8.8.0-r0 redis 二进制包 (4 arch musl) |
| `case/build-<target>.toml` × 4 | cargo xtask 配置 |
| `case/qemu-<arch>.toml` × 4 | StarryOS qemu 配置 (含 server start + 4 断言 + SHUTDOWN) |
| `case/prep-redis-rootfs.sh` | debugfs 注入脚本 (基座 = rootfs-`<arch>`-alpine.img) |
