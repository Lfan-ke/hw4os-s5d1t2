#!/bin/bash
# prep-iverilog-rootfs.sh — build rootfs-<arch>-iverilog.img for the `iverilog-0`
# StarryOS stress case (#764 "verilog <!-- verilator, iverilog, gnumake -->").
#
# Injects a STATIC, musl-cross-built `vvp` (the Icarus Verilog runtime, v12_0) with
# the system VPI module ($display/$finish/$time/...) linked in directly — static
# musl binaries can't dlopen() the usual system.vpi, so it is embedded (see
# vpi/sys_table_static.c + the VVP_STATIC_SYSTEM patch to vvp/vpi_modules.cc).
# Also injects the pre-compiled `dut.vvp` (host `iverilog -g2012 tb.v dut.v`, then
# its :vpi_module directives rewritten to reference only "system") and the golden.
#
# Validates that a real Icarus Verilog simulation RUNS on StarryOS (4 arch) and
# prints output identical to the host golden (which iverilog AND verilator agree on).
# Debugfs-only injection (no mount/sync). Usage: bash prep-iverilog-rootfs.sh <arch>
set -e
ARCH="${1:-x86_64}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs under tmp/axbuild/rootfs/); materials ship alongside this script.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-iverilog.img
STAGE=/tmp/iverilog-stage-$ARCH
BIN=$DL/testbin/vvp-$ARCH
VVP=$DL/dut.vvp
GOLDEN=$DL/golden.txt

[ -f "$BASE" ]   || { echo "missing base $BASE"; exit 2; }
[ -f "$BIN" ]    || { echo "missing $BIN (run the vvp cross-compile)"; exit 2; }
[ -f "$VVP" ]    || { echo "missing $VVP"; exit 2; }
[ -f "$GOLDEN" ] || { echo "missing golden $GOLDEN"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs"; exit 2; }

echo "=== [$ARCH] stage vvp ($(du -h "$BIN" | cut -f1), static) + dut.vvp + golden ==="
rm -rf "$STAGE"; mkdir -p "$STAGE/usr/local/bin" "$STAGE/root"
cp "$BIN" "$STAGE/usr/local/bin/vvp"; chmod 0755 "$STAGE/usr/local/bin/vvp"
cp "$VVP" "$STAGE/root/dut.vvp"
cp "$GOLDEN" "$STAGE/root/iverilog-golden.txt"
file "$STAGE/usr/local/bin/vvp" | sed 's/,.*statically/  [static]/'

echo "=== [$ARCH] copy base -> $IMG (grow to 2G, never shrink) ==="
cp -f "$BASE" "$IMG"
cur=$(stat -c %s "$IMG"); [ "$cur" -lt $((2*1024*1024*1024)) ] && truncate -s 2G "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1 || true
resize2fs "$IMG" >/dev/null 2>&1 || true

echo "=== [$ARCH] inject via debugfs -w ==="
DBG=/tmp/iverilog-debugfs-$ARCH.cmds; : > "$DBG"
( cd "$STAGE" && find . -type d | sort ) | while read -r d; do rel="${d#./}"; [ -n "$rel" ] && [ "$rel" != "." ] && echo "mkdir /$rel" >> "$DBG"; done
( cd "$STAGE" && find . -type f | sort ) | while read -r f; do rel="${f#./}"; echo "rm /$rel" >> "$DBG"; echo "write $STAGE/$rel /$rel" >> "$DBG"; done
debugfs -w -f "$DBG" "$IMG" >/tmp/iverilog-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/iverilog-debugfs-$ARCH.log | grep -viE 'File not found.*rm|rm:.*not found|File exists.*mkdir' | head

e2fsck -f -y "$IMG" >/dev/null 2>&1 || true
debugfs -R "stat /usr/local/bin/vvp" "$IMG" 2>/dev/null | grep -iE 'Inode' | head -1
echo "[$ARCH] DONE -> $IMG"
