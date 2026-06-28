#!/bin/bash
# prep-verilog-rootfs.sh — build rootfs-<arch>-verilog.img for the `verilog-0`
# StarryOS stress case (#764 "verilog <!-- verilator, iverilog -->").
#
# Injects the Verilator-generated simulation of a comprehensive SystemVerilog
# design (top.sv: ALU + regfile + counter + FSM + generate + packed enums),
# cross-compiled with the musl toolchain (CGO-free C++, fully static) into an
# Alpine musl rootfs. Validates that a real Verilator sim binary RUNS on StarryOS
# (4 arch) and produces output identical to the host-verilated golden.
#
# Debugfs-only injection (no mount/sync). Usage: bash prep-verilog-rootfs.sh <arch>
set -e
ARCH="${1:-x86_64}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs under tmp/axbuild/rootfs/); materials ship alongside this script.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-verilog.img
STAGE=/tmp/verilog-stage-$ARCH
BIN=$DL/testbin/vsim-$ARCH
GOLDEN=$DL/golden.txt

[ -f "$BASE" ]   || { echo "missing base $BASE"; exit 2; }
[ -f "$BIN" ]    || { echo "missing $BIN (run the verilator cross-compile)"; exit 2; }
[ -f "$GOLDEN" ] || { echo "missing golden $GOLDEN"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs"; exit 2; }

echo "=== [$ARCH] stage vsim ($(du -h "$BIN" | cut -f1), static) + golden ==="
rm -rf "$STAGE"; mkdir -p "$STAGE/usr/local/bin" "$STAGE/root"
cp "$BIN" "$STAGE/usr/local/bin/vsim"; chmod 0755 "$STAGE/usr/local/bin/vsim"
cp "$GOLDEN" "$STAGE/root/verilog-golden.txt"
file "$STAGE/usr/local/bin/vsim" | sed 's/,.*statically/  [static]/'

echo "=== [$ARCH] copy base -> $IMG (grow to 2G, never shrink) ==="
cp -f "$BASE" "$IMG"
cur=$(stat -c %s "$IMG"); [ "$cur" -lt $((2*1024*1024*1024)) ] && truncate -s 2G "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1 || true
resize2fs "$IMG" >/dev/null 2>&1 || true

echo "=== [$ARCH] inject via debugfs -w ==="
DBG=/tmp/verilog-debugfs-$ARCH.cmds; : > "$DBG"
( cd "$STAGE" && find . -type d | sort ) | while read -r d; do rel="${d#./}"; [ -n "$rel" ] && [ "$rel" != "." ] && echo "mkdir /$rel" >> "$DBG"; done
( cd "$STAGE" && find . -type f | sort ) | while read -r f; do rel="${f#./}"; echo "rm /$rel" >> "$DBG"; echo "write $STAGE/$rel /$rel" >> "$DBG"; done
debugfs -w -f "$DBG" "$IMG" >/tmp/verilog-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/verilog-debugfs-$ARCH.log | grep -viE 'File not found.*rm|rm:.*not found|File exists.*mkdir' | head

e2fsck -f -y "$IMG" >/dev/null 2>&1 || true
debugfs -R "stat /usr/local/bin/vsim" "$IMG" 2>/dev/null | grep -iE 'Inode' | head -1
echo "[$ARCH] DONE -> $IMG"
