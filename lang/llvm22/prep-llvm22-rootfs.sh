#!/bin/bash
# prep-llvm22-rootfs.sh — rootfs-<arch>-llvm22.img for the `llvm22-0` StarryOS stress
# case (#764 "llvm22"). Injects a STATIC binary cross-compiled by clang-22 (LLVM
# 22.1.6) from a comprehensive C++23 program (ranges/concepts/consteval/variant/bit/
# optional), validating LLVM-22 codegen runs on StarryOS 4-arch with output identical
# to the host golden. Debugfs-only. Usage: bash prep-llvm22-rootfs.sh <arch>
set -e
ARCH="${1:-x86_64}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs live under tmp/axbuild/rootfs/); the llvm22 testbin/ binaries
# and golden.txt ship alongside this script. No machine-specific absolute paths.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-llvm22.img
STAGE=/tmp/llvm22-stage-$ARCH
BIN=$DL/testbin/llvm22-$ARCH
[ -f "$BASE" ] || { echo "missing base $BASE"; exit 2; }
[ -f "$BIN" ]  || { echo "missing $BIN"; exit 2; }
[ -f "$DL/golden.txt" ] || { echo "missing golden"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs"; exit 2; }

echo "=== [$ARCH] stage llvm22 ($(du -h "$BIN"|cut -f1), static) + golden ==="
rm -rf "$STAGE"; mkdir -p "$STAGE/usr/local/bin" "$STAGE/root"
cp "$BIN" "$STAGE/usr/local/bin/llvm22test"; chmod 0755 "$STAGE/usr/local/bin/llvm22test"
cp "$DL/golden.txt" "$STAGE/root/llvm22-golden.txt"
file "$STAGE/usr/local/bin/llvm22test" | sed 's/,.*statically/  [static]/'

echo "=== [$ARCH] copy base -> $IMG (grow to 2G, never shrink) ==="
cp -f "$BASE" "$IMG"
cur=$(stat -c %s "$IMG"); [ "$cur" -lt $((2*1024*1024*1024)) ] && truncate -s 2G "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1 || true
resize2fs "$IMG" >/dev/null 2>&1 || true

echo "=== [$ARCH] inject via debugfs -w ==="
DBG=/tmp/llvm22-debugfs-$ARCH.cmds; : > "$DBG"
( cd "$STAGE" && find . -type d | sort ) | while read -r d; do rel="${d#./}"; [ -n "$rel" ] && [ "$rel" != "." ] && echo "mkdir /$rel" >> "$DBG"; done
( cd "$STAGE" && find . -type f | sort ) | while read -r f; do rel="${f#./}"; echo "rm /$rel" >> "$DBG"; echo "write $STAGE/$rel /$rel" >> "$DBG"; done
debugfs -w -f "$DBG" "$IMG" >/tmp/llvm22-debugfs-$ARCH.log 2>&1
e2fsck -f -y "$IMG" >/dev/null 2>&1 || true
echo "[$ARCH] DONE -> $IMG"
