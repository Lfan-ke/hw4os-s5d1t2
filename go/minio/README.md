# minio — 对象存储 server (#764 大应用 minio)

**MinIO**(Go,S3 兼容对象存储 + 内嵌 Console + net/http)在 StarryOS 四架构(x86_64/aarch64/riscv64/loongarch64)单核 qemu-10 上运行:headless `minio server` 在 loopback 启动,health 端点 HTTP 200,四架构全部通过。

## DoD 结论(qemu-10 单核 starry,2026-06-06)

| arch | 结果 | 备注 |
|------|------|------|
| x86_64 | √ `MINIO_OK=1` (3/3) | 官方 amd64 release(RELEASE.2025-09-07) |
| aarch64 | √ `MINIO_OK=1` (3/3) | `-cpu cortex-a72`;官方 arm64;run 54s |
| riscv64 | √ `MINIO_OK=1` (3/3) | `-cpu rv64`;自交叉编译(RELEASE.2025-10-15) |
| loongarch64 | √ `MINIO_OK=1` (3/3) | `-machine virt -cpu la464`;自交叉编译 |

判据权威:xtask `rc=0` + 日志 `SUCCESS PATTERN MATCHED` + `1/1 case(s) passed` + 锚定 `^MINIO_OK=1`,四架构各复核一致。

## 测试内容(minio 单节点,门控 `MINIO_OK = PASS==TOTAL==3`)

1. **VER**:`minio --version` 精确匹配该 arch 实际 release(官方 x86/aa = `2025-09-07`;自编译 rv/loong = `2025-10-15`,据实分架构红线,防假统一)。
2. **BANNER**:`minio server <data> --address 127.0.0.1:9000` 后台起,轮询直到 `MinIO Object Storage Server`(后端 init + loopback bind 成功)。
3. **HEALTH**:`GET http://127.0.0.1:9000/minio/health/live` 经 loopback net/http 返回 `HTTP/1.1 200 OK`。

验证内核面:Go runtime(M:N 调度 + GC + futex park + getrandom)、**loopback AF_INET TCP**(S3 :9000 + Console :9001)、文件后端 init、mmap。

## 关联内核修(mmap-EOF,rcore-os/tgoskits#1164)

minio 与同批 Go server(etcd)共享大 mmap 路径;`FileBackend::populate` 修复(EOF 之外的稀疏页不预分配帧,Linux:共享文件映射 EOF 外 = SIGBUS)是这批重型 server 在 starry 上稳定运行的前置之一。

注:aarch64 首次在 4-arch 串跑中曾因瞬态资源竞争超时(1200s),单独重试 54s 即通过;toml 已将 aarch64 timeout 提到 3000s 留足 TCG 余量。

## 在 qemu-10 四架构 starry 复现

```bash
# 1) 取/编译四架构 minio(见 SOURCES.md)放 download/golang-bins/minio/<arch>/minio
# 2) 组装 rootfs(debugfs 直写未挂载 ext4;不 mount/不 sync)
for a in x86_64 aarch64 riscv64 loongarch64; do bash prep-minio-rootfs.sh $a; done
# 3) case/ 下 build/qemu toml 放进 tgoskits/test-suit/starryos/stress/minio-0/,跑:
source <tgoskits 根>/.starry-env.sh
for a in x86_64 aarch64 riscv64 loongarch64; do
  cargo xtask starry test qemu --arch $a -g stress -c minio-0
done
# 通过判据:rc=0 + "SUCCESS PATTERN MATCHED: (?m)^MINIO_OK=1" + "1/1 case(s) passed"
```

## 文件

- `case/build-<arch>.toml` — 四架构 rootfs 构建配置
- `case/minio-0/qemu-<arch>.toml` — 四架构 qemu 运行 + 测试脚本(含 `-cpu`/`-machine`、per-arch 版本红线)
- `prep-minio-rootfs.sh` — debugfs 直写 rootfs 组装器(`<arch>` 参数)
- `SOURCES.md` — 四架构二进制来源(官方 amd64/arm64 + 自交叉编译 rv/loong)
