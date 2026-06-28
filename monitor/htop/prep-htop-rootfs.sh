#!/bin/bash
# prep-htop-rootfs.sh — build rootfs-<arch>-htop.img for the htop TUI test/delivery.
# htop is a pure C ncurses process viewer; it needs musl + libncursesw + terminfo, all
# already present in the glances rootfs (rootfs-<arch>-glances.img, Alpine python base).
# So we copy that rootfs and debugfs-inject just the htop binary from the Alpine apk
# (htop-3.4.1-r1, main). NEVER mount / NEVER sync (WSL2 D-state-deadlock safe).
#
# Usage:  bash prep-htop-rootfs.sh <arch>   # x86_64|aarch64|riscv64|loongarch64
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (where rootfs imgs live under tmp/axbuild/rootfs/); materials ship
# alongside this script under ./apks/ (Git LFS). No machine-specific absolute paths.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE/apks"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-glances.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-htop.img
STAGE=/tmp/htop-stage-$ARCH
APK=$DL/htop-$ARCH.apk

[ -f "$BASE" ] || { echo "missing base $BASE (need rootfs-$ARCH-glances.img)"; exit 2; }
[ -f "$APK" ]  || { echo "missing htop apk $APK"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs"; exit 2; }

# --- 1. extract htop binary (+ any /usr/share it ships) from the apk ----------
echo "=== [$ARCH] extract htop from $(basename "$APK") ==="
rm -rf "$STAGE"; mkdir -p "$STAGE"
# apk is a gzipped tar; the data segment holds usr/bin/htop etc.
tar xzf "$APK" -C "$STAGE" 2>/dev/null || { echo "  apk extract FAIL"; exit 2; }
[ -f "$STAGE/usr/bin/htop" ] || { echo "  htop binary not in apk"; find "$STAGE" -name htop; exit 2; }
# keep only the real payload dirs (drop .PKGINFO / .SIGN.* apk metadata)
rm -f "$STAGE"/.PKGINFO "$STAGE"/.SIGN.* 2>/dev/null
echo "  staged: $(cd "$STAGE" && find usr -type f | wc -l) files (htop + share)"

# --- 2. copy base -> img -------------------------------------------------------
echo "=== [$ARCH] copy base -> $IMG ==="
cp -f "$BASE" "$IMG"

# --- 3. debugfs-write the staged tree (no mount, no sync) ---------------------
DBG=/tmp/htop-debugfs-$ARCH.cmds
: > "$DBG"
( cd "$STAGE" && find . -type d | sort ) | while read -r d; do
  rel="${d#./}"; [ "$rel" = "." ] && continue
  echo "mkdir /$rel" >> "$DBG"
done
( cd "$STAGE" && find . -type f | sort ) | while read -r f; do
  rel="${f#./}"
  echo "rm /$rel"                >> "$DBG"
  echo "write $STAGE/$rel /$rel" >> "$DBG"
done
debugfs -w -f "$DBG" "$IMG" >/tmp/htop-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/htop-debugfs-$ARCH.log \
  | grep -viE 'File not found.*rm|rm:.*not found|File exists.*mkdir' | head

# --- 4. verify -----------------------------------------------------------------
e2fsck -f -y "$IMG" >/dev/null 2>&1
debugfs -R "stat /usr/bin/htop" "$IMG" 2>/dev/null | grep -iE 'Inode|Mode' | head -1 \
  && echo "[$ARCH] DONE -> $IMG" || echo "[$ARCH] htop MISSING in image"
