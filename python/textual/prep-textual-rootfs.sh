#!/bin/bash
# prep-textual-rootfs.sh — build rootfs-<arch>-textual.img for the `textual-0` StarryOS
# stress case (#764 python "textual <!-- tui support -->").
#
# Textual + Rich + their deps are PURE-PYTHON (py3-none-any wheels), so the per-arch
# coverage comes entirely from the base musl CPython rootfs (rootfs-<arch>-python.img,
# Python 3.12). We `pip install --target` the wheels into the guest site-packages and
# drop the host-authored textual_smoke.py at /opt, then debugfs-write the tree into the
# UNMOUNTED ext4 image (NEVER mount, NEVER sync -> WSL2 D-state-deadlock safe; canonical
# recipe shared with prep-neo4j/etcd).
#
# Usage:  bash prep-textual-rootfs.sh <arch>   # x86_64|aarch64|riscv64|loongarch64
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs under tmp/axbuild/rootfs/); the pure-python wheels +
# textual_smoke.py ship alongside this script (./wheels/ + ./textual_smoke.py). No abs paths.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-python.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-textual.img
STAGE=/tmp/textual-stage-$ARCH
WHEELS=$DL/wheels
PYVER=3.12
# Install into a DEDICATED dir, NOT the base rootfs site-packages: the base python
# image already ships some of these (e.g. platformdirs 4.5.0), and layering our wheels
# on top with `pip --target` leaves BOTH dist-info dirs -> version-conflict bugs. The
# test toml puts /opt/pytui FIRST on PYTHONPATH so our pinned versions win cleanly.
SP="opt/pytui"

[ -f "$BASE" ] || { echo "missing base $BASE (need rootfs-$ARCH-python.img)"; exit 2; }
[ -d "$WHEELS" ] || { echo "missing wheels dir $WHEELS"; exit 2; }
[ -f "$DL/textual_smoke.py" ] || { echo "missing textual_smoke.py"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }

# --- 1. stage pure-python wheels into the guest site-packages + the smoke ----
echo "=== [$ARCH] pip install textual wheels -> STAGE/$SP ==="
rm -rf "$STAGE"; mkdir -p "$STAGE/$SP" "$STAGE/opt"
python3 -m pip install --no-index --no-deps --find-links "$WHEELS" --target "$STAGE/$SP" \
  textual rich markdown-it-py mdit-py-plugins mdurl pygments typing-extensions \
  platformdirs linkify-it-py uc-micro-py >/tmp/textual-pip-$ARCH.log 2>&1 \
  || { echo "  pip install FAIL"; tail -15 /tmp/textual-pip-$ARCH.log; exit 2; }
cp -f "$DL/textual_smoke.py" "$STAGE/opt/textual_smoke.py"
echo "  staged $(ls "$STAGE/$SP" | grep -c .) site-packages entries + textual_smoke.py"

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
HAVE=$(debugfs -R "ls /$SP" "$IMG" 2>/dev/null | grep -c "textual")
echo "  textual entries in image site-packages: $HAVE"
debugfs -R "stat /opt/textual_smoke.py" "$IMG" 2>/dev/null | grep -iE 'Inode' | head -1
echo "[$ARCH] DONE -> $IMG"
