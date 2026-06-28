#!/bin/bash
# prep-juicefs-rootfs.sh — build rootfs-<arch>-juicefs.img for the `juicefs-0`
# StarryOS stress case (#764 "juicefs" / "juice <!-- fs -->" sub-dep item).
#
# WHAT IT DOES
#   Starts from the Alpine musl base image (rootfs-<arch>-alpine.img — busybox +
#   musl already present; we only need a couple of busybox applets: sh/rm/mkdir),
#   then unpacks the juicefs v1.3.1 release tarball
#   (golang-bins/juicefs/<arch>/juicefs-1.3.1-linux-<goarch>.tar.gz) and injects the
#   single static Go binary `juicefs` into /usr/local/bin.
#
#   juicefs ships as a FULLY STATIC ELF (verified: `file` reports "statically
#   linked", `readelf -l` shows NO INTERP segment, `readelf -d` shows NO NEEDED).
#   x86_64/aarch64 are the OFFICIAL juicedata releases; riscv64/loong64 are
#   self-cross-compiled (2026-05-24, go1.26.3) — but unlike etcd, juicefs FORCES
#   CGO (pkg/meta/sql_sqlite.go pulls mattn/go-sqlite3; DataDog/zstd, gspt, go-lz4
#   are cgo-only), so the self-built variants follow the official `juicefs.musl`
#   path: CGO_ENABLED=1 + CGO_LDFLAGS=-static (riscv64-linux-musl-gcc / zig cc
#   -target loongarch64-linux-musl). The result is still a fully static musl binary
#   — Alpine's musl rootfs runs it directly, NO ld-musl/libc/gcompat wiring needed
#   and NO dependency closure. (Provenance + sha256: golang-bins/SOURCES.md §1.3/§2.)
#
#   The tarball layout is FLAT (no top-level dir like etcd): it contains
#   LICENSE / README.md / README_CN.md / juicefs at the archive root. We extract
#   just `juicefs`.
#
# WSL2 RULE (same recipe as the other debugfs-based prep scripts):
#   A bare global `sync` D-state-deadlocks this host. This script uses `debugfs -w`
#   to write into the UNMOUNTED ext4 image directly — it NEVER mounts and NEVER calls
#   sync, so there is no deadlock. (This is the canonical recipe; the older
#   mount+umount prep scripts are deprecated for this reason.)
#
# Usage:   bash prep-juicefs-rootfs.sh <arch>   # arch in x86_64|aarch64|riscv64|loongarch64
#
# Idempotent: re-runs cleanly (debugfs rm+write replaces files). Only touches the
# rootfs-<arch>-juicefs.img it creates; downloads nothing (binaries already on disk).
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs live under tmp/axbuild/rootfs/); the juicefs binaries ship
# alongside this script under ./bins/ (Git LFS). No machine-specific absolute paths.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE/bins"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-juicefs.img
STAGE=/tmp/juicefs-stage-$ARCH

# Map StarryOS arch name -> juicefs release GOARCH token (== tarball filename suffix).
case "$ARCH" in
  x86_64)      GOARCH=amd64   ;;
  aarch64)     GOARCH=arm64   ;;
  riscv64)     GOARCH=riscv64 ;;
  loongarch64) GOARCH=loong64 ;;
  *) echo "unknown arch $ARCH"; exit 2 ;;
esac
TARBALL=$DL/$ARCH/juicefs-1.3.1-linux-$GOARCH.tar.gz

[ -f "$BASE" ]    || { echo "missing base $BASE"; exit 2; }
[ -f "$TARBALL" ] || { echo "missing juicefs tarball $TARBALL"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }

# --- 1. stage the single juicefs binary out of the (flat) release tarball -----
echo "=== [$ARCH] extract juicefs from $(basename "$TARBALL") ==="
rm -rf "$STAGE"; mkdir -p "$STAGE/usr/local/bin" "$STAGE/proj"
tar xzf "$TARBALL" -C "$STAGE" juicefs 2>/dev/null \
  || { echo "  tar extract FAIL"; exit 2; }
mv "$STAGE/juicefs" "$STAGE/usr/local/bin/juicefs"
chmod 0755 "$STAGE/usr/local/bin/juicefs"
echo "  staged:"; ls -la "$STAGE/usr/local/bin/"
file "$STAGE/usr/local/bin/juicefs" | sed 's/, Go BuildID.*//; s/, BuildID.*//'

# --- 2. copy base image, grow to 2G (one ~108MB binary fits easily) -----------
echo "=== [$ARCH] copy base -> $IMG (resize 2G) ==="
cp -f "$BASE" "$IMG"
truncate -s 2G "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1
resize2fs "$IMG" >/dev/null 2>&1

# --- 3. write the staged tree into the UNMOUNTED ext4 via debugfs -------------
# debugfs `rm`+`write` is idempotent for files; dirs are created with `mkdir`
# (errors for already-existing dirs are harmless and filtered from the log scan).
echo "=== [$ARCH] inject tree into $IMG via debugfs -w (no mount, no sync) ==="
DBG=/tmp/juicefs-debugfs-$ARCH.cmds
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
# preserve exec bit on the binary (debugfs `write` defaults to 0644)
echo "sif /usr/local/bin/juicefs mode 0100755" >> "$DBG"

debugfs -w -f "$DBG" "$IMG" >/tmp/juicefs-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/juicefs-debugfs-$ARCH.log \
  | grep -viE 'File not found.*rm|rm:.*not found|File exists.*mkdir' | head

# --- 4. verify the binary landed ----------------------------------------------
echo "=== [$ARCH] verify juicefs in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
debugfs -R "stat /usr/local/bin/juicefs" "$IMG" 2>/dev/null | grep -iE 'Inode|Mode' | head -1
debugfs -R "ls /usr/local/bin" "$IMG" 2>/dev/null | tr ' ' '\n' | grep -xE 'juicefs'
echo "[$ARCH] DONE -> $IMG"
