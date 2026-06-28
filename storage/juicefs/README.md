# juicefs (juice) — 分布式 POSIX 文件系统 (#764 sub-deps/juice)

**JuiceFS**(Go,POSIX 兼容的云原生分布式文件系统:可插拔元数据引擎 + 对象存储)在 StarryOS 四架构单核 qemu-10 上,其 **sqlite 元数据核心全绿 4/4**。

## DoD 结论(qemu-10 单核 starry,2026-06-05)

| arch | 结果 | 备注 |
|:--:|:--:|:--:|
| x86_64 | √ `JUICEFS_OK=1` (3/3) | 官方 amd64 release |
| aarch64 | √ `JUICEFS_OK=1` (3/3) | `-cpu cortex-a72`;官方 arm64;依赖内核 EL0 `MRS ID_AA64*` 模拟(fork PR #1128) |
| riscv64 | √ `JUICEFS_OK=1` (3/3) | `-cpu rv64`;官方 riscv64 release |
| loongarch64 | √ `JUICEFS_OK=1` (3/3) | `-cpu la464 -machine virt`;官方 loong64 release |

判据权威:xtask `rc=0` + 日志 `SUCCESS PATTERN MATCHED` + `1/1 case(s) passed` + 锚定 `^JUICEFS_OK=1`,四架构各复核一致。

## 测试内容(juicefs sqlite 元数据核心)

门控 `JUICEFS_OK = VER_OK && FMT_OK && RT_OK`(3 项,sqlite 核心):
1. **VER_OK(红线)**:`juicefs version` 精确 `1.3.1`。
2. **FMT_OK**:`juicefs format --storage file --bucket <dir> sqlite3://<meta> <vol>` 成功,打印 `Volume is formatted as {...}` 含我们的卷名 + 合法 UUID。
3. **RT_OK**:`juicefs status sqlite3://<meta>` 读回同一卷 —— UUID + Name 与 format 一致(元数据跨进程 reopen 往返)。

验证内核面:Go runtime(goroutine 调度 + netpoller/epoll)、**sqlite3 嵌入式元数据引擎的 mmap/WAL 跨进程 reopen**(format 进程写、status 进程读回)、file 对象存储 I/O。

### 根治内核修(本案坐实)

riscv64 上 sqlite reopen 曾因 **axfs-ng `CachedFile::set_len` 收缩分支未清零部分末页尾部** → sqlite WAL/shm 跨进程 reopen 读到脏 tail → "database is not formatted" 而崩。修复(fork PR #1124,`os/arceos/modules/axfs-ng/src/highlevel/file.rs`):截短时把部分末页 `[tail..]` 清零并标脏,与 Linux `truncate` 语义一致。本案 **FMT_OK + RT_OK 四架构全绿即坐实该修在 4 arch 有效**。aarch64 另依赖 EL0 MRS 模拟(#1128)。

## badger 次要引擎(非门控,据实说明)

测试脚本另跑一个**可选的** badger 嵌入式 KV 元数据引擎做对照(`[badger(secondary,non-gating)=…]`)。其在 **riscv64/loongarch64 失败**(`mremap size mismatch`),根因经隔离实验(test-mremap DIAG)证实 **不是 StarryOS mremap 内核 bug**(精确复刻 badger 的 `mmap 128MiB → ftruncate → mremap` 序列在 riscv64 直接跑完全正常),而是 badger 自身在该 arch 的内部问题。badger 是 juicefs 多种可选引擎之一,**非其核心**,故**不纳入 4/4 门控**,仅如实报告。juicefs 生产常用 redis/sql/tikv;sqlite 即其主力嵌入式引擎,已 4/4。

## 来源(provenance)

- **juicefs 1.3.1**(`github.com/juicedata/juicefs` v1.3.1 release tarball `juicefs-1.3.1-linux-<goarch>.tar.gz`):x86_64(amd64)/aarch64(arm64)/riscv64/loongarch64(loong64) **官方预编译**(juicefs 官方发行包含这四个 linux arch)。CGO-free 静态 Go 二进制,无 musl/ld 接线需求。
- 详见 `<本机下载缓存目录>/golang-bins/juicefs/`。

## 构建运行

```bash
bash case/prep-juicefs-rootfs.sh <arch>     # debugfs 注入 juicefs 二进制
source <仓库根>/.starry-env.sh              # 使用 qemu-10
cargo xtask starry test qemu --arch <arch> -g stress -c juicefs-0
# 成功判据: rc=0 + SUCCESS PATTERN MATCHED + ^JUICEFS_OK=1
```

注:`timeout = 3600/4000`(riscv64/loongarch64 TCG 下 Go + sqlite format/reopen 较重,非真 hang)。
