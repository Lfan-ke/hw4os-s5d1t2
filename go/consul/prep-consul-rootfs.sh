#!/bin/bash
# prep-consul-rootfs.sh — build rootfs-<arch>-consul.img for the `consul-0` StarryOS
# stress case (#764 "consul").
#
# WHAT IT DOES
#   Starts from the Alpine musl base image (rootfs-<arch>-alpine.img — busybox +
#   musl already present; we only need a couple of busybox applets: sh/sleep/kill),
#   then unpacks the OFFICIAL HashiCorp consul v1.22.7 release ZIP
#   (golang-bins/consul/<arch>/consul_1.22.7_linux_<goarch>.zip) and injects the
#   single static Go binary `consul` into /usr/local/bin.
#
#   consul ships as a FULLY STATIC, CGO-disabled Go binary (`file` reports
#   "statically linked", no interpreter), so the rootfs needs NO libc/ld-musl wiring
#   for it and NO dependency closure — drop the binary + a writable data dir and it
#   runs. The Go runtime gets its entropy via the getrandom(2) syscall (StarryOS
#   provides it; no /dev/urandom node needed) and parks goroutines via futex.
#
# ARCH COVERAGE
#   consul is provided for x86_64 (amd64) and aarch64 (arm64) ONLY. The official
#   HashiCorp releases do not ship riscv64/loongarch64, and consul cannot be trivially
#   cross-compiled for them (missing boltdb + gopsutil arch-specific source files), so
#   the riscv64/loongarch64 download dirs are empty and this script refuses those arches.
#
# WSL2 RULE:
#   A bare global `sync` D-state-deadlocks the WSL2 host. This script uses `debugfs -w`
#   to write into the UNMOUNTED ext4 image directly — it NEVER mounts and NEVER calls
#   sync, so there is no deadlock. (This is the canonical recipe; the older
#   mount+umount prep scripts are deprecated for this reason.)
#
# Usage:   bash prep-consul-rootfs.sh <arch>     # arch in x86_64|aarch64
#
# Idempotent: re-runs cleanly (debugfs rm+write replaces files). Only touches the
# rootfs-<arch>-consul.img it creates; downloads nothing (binary already on disk).
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs live under tmp/axbuild/rootfs/); the consul binaries ship
# alongside this script under ./bins/ (Git LFS). No machine-specific absolute paths.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE/bins"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-consul.img
STAGE=/tmp/consul-stage-$ARCH

# Map StarryOS arch name -> consul source. amd64/arm64 = official release zip;
# riscv64/loongarch64 = bare binary we cross-compiled from source (no official
# release exists — boltdb/gopsutil arch files were missing; see BUILD-PROVENANCE.md
# and build-consul-riscv-loong.sh). BARE=1 means "use $DL/$ARCH/consul directly".
BARE=0
case "$ARCH" in
  x86_64)      GOARCH=amd64 ;;
  aarch64)     GOARCH=arm64 ;;
  riscv64)     GOARCH=riscv64; BARE=1 ;;
  loongarch64) GOARCH=loong64; BARE=1 ;;
  *) echo "unknown arch $ARCH"; exit 2 ;;
esac
ZIP=$DL/$ARCH/consul_1.22.7_linux_$GOARCH.zip
BIN=$DL/$ARCH/consul

[ -f "$BASE" ] || { echo "missing base $BASE"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }

# --- 1. stage the consul binary (release zip for amd64/arm64; cross-built bare
#         binary for riscv64/loongarch64) -------------------------------------
rm -rf "$STAGE"; mkdir -p "$STAGE/usr/local/bin" "$STAGE/consul-data" "$STAGE/proj"
if [ "$BARE" = 1 ]; then
  echo "=== [$ARCH] stage cross-built consul ($BIN) ==="
  [ -f "$BIN" ] || { echo "missing cross-built consul $BIN — run build-consul-riscv-loong.sh"; exit 2; }
  cp "$BIN" "$STAGE/usr/local/bin/consul"
else
  echo "=== [$ARCH] extract consul from $(basename "$ZIP") ==="
  command -v unzip >/dev/null 2>&1 || { echo "need unzip"; exit 2; }
  [ -f "$ZIP" ] || { echo "missing consul zip $ZIP"; exit 2; }
  unzip -o "$ZIP" consul -d "$STAGE" >/dev/null 2>&1 \
    || { echo "  unzip extract FAIL"; exit 2; }
  mv "$STAGE/consul" "$STAGE/usr/local/bin/consul"
fi
chmod 0755 "$STAGE/usr/local/bin/consul"
echo "  staged:"; ls -la "$STAGE/usr/local/bin/"
file "$STAGE/usr/local/bin/consul" | sed 's/,.*statically/  [static]/'

# --- 2. copy base image, grow to 2G (one ~180MB binary fits with headroom) ---
echo "=== [$ARCH] copy base -> $IMG (resize 2G) ==="
cp -f "$BASE" "$IMG"
truncate -s 2G "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1
resize2fs "$IMG" >/dev/null 2>&1

# --- 3. write the staged tree into the UNMOUNTED ext4 via debugfs -------------
# debugfs `rm`+`write` is idempotent for files; dirs are created with `mkdir`
# (errors for already-existing dirs are harmless and filtered from the log scan).
echo "=== [$ARCH] inject tree into $IMG via debugfs -w (no mount, no sync) ==="
DBG=/tmp/consul-debugfs-$ARCH.cmds
: > "$DBG"
# directories first (shallow->deep)
( cd "$STAGE" && find . -type d | sort ) | while read -r d; do
  rel="${d#./}"; [ "$rel" = "." ] && continue
  echo "mkdir /$rel" >> "$DBG"
done
# files: rm-then-write so a re-run replaces cleanly
( cd "$STAGE" && find . -type f | sort ) | while read -r f; do
  rel="${f#./}"
  echo "rm /$rel"            >> "$DBG"
  echo "write $STAGE/$rel /$rel" >> "$DBG"
done
# preserve exec bit on the binary (debugfs `write` defaults to 0644)
echo "sif /usr/local/bin/consul mode 0100755" >> "$DBG"

debugfs -w -f "$DBG" "$IMG" >/tmp/consul-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/consul-debugfs-$ARCH.log \
  | grep -viE 'File not found.*rm|rm:.*not found|File exists.*mkdir' | head

# --- 4. verify the binary landed ----------------------------------------------
echo "=== [$ARCH] verify consul in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
debugfs -R "stat /usr/local/bin/consul" "$IMG" 2>/dev/null | grep -iE 'Inode|Mode' | head -1
debugfs -R "ls /usr/local/bin" "$IMG" 2>/dev/null | tr ' ' '\n' | grep -xE 'consul'
echo "[$ARCH] DONE -> $IMG"
