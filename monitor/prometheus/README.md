# prometheus (p8s) — 监控时序数据库 + PromQL 引擎 (monitor/p8s)

**Prometheus**（CNCF 监控系统：拉取式 metrics 抓取 + TSDB + PromQL 查询引擎，Go 静态二进制）在 StarryOS 四架构单核 qemu-10 上 4/4 通过。

## DoD 验证结论（qemu-10 单核 starry，2026-06-05）

| arch | 结果 | 备注 |
|------|------|------|
| x86_64 | √ `PROM_OK=1` | 官方 amd64 release |
| aarch64 | √ `PROM_OK=1` | `-cpu cortex-a72`；官方 arm64 release。依赖内核 EL0 `MRS ID_AA64*` 模拟（arm64 Go runtime cpu 探测）见 rcore-os/tgoskits#1128 |
| riscv64 | √ `PROM_OK=1` | `-cpu rv64`；**官方 riscv64 release**（Prometheus 上游发 riscv64）|
| loongarch64 | √ `PROM_OK=1` | `-cpu la464 -machine virt`；**自交叉编译 loong64**（上游无 loong64 预编译）|

判据权威：xtask `rc=0` + 日志 `SUCCESS PATTERN MATCHED` + `1/1 case(s) passed` + 锚定 `^PROM_OK=1`，四架构各复核一致。

测试内容（真实端到端监控链路，非仅 `--version`）：

1. **VER_OK（红线）**：`prometheus --version` 必须精确报 `3.11.3`，版本不符即判测例无效。
2. 前台拉起 **node_exporter 1.11.1**（最简 exporter，单个 CGO-free 静态 Go 二进制，暴露 host metrics 于 loopback `:9100/metrics`）作为**真实抓取目标**。
3. **READY_OK**：headless 拉起 prometheus server 于 loopback `:9090`，断言日志 `Server is ready to receive web requests.` 且 `/-/ready` 返回 `Ready`（= HTTP 服务起来 + TSDB 打开）。
4. **PromQL 引擎**：`/api/v1/query?query=vector(42)` 返回 `status:success` + 值 `42`（查询引擎工作）。
5. **SCRAPE_OK**：`/api/v1/query?query=up{job="node"}` 返回值 `1`（prometheus 经 loopback 真的把 node_exporter 抓到并入库）。
6. Gate：`VER_OK && READY_OK && SCRAPE_OK` 全真才由单条尾部 `printf` 产出 `PROM_OK=1`（防 echo/注释/换行假阳性）。

验证内核面：Go runtime（goroutine 调度 + netpoller/epoll）、loopback TCP/HTTP 双向（抓取端 `:9090`→`:9100` + 客户端查询）、mmap-backed TSDB 写盘、定时器/wal。

## qemu-10 真 Linux 4-arch 参照（测例可行性基线）

同测例在真 Linux 内核（Alpine linux-lts/virt @qemu-10）四架构 server 均 `PROM_OK=1`（loong64 二进制 + 逻辑经对应内核验证）。即测例逻辑在真 Linux 四架构成立，starry 全部通过可信。

## 来源（provenance，每个二进制据实注明）

- **prometheus 3.11.3**（`github.com/prometheus/prometheus` v3.11.3 release tarball `prometheus-3.11.3.linux-<goarch>.tar.gz`）：
  - x86_64(amd64) / aarch64(arm64) / riscv64：**官方预编译**，SHA256 与官方 `sha256sums.txt` 逐字一致：
    - `9479af67…bb38  amd64`
    - `d2ec0a96…708a  arm64`
    - `bd697893…ba9a  riscv64`
  - loongarch64(loong64)：上游**不发** loong64 → 从 v3.11.3 tag **自交叉编译**（go1.26.3，2026-05-24）：① 前端 UI（arch 无关，Node v22.21.1 `npm run build` + `compress_assets.sh` 生成 `web/ui/embed.go`）② `CGO_ENABLED=0 GOOS=linux GOARCH=loong64 go build -tags "netgo,builtinassets" -ldflags "-X .../version.Version=3.11.3 …"`；`file` 报告 `LoongArch ELF`，~143MB（含嵌入 UI，与官方 riscv64 147MB 同量级即证 UI 已嵌）。SHA256 `0805224b…97b4`（自编非确定性）。
- **node_exporter 1.11.1**（`github.com/prometheus/node_exporter` release，单静态 Go 二进制）：x86_64/aarch64/riscv64 官方预编译；loongarch64 自交叉编译（同上工具链）。
- 完整 provenance（下载 URL、全部 SHA256、loong64 自编 configure/ldflags、node_exporter §2b、grafana §2）见 `<本机下载缓存目录>/monitor-bins/SOURCES.md`。

## 构建运行

```bash
bash case/prep-prometheus-rootfs.sh <arch>   # debugfs 注入 prometheus + promtool + node_exporter + prometheus.yml
source <仓库根>/.starry-env.sh               # 使用 qemu-10
cargo xtask starry test qemu --arch <arch> -g stress -c prometheus-0
# 成功判据: rc=0 + SUCCESS PATTERN MATCHED + ^PROM_OK=1
```

注：四架构 prometheus/node_exporter 均为 **CGO-free 纯静态 Go 二进制**，无 musl/ld 接线需求（不依赖 rootfs libc）。`timeout = 3000`（TCG 下 Go server 冷启动 + 首轮 scrape 间隔需足够长，非真 hang）。
