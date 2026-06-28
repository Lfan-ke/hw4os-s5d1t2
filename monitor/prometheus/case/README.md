# prometheus-0 — StarryOS monitor stress case

`prometheus` (p8s) is the CNCF monitoring system: a pull-based metrics scraper + local TSDB
+ PromQL query engine, shipped as a CGO-free static Go binary. This case exercises StarryOS
by running a real headless prometheus server **plus** a real scrape target
(`node_exporter`) and validating the full monitoring loop over the in-guest loopback
interface — version, server readiness, the PromQL engine, and an end-to-end scrape.

Tracking issue: rcore-os/tgoskits#764 (monitor sub-task, `p8s <!-- prometheus -->`). Binary
sources, SHA256, and the loong64 self-cross-build recipe are documented in
`<local download cache>/monitor-bins/SOURCES.md` §1 (prometheus) / §2b (node_exporter).

## What the case does (methodology)

Each `prometheus-0/qemu-<arch>.toml` boots StarryOS with the prometheus rootfs image
(`rootfs-<arch>-prometheus.img`) and runs a `shell_init_cmd` that:

1. `prometheus --version` — assert the running binary is exactly **`3.11.3`** (`VER_OK`,
   red-line); a wrong version fails the case as invalid.
2. Launch **`node_exporter` 1.11.1** headless on loopback `:9100` as a REAL scrape target
   (single CGO-free static Go binary exposing host metrics at `/metrics`); poll
   `/metrics` until it serves (`NE_UP`).
3. Launch **prometheus** headless on loopback `:9090` against a minimal `prometheus.yml`
   that scrapes `job="node"` at `127.0.0.1:9100`.
4. `READY_OK` — assert the log emits `Server is ready to receive web requests.` AND
   `/-/ready` answers `Ready` (HTTP serving + TSDB opened).
5. PromQL engine — `/api/v1/query?query=vector(42)` returns `"status":"success"` and the
   value `42` (the query evaluator runs).
6. `SCRAPE_OK` — `/api/v1/query?query=up{job="node"}` returns value `1` (prometheus has
   actually connected loopback `:9090 -> :9100`, scraped node_exporter, and ingested the
   sample into the TSDB).
7. Gate: emit `PROM_OK=1` **only** when `VER_OK=1 && READY_OK=1 && SCRAPE_OK=1`. The success
   token is produced by a single trailing `printf` so no echo/comment/wrap can produce a
   false positive. On failure the prometheus / node_exporter stderr is dumped.

QEMU pass criteria:
- `success_regex = ["(?m)^PROM_OK=1"]`
- `fail_regex = ['(?i)\bpanic(?:ked)?\b']`
- `timeout = 3000` (TCG cold-start of two Go servers + first scrape interval — not a hang).

QEMU knobs per the case spec: `-smp 1` (single CPU) on every arch; aarch64 uses
`-cpu cortex-a72`; loongarch64 uses `-machine virt -cpu la464` + `to_bin=true`; drive is
`rootfs-<arch>-prometheus.img` via virtio-blk-pci; a virtio-net-pci/user netdev is attached
(loopback works in-guest without it; kept for parity with other server cases).

Kernel stress surface: Go runtime (goroutine scheduler, netpoller over `epoll`), loopback
`socket/bind/listen/accept4` + `connect` (both server and scrape client live in-guest),
`mmap`-backed TSDB head-block writes + WAL, timers. On **aarch64** it additionally depends
on the kernel emulating EL0 reads of the `ID_AA64*` feature registers (`MRS`) that the arm64
Go runtime issues for CPU detection — without that every arm64 Go binary SIGILLs at startup
(rcore-os/tgoskits#1128; `axcpu` aarch64 `emulate_mrs_id_reg`, sanitized user-safe view).

## Files

| file | purpose |
|------|---------|
| `build-x86_64-unknown-none.toml` | StarryOS build config, x86_64 |
| `build-aarch64-unknown-none-softfloat.toml` | StarryOS build config, aarch64 |
| `build-riscv64gc-unknown-none-elf.toml` | StarryOS build config, riscv64 |
| `build-loongarch64-unknown-none-softfloat.toml` | StarryOS build config, loongarch64 |
| `prometheus-0/qemu-x86_64.toml` | QEMU run + DoD probe, x86_64 |
| `prometheus-0/qemu-aarch64.toml` | QEMU run + DoD probe, aarch64 |
| `prometheus-0/qemu-riscv64.toml` | QEMU run + DoD probe, riscv64 |
| `prometheus-0/qemu-loongarch64.toml` | QEMU run + DoD probe, loongarch64 |
| `prep-prometheus-rootfs.sh` | builds `rootfs-<arch>-prometheus.img` from the Alpine base image |

## Per-arch binary availability (HONEST)

Prometheus's **official release** ships linux `386/amd64/arm64/armv*/mips*/ppc64*/riscv64/
s390x` — i.e. **x86_64 + aarch64 + riscv64 are official prebuilt**, but **no loong64**. The
loong64 binary is therefore **SELF-CROSS-BUILT** from the v3.11.3 tag with go1.26.3
(`GOARCH=loong64`, `-tags netgo,builtinassets`, front-end UI built first so it is embedded;
full recipe + SHA256 in `SOURCES.md` §1). `node_exporter` 1.11.1 is the same story (official
x86_64/aarch64/riscv64, self-cross-built loong64, §2b).

| arch | prometheus 3.11.3 | node_exporter 1.11.1 | source | rootfs buildable now? |
|------|-------------------|----------------------|--------|------------------------|
| x86_64 | YES | YES | official amd64 release | YES |
| aarch64 | YES | YES | official arm64 release | YES |
| riscv64 | YES | YES | **official riscv64 release** | YES |
| loongarch64 | YES | YES | **self-cross-built loong64** (go1.26.3) | YES |

All four arches' binaries are CGO-free static Go ELFs (no musl/ld wiring; they do not link
against the rootfs libc). `prep-prometheus-rootfs.sh` extracts the prometheus tarball
(`prometheus` + `promtool` + `prometheus.yml`) and the `node_exporter` binary into the Alpine
base image and writes the minimal scrape config; injection is via `debugfs -w` into the
**unmounted** ext4 image (no mount, no `sync` — avoids the WSL2 D-state deadlock).

## Building the rootfs

```bash
bash prep-prometheus-rootfs.sh x86_64       # -> tmp/axbuild/rootfs/rootfs-x86_64-prometheus.img
bash prep-prometheus-rootfs.sh aarch64      # -> tmp/axbuild/rootfs/rootfs-aarch64-prometheus.img
bash prep-prometheus-rootfs.sh riscv64      # -> tmp/axbuild/rootfs/rootfs-riscv64-prometheus.img
bash prep-prometheus-rootfs.sh loongarch64  # -> tmp/axbuild/rootfs/rootfs-loongarch64-prometheus.img
```
