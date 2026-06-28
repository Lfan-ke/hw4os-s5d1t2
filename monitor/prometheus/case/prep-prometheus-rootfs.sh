#!/bin/bash
# prep-prometheus-rootfs.sh — build rootfs-<arch>-prometheus.img for the
# `prometheus-0` StarryOS stress case (monitor category "p8s").
#
# WHAT IT DOES
#   Starts from the Alpine musl base image (rootfs-<arch>-alpine.img — busybox +
#   musl already present; we only need a couple of busybox applets: sh/sleep/kill
#   and busybox `wget` for the HTTP API probe), then unpacks the prometheus v3.11.3
#   release tarball (monitor-bins/prometheus/<arch>/prometheus-3.11.3.linux-<goarch>.tar.gz)
#   and injects the TWO static Go binaries — `prometheus` (server) + `promtool` —
#   into /usr/local/bin, plus a scrape config at /etc/prometheus.yml.
#
#   It ALSO injects `node_exporter` v1.11.1 (the simplest exporter — a single
#   CGO-free static Go binary exposing host metrics at :9100/metrics) into
#   /usr/bin, so the qemu case can run it as a REAL scrape target and exercise
#   prometheus's core scrape -> ingest -> query pipeline (up{job="node"}==1),
#   not just the synthetic vector(42). node_exporter binary per arch +
#   provenance: monitor-bins/node_exporter/<arch>/node_exporter, ../SOURCES.md §2b
#   (amd64/arm64/riscv64 official; loong64 self-cross-compiled, CGO_ENABLED=0).
#
#   prometheus ships as a FULLY STATIC, CGO-disabled Go binary (`file` reports
#   "statically linked", no interpreter; tags=netgo,builtinassets so
#   the web UI is embedded), so the rootfs needs NO libc/ld-musl wiring and NO
#   dependency closure — drop the binary + a writable tsdb dir and it runs. The Go
#   runtime gets its entropy via getrandom(2) (StarryOS provides it; no /dev/urandom
#   node needed), parks goroutines via futex, and the net stack listens on loopback
#   :9090 (in-guest user-net). The PromQL engine + TSDB + web/API path are exercised
#   by the qemu case via busybox wget against /-/ready and /api/v1/query.
#
# ARCH MAP (StarryOS arch name -> prometheus release GOARCH token == tar top dir):
#   x86_64 -> amd64, aarch64 -> arm64, riscv64 -> riscv64, loongarch64 -> loong64.
#   amd64/arm64/riscv64 are OFFICIAL GitHub release assets; loong64 is the
#   self-cross-compiled v3.11.3 (go1.26.3, embedded UI) per ../SOURCES.md §1.
#
# WSL2 RULE:
#   A bare global `sync` D-state-deadlocks this host. This script uses `debugfs -w`
#   to write into the UNMOUNTED ext4 image directly — it NEVER mounts and NEVER calls
#   sync, so there is no deadlock. (This is the canonical recipe; mount+umount prep
#   scripts are deprecated for this reason.)
#
# Usage:   bash prep-prometheus-rootfs.sh <arch>   # arch in x86_64|aarch64|riscv64|loongarch64
#
# Idempotent: re-runs cleanly (debugfs rm+write replaces files). Only touches the
# rootfs-<arch>-prometheus.img it creates; downloads nothing (tarballs already on disk).
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs live under tmp/axbuild/rootfs/); the prometheus + node_exporter
# binaries ship alongside this script under ./bins/ (Git LFS). No machine-specific paths.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE/bins/prometheus"
NEDL="$HERE/bins/node_exporter"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-prometheus.img
STAGE=/tmp/prometheus-stage-$ARCH

# Map StarryOS arch name -> prometheus release GOARCH token (== tar top-level dir suffix).
case "$ARCH" in
  x86_64)      GOARCH=amd64   ;;
  aarch64)     GOARCH=arm64   ;;
  riscv64)     GOARCH=riscv64 ;;
  loongarch64) GOARCH=loong64 ;;
  *) echo "unknown arch $ARCH"; exit 2 ;;
esac
TARBALL=$DL/$ARCH/prometheus-3.11.3.linux-$GOARCH.tar.gz
TOPDIR=prometheus-3.11.3.linux-$GOARCH   # directory name INSIDE the tarball
NEBIN=$NEDL/$ARCH/node_exporter          # pre-extracted static node_exporter binary

[ -f "$BASE" ]    || { echo "missing base $BASE"; exit 2; }
[ -f "$TARBALL" ] || { echo "missing prometheus tarball $TARBALL"; exit 2; }
[ -f "$NEBIN" ]   || { echo "missing node_exporter binary $NEBIN"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }

