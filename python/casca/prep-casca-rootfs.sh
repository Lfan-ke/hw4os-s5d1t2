#!/bin/bash
# prep-casca-rootfs.sh — build rootfs-<arch>-casca.img for the `casca-0` StarryOS
# stress case (#764 python "casca").
#
# casca is a single PURE-PYTHON (py3-none-any) wheel with no extra deps, so the per-arch
# coverage comes entirely from the base musl CPython rootfs (rootfs-<arch>-python.img,
# Python 3.12). We `pip install --target` the casca wheel into the guest site-packages and
# drop casca_smoke.py at /opt, then debugfs-write the tree into the
# UNMOUNTED ext4 image (never mount, never sync -> WSL2 D-state-deadlock safe; same
# recipe used by the other prep scripts).
#
# Usage:  bash prep-casca-rootfs.sh <arch>   # x86_64|aarch64|riscv64|loongarch64
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs under tmp/axbuild/rootfs/); the pure-python casca wheel +
# casca_smoke.py ship alongside this script (./wheels/ + ./casca_smoke.py). No abs paths.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-python.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-casca.img
STAGE=/tmp/casca-stage-$ARCH
WHEELS=$DL/wheels
PYVER=3.12
# Install into a DEDICATED dir, NOT the base rootfs site-packages: the base python
# image already ships some of these (e.g. platformdirs 4.5.0), and layering our casca-wheels
# on top with `pip --target` leaves BOTH dist-info dirs -> version-conflict bugs. The
# test toml puts /opt/pytui FIRST on PYTHONPATH so our pinned versions win cleanly.
SP="opt/pytui"

[ -f "$BASE" ] || { echo "missing base $BASE (need rootfs-$ARCH-python.img)"; exit 2; }
[ -d "$WHEELS" ] || { echo "missing casca-wheels dir $WHEELS"; exit 2; }
[ -f "$DL/casca_smoke.py" ] || { echo "missing casca_smoke.py"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }

# --- 1. stage the pure-python casca wheel into the guest site-packages + the smoke ----
echo "=== [$ARCH] pip install casca wheel -> STAGE/$SP ==="
rm -rf "$STAGE"; mkdir -p "$STAGE/$SP" "$STAGE/opt"
python3 -m pip install --no-index --no-deps --find-links "$WHEELS" --target "$STAGE/$SP" \
  casca >/tmp/textual-pip-$ARCH.log 2>&1 \
  || { echo "  pip install FAIL"; tail -15 /tmp/textual-pip-$ARCH.log; exit 2; }
cp -f "$DL/casca_smoke.py" "$STAGE/opt/casca_smoke.py"
echo "  staged $(ls "$STAGE/$SP" | grep -c .) site-packages entries + casca_smoke.py"

# --- 2. copy base -> img, grow for headroom -----------------------------------
echo "=== [$ARCH] copy base -> $IMG (grow +512M) ==="
cp -f "$BASE" "$IMG"
SZ=$(stat -c%s "$IMG"); NEW=$(( SZ + 512*1024*1024 ))
truncate -s "$NEW" "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1
resize2fs "$IMG" >/dev/null 2>&1

# --- 3. debugfs-write the staged tree (no mount, no sync) ---------------------
echo "=== [$ARCH] inject tree into $IMG via debugfs -w ==="
DBG=/tmp/textual-debugfs-$ARCH.cmds
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
debugfs -w -f "$DBG" "$IMG" >/tmp/textual-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/textual-debugfs-$ARCH.log \
  | grep -viE 'File not found.*rm|rm:.*not found|File exists.*mkdir' | head

# --- 4. verify -----------------------------------------------------------------
e2fsck -f -y "$IMG" >/dev/null 2>&1
HAVE=$(debugfs -R "ls /$SP" "$IMG" 2>/dev/null | grep -c "casca")
echo "  casca entries in image site-packages: $HAVE"
debugfs -R "stat /opt/casca_smoke.py" "$IMG" 2>/dev/null | grep -iE 'Inode' | head -1
echo "[$ARCH] DONE -> $IMG"
