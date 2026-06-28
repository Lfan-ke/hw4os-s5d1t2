#!/bin/bash
# prep-minio-rootfs.sh — build rootfs-<arch>-minio.img for the `minio-0` StarryOS
# stress case (#764 "minio").
#
# WHAT IT DOES
#   Starts from the Alpine musl base image (rootfs-<arch>-alpine.img — busybox
#   sh/sleep/kill/wget already present), then injects the single static Go binary
#   golang-bins/minio/<arch>/minio into /usr/bin/minio and pre-creates the server's
#   data + config dirs (/tmp is writable at runtime, but we also stage /minio-data
#   and /root/.minio so the layout is explicit).
#
#   minio is a FULLY STATIC, CGO-disabled Go binary (`file` reports
#   "statically linked"), so the rootfs needs NO libc/ld-musl wiring and NO
#   dependency closure for it. The embedded Console UI is baked into the binary
#   (~102-111 MB), so no extra web assets are needed. Entropy comes from the
#   getrandom(2) syscall (no /dev/urandom node required). The health probe
#   (busybox wget against http://127.0.0.1:9000/minio/health/live) runs over
#   loopback, which works in-guest without -netdev.
#
#   !!! VERSION SPLIT (READ — affects the per-arch toml version gate) !!!
#   The two OFFICIAL dl.min.io binaries and the two self-cross-compiled ones carry
#   DIFFERENT release tags (dl.min.io serves "latest stable", which rolled BEFORE
#   the riscv64/loong64 cross-builds were cut):
#       x86_64  / aarch64     -> RELEASE.2025-09-07T16-13-09Z   (official dl.min.io)
#       riscv64 / loongarch64 -> RELEASE.2025-10-15T17-29-55Z   (self cross-compiled)
#   This script only ASSEMBLES the rootfs; the version assertion lives in each
#   qemu-<arch>.toml, where MINIO_VER is set to the date THAT arch actually ships.
#   See SOURCES.md for the per-arch version red-line rationale.
#
# WSL2 RULE:
#   A bare global `sync` D-state-deadlocks the WSL2 host. This script uses `debugfs -w`
#   to write into the UNMOUNTED ext4 image directly — it NEVER mounts and NEVER calls
#   sync, so there is no deadlock.
#
# Usage:   bash prep-minio-rootfs.sh <arch>     # arch in x86_64|aarch64|riscv64|loongarch64
#
# Idempotent: re-runs cleanly. Only touches the rootfs-<arch>-minio.img it creates.
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs live under tmp/axbuild/rootfs/); the minio binaries ship
# alongside this script under ./bins/ (Git LFS). No machine-specific absolute paths.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE/bins"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-minio.img
STAGE=/tmp/minio-stage-$ARCH

case "$ARCH" in
  x86_64|aarch64|riscv64|loongarch64) : ;;
  *) echo "unknown arch $ARCH"; exit 2 ;;
esac
BIN=$DL/$ARCH/minio

[ -f "$BASE" ] || { echo "missing base $BASE"; exit 2; }
[ -f "$BIN" ]  || { echo "missing minio binary $BIN"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }

# --- 1. stage the minio binary + runtime dirs ---------------------------------
echo "=== [$ARCH] stage minio binary + data dirs ==="
rm -rf "$STAGE"; mkdir -p "$STAGE/usr/bin" "$STAGE/minio-data" "$STAGE/root/.minio"
cp -f "$BIN" "$STAGE/usr/bin/minio"
chmod 0755 "$STAGE/usr/bin/minio"
file "$STAGE/usr/bin/minio" | sed 's/,.*statically/  [static]/'
# echo the version this arch's binary carries (informational; gate is in the toml)
strings -n 8 "$STAGE/usr/bin/minio" 2>/dev/null \
  | grep -oE 'RELEASE\.20[0-9]{2}-[0-9]{2}-[0-9]{2}T[0-9]{2}-[0-9]{2}-[0-9]{2}Z' \
  | sort -u | head -1 | sed 's/^/  embedded version: /'

# --- 2. copy base image, grow to 2G (single ~110MB binary) --------------------
echo "=== [$ARCH] copy base -> $IMG (resize 2G) ==="
cp -f "$BASE" "$IMG"
truncate -s 2G "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1
resize2fs "$IMG" >/dev/null 2>&1

# --- 3. write the staged tree into the UNMOUNTED ext4 via debugfs -------------
echo "=== [$ARCH] inject tree into $IMG via debugfs -w (no mount, no sync) ==="
DBG=/tmp/minio-debugfs-$ARCH.cmds
: > "$DBG"
( cd "$STAGE" && find . -type d | sort ) | while read -r d; do
  rel="${d#./}"; [ "$rel" = "." ] && continue
  echo "mkdir /$rel" >> "$DBG"
done
( cd "$STAGE" && find . -type f | sort ) | while read -r f; do
  rel="${f#./}"
  echo "rm /$rel"            >> "$DBG"
  echo "write $STAGE/$rel /$rel" >> "$DBG"
done
# preserve exec bit on the binary (debugfs `write` defaults to 0644)
echo "sif /usr/bin/minio mode 0100755" >> "$DBG"

debugfs -w -f "$DBG" "$IMG" >/tmp/minio-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/minio-debugfs-$ARCH.log \
  | grep -viE 'File not found.*rm|rm:.*not found|File exists.*mkdir' | head

# --- 4. verify the binary landed ----------------------------------------------
echo "=== [$ARCH] verify minio in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
debugfs -R "stat /usr/bin/minio" "$IMG" 2>/dev/null | grep -iE 'Inode|Mode' | head -1
debugfs -R "ls /usr/bin" "$IMG" 2>/dev/null | tr ' ' '\n' | grep -x minio
echo "[$ARCH] DONE -> $IMG"
