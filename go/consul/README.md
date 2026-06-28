# consul — HashiCorp 服务发现/KV (#764)

**Consul 1.22.7**(Go,服务发现 + 健康检查 + 分布式 KV + serf gossip)在 StarryOS 四架构(x86_64/aarch64/riscv64/loongarch64)单核 qemu-10 上运行:`consul agent -dev` 在 loopback 启动,集群成员可见,四架构全部通过。

## DoD 结论(qemu-10 单核 starry,2026-06-06)

| arch | 结果 | 备注 |
|------|------|------|
| x86_64 | √ `CONSUL_OK=1` (3/3) | 官方 amd64 release |
| aarch64 | √ `CONSUL_OK=1` (3/3) | `-cpu cortex-a72`;官方 arm64 release |
| riscv64 | √ `CONSUL_OK=1` (3/3) | `-cpu rv64`;源码自编译;依赖内核 getrlimit 修(见下) |
| loongarch64 | √ `CONSUL_OK=1` (3/3) | `-machine virt -cpu la464`;源码自编译 |

判据权威:xtask `rc=0` + 日志 `SUCCESS PATTERN MATCHED` + `1/1 case(s) passed` + 锚定 `^CONSUL_OK=1`,四架构各复核一致。

## 测试内容(consul dev agent 单节点)

门控 `CONSUL_OK = (PASS==TOTAL==3)`:
1. **VER**:`consul version` 精确 `Consul v1.22.7`。
2. **AGENT**:`consul agent -dev -bind=127.0.0.1 -client=127.0.0.1 -node=starrynode` 后台起,轮询直到 `Consul agent running!`(serf/raft 在 loopback 起来)。
3. **MEMBERS**:`consul members` 经 client RPC 往返显示 `starrynode ... alive`(gossip + RPC over loopback)。

验证内核面:Go runtime(goroutine 调度 + netpoller/epoll)、**loopback AF_INET TCP/UDP**(serf gossip 8301 + RPC 8300 + HTTP 8500)、getrlimit/futex/mmap。

## 根治内核修

riscv64 上 consul agent 启动即 `==> function not implemented` 而 `Done(1)` 退出。根因:`getrlimit` 被 starry 的 syscall dispatch 用 `#[cfg(target_arch="x86_64")]` 独占门控,而 consul 的 Go runtime 在 riscv64 直接调 legacy `getrlimit` → 落 ENOSYS catch-all → abort。修(`os/StarryOS/kernel/src/syscall/mod.rs`):去掉 cfg,让四架构都把 `getrlimit`/`setrlimit` 路由到既有 `sys_prlimit64`(pid=0);`struct rlimit` 在所有 64 位 arch 上 = 两个 u64 = 与 `rlimit64` 同布局,转换安全。x86/aa/loong 的 consul 走 prlimit64 故先前已通过,此修对其无回归。

## 来源(provenance)

- **consul 1.22.7**:x86_64(amd64)/aarch64(arm64)= HashiCorp 官方 release 预编译;riscv64/loongarch64 = 源码交叉编译(补 boltdb/gopsutil 的 arch 文件),build 脚本 `build-consul-riscv-loong.sh`。CGO-free 静态 Go 二进制。
- 二进制连同 `BUILD-PROVENANCE.md` 随附于本目录 `bins/`(Git LFS)。

## 构建运行

```bash
bash prep-consul-rootfs.sh <arch>           # debugfs 注入 consul 二进制
source <tgoskits 根>/.starry-env.sh         # 启用 qemu-10
cargo xtask starry test qemu --arch <arch> -g stress -c consul-0
# 成功判据: rc=0 + SUCCESS PATTERN MATCHED + ^CONSUL_OK=1
```
