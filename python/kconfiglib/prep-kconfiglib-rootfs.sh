#!/bin/bash
# prep-kconfiglib-rootfs.sh — build rootfs-<arch>-kconfiglib.img for the
# `kconfiglib-0` StarryOS stress case (#764 python kconfiglib).
#
# kconfiglib is a single-file, fully-headless PURE-PYTHON Kconfig parser
# (kconfiglib.py v14.1.0; NO curses/tk at import). The whole case is just that
# one module dropped into the base python rootfs's site-packages, plus a synthetic
# Kconfig + invariant battery written at boot by the qemu toml's shell_init_cmd.
#
# CLOSURE STRATEGY (mirrors prep-celery-rootfs.sh):
#   * Base = rootfs-<arch>-python.img (Alpine musl python 3.12, ALL 4 arches).
#   * kconfiglib.py is pure-python, single-file -> the SAME source works on all
#     4 arches. Injected into the UNMOUNTED ext4 image with `debugfs -w`.
#
# WSL2 NOTE: a bare global `sync` D-state-deadlocks this host. This script uses
# `debugfs -w` (writes the UNMOUNTED ext4 image directly) so it NEVER mounts and
# NEVER calls sync -> no deadlock. It only touches the kconfiglib image it creates.
#
# Usage:
#   bash prep-kconfiglib-rootfs.sh <arch> [workspace_root]
#     arch in x86_64|aarch64|riscv64|loongarch64
#     workspace_root defaults to the main tgoskits checkout (so ${workspace}
#       resolves there); pass an alternate checkout root when running from one.
set -uo pipefail
ARCH="${1:?usage: prep-kconfiglib-rootfs.sh <arch> [workspace_root]}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs under tmp/axbuild/rootfs/); the single-file kconfiglib.py
# source ships alongside this script (./kconfiglib.py). No machine-specific absolute paths.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
WS="${2:-$ROOT}"

PYVER=3.12
SITE="/usr/lib/python${PYVER}/site-packages"
# single-file pure-python kconfiglib source (v14.1.0), co-located beside this script
SRC_FILE="${KCONFIGLIB_SRC:-$HERE/kconfiglib.py}"

# base python image comes from the SHARED main-repo rootfs dir (already built);
# the kconfiglib image is written into the WORKSPACE's own rootfs dir so
# ${workspace} resolves to it when xtask runs from the worktree.
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-python.img
IMG=$WS/tmp/axbuild/rootfs/rootfs-$ARCH-kconfiglib.img

[ -f "$BASE" ] || { echo "missing base $BASE (build the python rootfs first)"; exit 2; }
[ -f "$SRC_FILE" ] || { echo "missing kconfiglib source $SRC_FILE"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }
mkdir -p "$WS/tmp/axbuild/rootfs"

# --- 1. copy base image ---------------------------------------------------------
echo "=== [$ARCH] copy base -> $IMG ==="
cp -f "$BASE" "$IMG"

# --- 2. inject kconfiglib.py via debugfs (no mount/sync) ------------------------
echo "=== [$ARCH] inject kconfiglib.py into $IMG at $SITE ==="
DBG=/tmp/kc-debugfs-$ARCH.cmds
{
  echo "rm $SITE/kconfiglib.py"
  echo "write $SRC_FILE $SITE/kconfiglib.py"
  echo "quit"
} > "$DBG"
debugfs -w -f "$DBG" "$IMG" >/tmp/kc-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/kc-debugfs-$ARCH.log | grep -viE 'File not found.*rm|rm:.*not found' | head
rm -f "$DBG"

# --- 3. verify the module landed ------------------------------------------------
echo "=== [$ARCH] verify kconfiglib in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
if debugfs -R "stat $SITE/kconfiglib.py" "$IMG" 2>/dev/null | grep -q 'Inode'; then
  echo "  OK $SITE/kconfiglib.py"
else
  echo "  MISSING $SITE/kconfiglib.py"; exit 3
fi
echo "[$ARCH] DONE -> $IMG"
