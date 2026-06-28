#!/bin/bash
# prep-bluesv-rootfs.sh — build rootfs-<arch>-bluesv.img for the `bluesv-0`
# StarryOS stress case (#764 "bluesv <!-- bluespec systemverilog, system c -->").
#
# Injects the STATIC vvp (Icarus runtime, VCD-enabled, system VPI embedded) + the
# Verilog that the Bluespec compiler (bsc) generated from Tb.bsv (compiled to
# bluesv.vvp via `bsc -verilog -e mkTb -vsim iverilog`, then made portable). On
# StarryOS the bsc-generated design is simulated by vvp: it must run to completion,
# match the host golden, AND emit a VCD waveform (dump.vcd) — i.e. iverilog replaces
# bluesim as the simulator, and the generated waveform is verified.
# Debugfs-only injection. Usage: bash prep-bluesv-rootfs.sh <arch>
set -e
ARCH="${1:-x86_64}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs under tmp/axbuild/rootfs/); materials ship alongside this script.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-bluesv.img
STAGE=/tmp/bluesv-stage-$ARCH
BIN=$DL/testbin/vvp-$ARCH

[ -f "$BASE" ] || { echo "missing base $BASE"; exit 2; }
[ -f "$BIN" ]  || { echo "missing $BIN"; exit 2; }
[ -f "$DL/bluesv.vvp" ] || { echo "missing bluesv.vvp"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs"; exit 2; }

echo "=== [$ARCH] stage VCD-vvp ($(du -h "$BIN" | cut -f1), static) + bluesv.vvp + golden ==="
rm -rf "$STAGE"; mkdir -p "$STAGE/usr/local/bin" "$STAGE/root"
cp "$BIN" "$STAGE/usr/local/bin/vvp"; chmod 0755 "$STAGE/usr/local/bin/vvp"
cp "$DL/bluesv.vvp"        "$STAGE/root/bluesv.vvp"
cp "$DL/bluesv-golden.txt" "$STAGE/root/bluesv-golden.txt"
file "$STAGE/usr/local/bin/vvp" | sed 's/,.*statically/  [static]/'

echo "=== [$ARCH] copy base -> $IMG (grow to 2G, never shrink) ==="
cp -f "$BASE" "$IMG"
cur=$(stat -c %s "$IMG"); [ "$cur" -lt $((2*1024*1024*1024)) ] && truncate -s 2G "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1 || true
resize2fs "$IMG" >/dev/null 2>&1 || true

echo "=== [$ARCH] inject via debugfs -w ==="
DBG=/tmp/bluesv-debugfs-$ARCH.cmds; : > "$DBG"
( cd "$STAGE" && find . -type d | sort ) | while read -r d; do rel="${d#./}"; [ -n "$rel" ] && [ "$rel" != "." ] && echo "mkdir /$rel" >> "$DBG"; done
( cd "$STAGE" && find . -type f | sort ) | while read -r f; do rel="${f#./}"; echo "rm /$rel" >> "$DBG"; echo "write $STAGE/$rel /$rel" >> "$DBG"; done
debugfs -w -f "$DBG" "$IMG" >/tmp/bluesv-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/bluesv-debugfs-$ARCH.log | grep -viE 'File not found.*rm|rm:.*not found|File exists.*mkdir' | head
e2fsck -f -y "$IMG" >/dev/null 2>&1 || true
echo "[$ARCH] DONE -> $IMG"
