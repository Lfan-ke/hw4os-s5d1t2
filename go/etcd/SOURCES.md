# etcd 四架构二进制来源 / provenance

为满足 #764「四架构」要求。etcd **v3.6.11** 官方发布矩阵已覆盖全部四架构(amd64 / arm64 / **riscv64** / **loong64**),四架构均取**官方 release tarball**,无需自行交叉编译。

## 二进制清单

| 架构 | release tarball | 来源 | 含二进制 |
|---|---|---|---|
| x86_64 | `etcd-v3.6.11-linux-amd64.tar.gz` | 官方 release | etcd / etcdctl / etcdutl |
| aarch64 | `etcd-v3.6.11-linux-arm64.tar.gz` | 官方 release | 同上 |
| riscv64 | `etcd-v3.6.11-linux-riscv64.tar.gz` | 官方 release | 同上 |
| loongarch64 | `etcd-v3.6.11-linux-loong64.tar.gz` | 官方 release | 同上 |

- 仓库: `https://github.com/etcd-io/etcd`,tag **`v3.6.11`**
- 下载: `https://github.com/etcd-io/etcd/releases/download/v3.6.11/etcd-v3.6.11-linux-<goarch>.tar.gz`
- arch→goarch 映射: `x86_64→amd64`、`aarch64→arm64`、`riscv64→riscv64`、`loongarch64→loong64`
- 三个二进制(`etcd` / `etcdctl` / `etcdutl`)均为 **CGO 禁用的全静态 Go ELF**(`file` 报 "statically linked"),无 libc/ld-musl 依赖,直接落 rootfs 即可运行。

## 运行时:`ETCD_UNSUPPORTED_ARCH`(riscv64 / loongarch64 必需)

etcd 自带一个 tier-1 架构门:仅 amd64/arm64/ppc64le/s390x 被视为「supported」,在其它架构上启动时会 fatal 拒绝:

```
Refusing to run etcd on unsupported architecture since ETCD_UNSUPPORTED_ARCH is not set
```

二进制**本身完全可用**,这只是 etcd 的「你自行承担」提示门。riscv64/loongarch64 的 qemu toml 因此在启动前 `export ETCD_UNSUPPORTED_ARCH=<goarch>`(`riscv64` / `loong64`)。amd64/arm64 无需此变量。

## 复现取材

```bash
for ga in amd64 arm64 riscv64 loong64; do
  curl -LO https://github.com/etcd-io/etcd/releases/download/v3.6.11/etcd-v3.6.11-linux-$ga.tar.gz
done
```
