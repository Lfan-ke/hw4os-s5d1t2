# etcd — 分布式 KV / 一致性存储 (#764 etcd)

**etcd v3.6.11**(Go,Raft 一致性 + bbolt MVCC 存储 + gRPC over loopback)在 StarryOS 四架构(x86_64/aarch64/riscv64/loongarch64)单核 qemu-10 上运行:单节点 etcd 在 loopback 启动,`put`/`get` 经 client RPC 字节级往返,四架构全部通过。

## DoD 结论(qemu-10 单核 starry,2026-06-06)

| arch | 结果 | 备注 |
|------|------|------|
| x86_64 | √ `ETCD_OK=1` | 官方 amd64 release;依赖内核 mmap-EOF 修(见下) |
| aarch64 | √ `ETCD_OK=1` | `-cpu cortex-a72`;官方 arm64 release |
| riscv64 | √ `ETCD_OK=1` | `-cpu rv64`;官方 riscv64 release;`ETCD_UNSUPPORTED_ARCH=riscv64` |
| loongarch64 | √ `ETCD_OK=1` | `-machine virt -cpu la464`;官方 loong64 release;`ETCD_UNSUPPORTED_ARCH=loong64` |

判据权威:xtask `rc=0` + 日志 `SUCCESS PATTERN MATCHED` + `1/1 case(s) passed` + 锚定 `^ETCD_OK=1`,四架构各复核一致。

## 测试内容(etcd 单节点,门控 `ETCD_OK = VER_OK && KV_OK`)

1. **VER**:`etcd --version` 精确 `etcd Version: 3.6.11`(版本红线;错版本 = 无效测试)。
2. **AGENT**:`etcd --name s1 --data-dir /root/etcd.d --listen-client-urls http://127.0.0.1:2379 ... --force-new-cluster` 后台起,轮询 `etcdctl endpoint health` 直到 `is healthy`(Raft/bbolt 在 loopback 起来)。
3. **KV**:`etcdctl put starry/key ETCD_VALUE_42` 后 `etcdctl get` 经 client RPC 往返,值须字节级一致。

验证内核面:Go runtime(M:N 调度 + GC + futex parking)、**loopback AF_INET TCP**(client RPC 2379 / peer 2380)、getrandom 熵、**bbolt 大 mmap**(见根因修)、getrlimit/futex。

数据目录用 `/root/etcd.d`(ext4,页缓存有界 LRU),**不用 `/tmp`(tmpfs 无界)**:bbolt 用很大的 `InitialMmapSize` mmap db,ext4 后端使常驻集有界。

## 根治内核修

**现象**:修复前 etcd 在 x86_64 启动阶段 OOM(物理内存耗尽)。

**根因**:starry 的 `FileBackend::populate` 对文件 mmap 范围内**每一页都急切分配物理帧,包括完全落在文件 EOF 之外的稀疏页**。etcd 的 bbolt 用约 **10 GB 的 `InitialMmapSize`**(≈260 万个 4K 页)mmap 一个仅十几 KB(4 页)的 db 文件;当内核为某次 syscall fault-in 这段用户缓冲时,一次覆盖整区的 populate 为全部 260 万页逐页分配帧,跑到约 40 万页耗尽 RAM。

**修复(向 Linux 靠拢,非 workaround)**:共享文件映射在最后一个文件页之后访问为 SIGBUS,EOF 之外的页不预先填充。
- `axfs-ng CachedFile::file_len()` 暴露文件长度;
- `FileBackend::populate` 循环前算 `eof_page = file_len.div_ceil(4096)`,跳过 `pn >= eof_page` 的页(保持 unmapped,真实访问 → 缺页 → SIGBUS)。

修复后该 10 GB 稀疏映射只为真实文件页分配帧(本例 4 页),内存恒定有界。完全在 EOF 内的文件映射(如 JVM jimage)无回归。

→ 上游 PR **rcore-os/tgoskits#1164**;此修复同时是其它大 mmap 数据库/server 类应用(minio/neo4j/mongo/juicefs-badger 等)在 starry 上运行的前置条件之一。

## 在 qemu-10 四架构 starry 复现

维护者侧步骤(qemu-10 单核;`source <tgoskits 根>/.starry-env.sh` 启用 qemu-10.2.1):

```bash
# 1) 取官方四架构 release tarball(见 SOURCES.md)放到
#    download/golang-bins/etcd/<arch>/etcd-v3.6.11-linux-<goarch>.tar.gz
# 2) 组装 rootfs(debugfs 直写未挂载 ext4;不 mount/不 sync,WSL2 安全)
for a in x86_64 aarch64 riscv64 loongarch64; do
  bash prep-etcd-rootfs.sh $a
done
# 3) 把 case/ 下的 build/qemu toml 放进 tgoskits/test-suit/starryos/stress/etcd-0/
#    跑(内核须含 mmap-EOF 修 rcore-os#1164):
source <tgoskits 根>/.starry-env.sh
for a in x86_64 aarch64 riscv64 loongarch64; do
  cargo xtask starry test qemu --arch $a -g stress -c etcd-0
done
# 通过判据:rc=0 + "SUCCESS PATTERN MATCHED: (?m)^ETCD_OK=1" + "1/1 case(s) passed"
```

## 文件

- `case/build-<arch>.toml` — 四架构 rootfs 构建配置
- `case/etcd-0/qemu-<arch>.toml` — 四架构 qemu 运行 + 测试脚本(含 `-cpu`/`-machine`、`ETCD_UNSUPPORTED_ARCH`、数据目录 ext4)
- `prep-etcd-rootfs.sh` — debugfs 直写 rootfs 组装器(`<arch>` 参数)
- `SOURCES.md` — 四架构二进制来源 + ETCD_UNSUPPORTED_ARCH 说明
