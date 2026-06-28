# consul 四架构二进制来源 / 交叉编译 provenance

为满足 #764「四架构」要求。HashiCorp 官方 consul **仅发布 amd64 + arm64**(无 riscv64/loongarch64 release),故 riscv64 / loongarch64 二进制由本仓库**从源码交叉编译**得到。交付时凭本文件可完全复现。

## 二进制清单

| 架构 | 文件 | 来源 | 大小 |
|---|---|---|---|
| x86_64 | `x86_64/consul_1.22.7_linux_amd64.zip` | 官方 release | — |
| aarch64 | `aarch64/consul_1.22.7_linux_arm64.zip` | 官方 release | — |
| **riscv64** | `riscv64/consul` | **本仓库交叉编译** | 118 MB |
| **loongarch64** | `loongarch64/consul` | **本仓库交叉编译** | 119 MB |

## 源码

- 仓库: `https://github.com/hashicorp/consul`
- tag: **`v1.22.7`**  (commit `c18bcb9db1fd73307ee8bf64a9bc17610d5427d5`,与官方 amd64 release 的 Revision `c18bcb9d` 一致)
- 克隆: `git clone --depth 1 --branch v1.22.7 https://github.com/hashicorp/consul`

## 工具链

- Go `go1.22.2 linux/amd64`(系统 `/usr/bin/go`;Go ≥1.20 支持 `linux/loong64`、≥1.14 支持 `linux/riscv64`)
- `CGO_ENABLED=0`(纯静态,无 libc 依赖 → 适合 musl/starry)

## 必需的依赖补丁(官方无 release 的真正原因)

consul 的两个间接依赖缺 riscv64/loong64 架构文件,这是官方不出这两个 release 的根因。交叉编译时通过 `go.mod replace` 指向打补丁的本地副本:

### 1. `github.com/boltdb/bolt@v1.3.1`(间接,经 `raft-boltdb/v2 v2.2.2`)
旧版 boltdb(已弃维)无 `bolt_riscv64.go` / `bolt_loong64.go`(缺 `maxMapSize`/`maxAllocSize`/`brokenUnaligned`)。补两文件(常量同 `bolt_arm64.go`,均 64-bit LP64、unaligned OK):
```go
//go:build riscv64   (另一份 //go:build loong64)
package bolt
const maxMapSize = 0xFFFFFFFFFFFF // 256TB
const maxAllocSize = 0x7FFFFFFF
var brokenUnaligned = false
```
boltdb v1.3.1 早于 go modules,replace 本地副本须补 `go.mod`:`module github.com/boltdb/bolt` + `go 1.12`。

### 2. `github.com/shirou/gopsutil/v3@v3.22.9`(直接依赖)
该版 `host/` 无 `host_linux_loong64.go`(缺 `utmp` 结构 + `sizeOfUtmp`)。loong64 与 riscv64 同为 LP64 little-endian,utmp 布局相同 → 直接复制 `host_linux_riscv64.go` 为 `host_linux_loong64.go`(文件名后缀即 build 约束,内容不改)。riscv64 不需此补丁(该版已含 riscv64 host 文件)。

## 复现命令

```bash
git clone --depth 1 --branch v1.22.7 https://github.com/hashicorp/consul /tmp/consul-src

# 补丁 boltdb
cp -r $(go env GOMODCACHE)/github.com/boltdb/bolt@v1.3.1 /tmp/bolt-patched && chmod -R u+w /tmp/bolt-patched
printf 'module github.com/boltdb/bolt\n\ngo 1.12\n' > /tmp/bolt-patched/go.mod
# 写 /tmp/bolt-patched/bolt_riscv64.go 和 bolt_loong64.go(见上常量)

# 补丁 gopsutil(仅 loong64 需要)
cp -r $(go env GOMODCACHE)/github.com/shirou/gopsutil/v3@v3.22.9 /tmp/gopsutil-patched && chmod -R u+w /tmp/gopsutil-patched
cp /tmp/gopsutil-patched/host/host_linux_riscv64.go /tmp/gopsutil-patched/host/host_linux_loong64.go

cd /tmp/consul-src
go mod edit -replace github.com/boltdb/bolt=/tmp/bolt-patched
go mod edit -replace github.com/shirou/gopsutil/v3=/tmp/gopsutil-patched
LD="-s -w -X github.com/hashicorp/consul/version.GitVersion=1.22.7 -X github.com/hashicorp/consul/version.GitCommit=c18bcb9d -X github.com/hashicorp/consul/version.GitDescribe=v1.22.7"
CGO_ENABLED=0 GOOS=linux GOARCH=riscv64 go build -ldflags "$LD" -o consul-riscv64 .
CGO_ENABLED=0 GOOS=linux GOARCH=loong64  go build -ldflags "$LD" -o consul-loong64 .
```

构建于 2026-05-29。boltdb/gopsutil 补丁已固化到可重跑的 `build-consul-riscv-loong.sh`。
