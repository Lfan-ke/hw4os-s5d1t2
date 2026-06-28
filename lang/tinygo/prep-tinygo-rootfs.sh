#!/bin/bash
# prep-tinygo-rootfs.sh — rootfs-<arch>-tinygo.img for `tinygo-0` (#764 tinygo).
# Injects a static TinyGo-compiled Go binary (goroutines/channels/select/atomic/
# generics/closures/defer). Currently aarch64 (static). x86_64 default is dynamic-glibc
# (won't run on musl); riscv64/loong64 unsupported by TinyGo upstream. Usage: <arch>
set -e
ARCH="${1:-aarch64}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs live under tmp/axbuild/rootfs/); the tinygo testbin/ binaries
# and golden.txt ship alongside this script. No machine-specific absolute paths.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-tinygo.img
STAGE=/tmp/tinygo-stage-$ARCH
BIN=$DL/testbin/tinygo-$ARCH
[ -f "$BASE" ] || { echo "missing base $BASE"; exit 2; }
[ -f "$BIN" ]  || { echo "missing $BIN"; exit 2; }
rm -rf "$STAGE"; mkdir -p "$STAGE/usr/local/bin" "$STAGE/root"
cp "$BIN" "$STAGE/usr/local/bin/tinygotest"; chmod 0755 "$STAGE/usr/local/bin/tinygotest"
cp "$DL/golden.txt" "$STAGE/root/tinygo-golden.txt"
cp -f "$BASE" "$IMG"
cur=$(stat -c %s "$IMG"); [ "$cur" -lt $((2*1024*1024*1024)) ] && truncate -s 2G "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1 || true; resize2fs "$IMG" >/dev/null 2>&1 || true
DBG=/tmp/tinygo-debugfs-$ARCH.cmds; : > "$DBG"
( cd "$STAGE" && find . -type d|sort)|while read -r d; do rel="${d#./}"; [ -n "$rel" ]&&[ "$rel" != "." ]&&echo "mkdir /$rel">>"$DBG"; done
( cd "$STAGE" && find . -type f|sort)|while read -r f; do rel="${f#./}"; echo "rm /$rel">>"$DBG"; echo "write $STAGE/$rel /$rel">>"$DBG"; done
debugfs -w -f "$DBG" "$IMG" >/tmp/tinygo-debugfs-$ARCH.log 2>&1
e2fsck -f -y "$IMG" >/dev/null 2>&1 || true
echo "[$ARCH] DONE -> $IMG"
