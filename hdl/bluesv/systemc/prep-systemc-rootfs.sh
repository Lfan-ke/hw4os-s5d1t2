#!/bin/bash
# prep-systemc-rootfs.sh — build rootfs-<arch>-systemc.img for the `systemc-0`
# StarryOS stress case (#764 bluesv "system c" half).
#
# Injects a STATIC sc_sim binary: a SystemC testbench (sc_module + sc_clock +
# SC_METHOD/SC_THREAD + sc_signal + wait + sc_start) linked against a musl-cross-built
# static libsystemc (Accellera SystemC 2.3.4, SC_USE_PTHREADS coroutine backend,
# SC_LONG_64 patched for riscv64/loongarch64). Validates the SystemC discrete-event
# kernel RUNS on StarryOS (4 arch) with output identical to the host golden.
# Debugfs-only injection. Usage: bash prep-systemc-rootfs.sh <arch>
set -e
ARCH="${1:-x86_64}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs under tmp/axbuild/rootfs/); materials ship alongside this script.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-systemc.img
STAGE=/tmp/systemc-stage-$ARCH
BIN=$DL/testbin/sc_sim-$ARCH

[ -f "$BASE" ] || { echo "missing base $BASE"; exit 2; }
[ -f "$BIN" ]  || { echo "missing $BIN"; exit 2; }
[ -f "$DL/sc-golden.txt" ] || { echo "missing golden"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs"; exit 2; }

echo "=== [$ARCH] stage sc_sim ($(du -h "$BIN"|cut -f1), static) + golden ==="
rm -rf "$STAGE"; mkdir -p "$STAGE/usr/local/bin" "$STAGE/root"
cp "$BIN" "$STAGE/usr/local/bin/sc_sim"; chmod 0755 "$STAGE/usr/local/bin/sc_sim"
cp "$DL/sc-golden.txt" "$STAGE/root/sc-golden.txt"
file "$STAGE/usr/local/bin/sc_sim" | sed 's/,.*statically/  [static]/'

echo "=== [$ARCH] copy base -> $IMG (grow to 2G, never shrink) ==="
cp -f "$BASE" "$IMG"
cur=$(stat -c %s "$IMG"); [ "$cur" -lt $((2*1024*1024*1024)) ] && truncate -s 2G "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1 || true
resize2fs "$IMG" >/dev/null 2>&1 || true

echo "=== [$ARCH] inject via debugfs -w ==="
DBG=/tmp/systemc-debugfs-$ARCH.cmds; : > "$DBG"
( cd "$STAGE" && find . -type d | sort ) | while read -r d; do rel="${d#./}"; [ -n "$rel" ] && [ "$rel" != "." ] && echo "mkdir /$rel" >> "$DBG"; done
( cd "$STAGE" && find . -type f | sort ) | while read -r f; do rel="${f#./}"; echo "rm /$rel" >> "$DBG"; echo "write $STAGE/$rel /$rel" >> "$DBG"; done
debugfs -w -f "$DBG" "$IMG" >/tmp/systemc-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/systemc-debugfs-$ARCH.log | grep -viE 'File not found.*rm|rm:.*not found|File exists.*mkdir' | head
e2fsck -f -y "$IMG" >/dev/null 2>&1 || true
echo "[$ARCH] DONE -> $IMG"
