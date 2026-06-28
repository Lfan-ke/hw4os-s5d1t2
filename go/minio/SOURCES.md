# minio 四架构二进制来源 / provenance

为满足 #764「四架构」要求。minio 官方(dl.min.io)**仅发布 amd64 / arm64**(无 riscv64 / loongarch64 release),故 riscv64 / loongarch64 二进制由本仓库**从源码交叉编译**得到。minio 是纯 Go(CGO 禁用)→ 交叉编译直接成。

## 二进制清单

| 架构 | 文件 | 来源 | release |
|---|---|---|---|
| x86_64 | `x86_64/minio` | 官方 dl.min.io linux-amd64 | RELEASE.2025-09-07T16-13-09Z |
| aarch64 | `aarch64/minio` | 官方 dl.min.io linux-arm64 | RELEASE.2025-09-07T16-13-09Z |
| **riscv64** | `riscv64/minio` | **本仓库交叉编译** | RELEASE.2025-10-15T17-29-55Z |
| **loongarch64** | `loongarch64/minio` | **本仓库交叉编译** | RELEASE.2025-10-15T17-29-55Z |

> 版本红线按 arch 分别校验:官方 x86/aarch64 = `2025-09-07`,自编译 rv/loong = `2025-10-15`(各 toml 的 `MINIO_VER` 据实匹配,不做假统一)。

## 源码 / 工具链

- 仓库: `https://github.com/minio/minio`
- 二进制为 `file` 验证的 **CGO 禁用全静态 Go ELF**,无 libc/ld-musl 依赖。
- riscv64/loong64 交叉编译(go1.26.3,2026-05-24):`CGO_ENABLED=0 GOOS=linux GOARCH={riscv64,loong64} go build`,复刻官方 build 脚本的 ldflags/tags。两 arch 均成。

## 复现取材

```bash
# 官方 amd64/arm64
for ga in amd64 arm64; do curl -LO https://dl.min.io/server/minio/release/linux-$ga/minio; done
# rv/loong 自编译
git clone https://github.com/minio/minio && cd minio
for ga in riscv64 loong64; do CGO_ENABLED=0 GOOS=linux GOARCH=$ga go build -o minio-$ga ./cmd/minio; done
```
