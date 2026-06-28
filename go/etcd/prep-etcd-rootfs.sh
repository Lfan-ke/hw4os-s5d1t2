#!/bin/bash
# prep-etcd-rootfs.sh — build rootfs-<arch>-etcd.img for the `etcd-0` StarryOS
# stress case (#764 "etcd").
#
# WHAT IT DOES
#   Starts from the Alpine musl base image (rootfs-<arch>-alpine.img — busybox +
#   musl already present; we only need a couple of busybox applets: sh/sleep/kill),
#   then unpacks the OFFICIAL etcd v3.6.11 release tarball
#   (golang-bins/etcd/<arch>/etcd-v3.6.11-linux-<goarch>.tar.gz) and injects just the
#   THREE static Go binaries — `etcd`, `etcdctl`, `etcdutl` — into /usr/bin.
#
#   etcd ships as a FULLY STATIC, CGO-disabled Go binary (`file` reports
#   "statically linked"), so the rootfs needs NO libc/ld-musl wiring for it and NO
#   dependency closure — drop the binary + a writable data dir and it runs. The Go
#   runtime gets its entropy via the getrandom(2) syscall (StarryOS provides it; no
#   /dev/urandom node needed) and parks goroutines via futex.
#
# WSL2 RULE:
#   A bare global `sync` D-state-deadlocks the WSL2 host. This script uses `debugfs -w`
#   to write into the UNMOUNTED ext4 image directly — it NEVER mounts and NEVER calls
#   sync, so there is no deadlock. (This is the canonical recipe; the older
#   mount+umount prep scripts are deprecated for this reason.)
#
# Usage:   bash prep-etcd-rootfs.sh <arch>     # arch in x86_64|aarch64|riscv64|loongarch64
#
# Idempotent: re-runs cleanly (debugfs rm+write replaces files). Only touches the
# rootfs-<arch>-etcd.img it creates; downloads nothing (binaries already on disk).
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs live under tmp/axbuild/rootfs/); the etcd binaries ship
# alongside this script under ./bins/ (Git LFS). No machine-specific absolute paths.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE/bins"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-etcd.img
STAGE=/tmp/etcd-stage-$ARCH

# Map StarryOS arch name -> etcd release GOARCH token (== tar top-level dir suffix).
case "$ARCH" in
  x86_64)      GOARCH=amd64   ;;
  aarch64)     GOARCH=arm64   ;;
  riscv64)     GOARCH=riscv64 ;;
  loongarch64) GOARCH=loong64 ;;
  *) echo "unknown arch $ARCH"; exit 2 ;;
esac
TARBALL=$DL/$ARCH/etcd-v3.6.11-linux-$GOARCH.tar.gz
TOPDIR=etcd-v3.6.11-linux-$GOARCH   # directory name INSIDE the tarball

[ -f "$BASE" ]    || { echo "missing base $BASE"; exit 2; }
[ -f "$TARBALL" ] || { echo "missing etcd tarball $TARBALL"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }

# --- 1. stage the three etcd binaries out of the release tarball --------------
echo "=== [$ARCH] extract etcd/etcdctl/etcdutl from $(basename "$TARBALL") ==="
rm -rf "$STAGE"; mkdir -p "$STAGE/usr/bin" "$STAGE/etcd-data" "$STAGE/proj"
tar xzf "$TARBALL" -C "$STAGE" \
  "$TOPDIR/etcd" "$TOPDIR/etcdctl" "$TOPDIR/etcdutl" 2>/dev/null \
  || { echo "  tar extract FAIL"; exit 2; }
mv "$STAGE/$TOPDIR/etcd"    "$STAGE/usr/bin/etcd"
mv "$STAGE/$TOPDIR/etcdctl" "$STAGE/usr/bin/etcdctl"
mv "$STAGE/$TOPDIR/etcdutl" "$STAGE/usr/bin/etcdutl"
rmdir "$STAGE/$TOPDIR" 2>/dev/null || rm -rf "$STAGE/$TOPDIR"
chmod 0755 "$STAGE/usr/bin/etcd" "$STAGE/usr/bin/etcdctl" "$STAGE/usr/bin/etcdutl"
echo "  staged:"; ls -la "$STAGE/usr/bin/"
file "$STAGE/usr/bin/etcd" | sed 's/,.*statically/  [static]/'

# --- 2. copy base image, grow to 2G (three ~25MB binaries fit easily) ---------
echo "=== [$ARCH] copy base -> $IMG (resize 2G) ==="
cp -f "$BASE" "$IMG"
truncate -s 2G "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1
resize2fs "$IMG" >/dev/null 2>&1

# --- 3. write the staged tree into the UNMOUNTED ext4 via debugfs -------------
# debugfs `rm`+`write` is idempotent for files; dirs are created with `mkdir`
# (errors for already-existing dirs are harmless and filtered from the log scan).
echo "=== [$ARCH] inject tree into $IMG via debugfs -w (no mount, no sync) ==="
DBG=/tmp/etcd-debugfs-$ARCH.cmds
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
# preserve exec bit on the three binaries (debugfs `write` defaults to 0644)
for b in etcd etcdctl etcdutl; do
  echo "sif /usr/bin/$b mode 0100755" >> "$DBG"
done

debugfs -w -f "$DBG" "$IMG" >/tmp/etcd-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/etcd-debugfs-$ARCH.log \
  | grep -viE 'File not found.*rm|rm:.*not found|File exists.*mkdir' | head

# --- 4. verify the binaries landed --------------------------------------------
echo "=== [$ARCH] verify etcd in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
debugfs -R "stat /usr/bin/etcd"    "$IMG" 2>/dev/null | grep -iE 'Inode|Mode' | head -1
debugfs -R "ls /usr/bin" "$IMG" 2>/dev/null | tr ' ' '\n' | grep -xE 'etcd|etcdctl|etcdutl'
echo "[$ARCH] DONE -> $IMG"