# --- 1. stage prometheus + promtool + node_exporter + config out of the release tarball ---
echo "=== [$ARCH] extract prometheus/promtool from $(basename "$TARBALL") + node_exporter ==="
rm -rf "$STAGE"
mkdir -p "$STAGE/usr/local/bin" "$STAGE/usr/bin" "$STAGE/etc" "$STAGE/tmp/prom" "$STAGE/proj"
tar xzf "$TARBALL" -C "$STAGE" \
  "$TOPDIR/prometheus" "$TOPDIR/promtool" 2>/dev/null \
  || { echo "  tar extract FAIL"; exit 2; }
mv "$STAGE/$TOPDIR/prometheus" "$STAGE/usr/local/bin/prometheus"
mv "$STAGE/$TOPDIR/promtool"   "$STAGE/usr/local/bin/promtool"
rmdir "$STAGE/$TOPDIR" 2>/dev/null || rm -rf "$STAGE/$TOPDIR"
chmod 0755 "$STAGE/usr/local/bin/prometheus" "$STAGE/usr/local/bin/promtool"

# node_exporter (the "simplest exporter") -> /usr/bin; pre-extracted static Go binary.
cp -f "$NEBIN" "$STAGE/usr/bin/node_exporter"
chmod 0755 "$STAGE/usr/bin/node_exporter"

# prometheus config: keep the global block AND add a `node` scrape job pointing at the
# in-guest node_exporter on loopback :9100 so the case exercises scrape->ingest->query
# (up{job="node"}==1). scrape_interval 1s for the node job => first scrape lands fast
# under TCG. The existing vector(42) PromQL probe is unaffected.
cat > "$STAGE/etc/prometheus.yml" <<'YML'
global:
  scrape_interval: 15s
  evaluation_interval: 15s
scrape_configs:
  - job_name: node
    scrape_interval: 5s
    scrape_timeout: 4s
    static_configs:
      - targets: ['127.0.0.1:9100']
YML

echo "  staged:"; ls -la "$STAGE/usr/local/bin/" "$STAGE/usr/bin/"
file "$STAGE/usr/local/bin/prometheus" | sed 's/,.*statically/  [static]/'
file "$STAGE/usr/bin/node_exporter"    | sed 's/,.*statically/  [static]/'

# --- 2. copy base image, grow to 3G (prometheus ELF is ~210-230MB; leave tsdb headroom) ---
echo "=== [$ARCH] copy base -> $IMG (resize 3G) ==="
cp -f "$BASE" "$IMG"
truncate -s 3G "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1
resize2fs "$IMG" >/dev/null 2>&1

# --- 3. write the staged tree into the UNMOUNTED ext4 via debugfs -------------
# debugfs `rm`+`write` is idempotent for files; dirs are created with `mkdir`
# (errors for already-existing dirs are harmless and filtered from the log scan).
echo "=== [$ARCH] inject tree into $IMG via debugfs -w (no mount, no sync) ==="
DBG=/tmp/prometheus-debugfs-$ARCH.cmds
: > "$DBG"
# directories first (shallow->deep)
( cd "$STAGE" && find . -type d | sort ) | while read -r d; do
  rel="${d#./}"; [ "$rel" = "." ] && continue
  echo "mkdir /$rel" >> "$DBG"
done
# files: rm-then-write so a re-run replaces cleanly
( cd "$STAGE" && find . -type f | sort ) | while read -r f; do
  rel="${f#./}"
  echo "rm /$rel"                 >> "$DBG"
  echo "write $STAGE/$rel /$rel"  >> "$DBG"
done
# preserve exec bit on the binaries (debugfs `write` defaults to 0644)
for b in prometheus promtool; do
  echo "sif /usr/local/bin/$b mode 0100755" >> "$DBG"
done
echo "sif /usr/bin/node_exporter mode 0100755" >> "$DBG"

debugfs -w -f "$DBG" "$IMG" >/tmp/prometheus-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/prometheus-debugfs-$ARCH.log \
  | grep -viE 'File not found.*rm|rm:.*not found|File exists.*mkdir' | head

# --- 4. verify the binaries landed --------------------------------------------
echo "=== [$ARCH] verify prometheus + node_exporter in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
debugfs -R "stat /usr/local/bin/prometheus" "$IMG" 2>/dev/null | grep -iE 'Inode|Mode' | head -1
debugfs -R "ls /usr/local/bin" "$IMG" 2>/dev/null | tr ' ' '\n' | grep -xE 'prometheus|promtool'
debugfs -R "stat /usr/bin/node_exporter" "$IMG" 2>/dev/null | grep -iE 'Inode|Mode' | head -1
debugfs -R "ls /usr/bin" "$IMG" 2>/dev/null | tr ' ' '\n' | grep -x 'node_exporter'
debugfs -R "ls /etc" "$IMG" 2>/dev/null | tr ' ' '\n' | grep -x 'prometheus.yml'
echo "[$ARCH] DONE -> $IMG"
