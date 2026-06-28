#!/bin/bash
# prep-celery-rootfs.sh — build rootfs-<arch>-celery.img for the `celery-0`
# StarryOS stress case (#764 python `celery <!-- flower -->`):
#   REAL cross-process celery (worker as a SEPARATE process from the producer)
#   over the kombu `filesystem://` broker (dir-based, zero external server), plus
#   flower (celery's tornado web monitor) starting up.
#
# CLOSURE STRATEGY (see this directory's SOURCES / the download cache PROVENANCE):
#   * Base = rootfs-<arch>-python.img (Alpine musl python 3.12.13 + pip, ALL 4 arches).
#   * celery 5.5.3 + flower 2.0.1 + 18 deps are ALL pure-python `py3-none-any` /
#     `py2.py3-none-any` wheels -> ONE wheel set, 4-arch universal. Installed with
#     host `pip install --target` into a staging tree (offline, --no-deps,
#     --no-index), then the staging tree is injected into the UNMOUNTED ext4 image
#     with `debugfs -w`.
#   * tornado 6.5.5 (flower's web server) is the ONLY package with a native part
#     (`tornado/speedups.abi3.so`, OPTIONAL — pure-python fallback exists):
#       - x86_64 / aarch64 : PyPI musllinux wheel (has the native .so, musl-native).
#       - riscv64 / loongarch64 : NO musllinux wheel -> use the sdist's pure-python
#         `tornado/` tree (no .so; tornado auto-falls-back to pure python) + reuse the
#         arch-independent wheel `.dist-info` for version metadata.
#
# WSL2 NOTE: a bare global `sync` D-state-deadlocks this host.
# This script uses `debugfs -w` (writes the UNMOUNTED ext4 image directly) so it
# NEVER mounts and NEVER calls sync -> no deadlock. It only touches the celery
# image it creates (in the given workspace's tmp/axbuild/rootfs).
#
# Usage:
#   bash prep-celery-rootfs.sh <arch> [workspace_root]
#     arch in x86_64|aarch64|riscv64|loongarch64
#     workspace_root defaults to TGOSKITS_ROOT (so ${workspace} resolves there)
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs under tmp/axbuild/rootfs/); the pure-python celery/flower
# wheels + tornado musllinux wheels + tornado sdist ship alongside this script
# (./wheels/ + ./tornado-musllinux/ + ./tornado-sdist/). No machine-specific absolute paths.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
WS="${2:-$ROOT}"
DL="$HERE"
WHEELS=$DL/wheels
PYVER=3.12
# base python image comes from the SHARED main-repo rootfs dir (already built);
# the celery image is written into the WORKSPACE's own rootfs dir so ${workspace}
# resolves to it when xtask runs from the worktree.
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-python.img
IMG=$WS/tmp/axbuild/rootfs/rootfs-$ARCH-celery.img
STAGE=/tmp/cb-stage-$ARCH

[ -f "$BASE" ] || { echo "missing base $BASE"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }
mkdir -p "$WS/tmp/axbuild/rootfs"

# --- 1. stage the pure-python celery/flower closure via host pip --target -------
echo "=== [$ARCH] stage celery+flower closure into $STAGE/usr/lib/python$PYVER/site-packages ==="
rm -rf "$STAGE"
SP="$STAGE/usr/lib/python$PYVER/site-packages"
mkdir -p "$SP"
python3 -m pip install --no-index --no-deps --find-links "$WHEELS" --target "$SP" \
  celery flower kombu billiard amqp vine \
  click click-didyoumean click-plugins click-repl \
  prometheus-client prompt-toolkit python-dateutil six tzdata pytz humanize packaging wcwidth \
  2>&1 | tail -3

# --- 2. tornado per-arch (native wheel on x86/aarch, pure-py sdist on riscv/loong) -
echo "=== [$ARCH] install tornado ==="
case "$ARCH" in
  x86_64|aarch64)
    TW=$(ls "$DL/tornado-musllinux/$ARCH/"tornado-*.whl 2>/dev/null | head -1)
    [ -n "$TW" ] || { echo "  missing tornado musllinux wheel for $ARCH"; exit 2; }
    ( cd "$SP" && unzip -q -o "$TW" )
    echo "  + $(basename "$TW") (native musl .so)"
    ;;
  riscv64|loongarch64)
    # pure-python sdist tree (no speedups.so -> tornado auto-falls-back to pure py),
    # plus the arch-independent .dist-info from the x86_64 wheel for version metadata.
    TS=$DL/tornado-sdist/tornado-6.5.5.tar.gz
    TWREF=$(ls "$DL/tornado-musllinux/x86_64/"tornado-*.whl | head -1)
    TMP=/tmp/cb-tornado-$ARCH; rm -rf "$TMP"; mkdir -p "$TMP"
    tar xzf "$TS" -C "$TMP"
    cp -a "$TMP/tornado-6.5.5/tornado" "$SP/tornado"
    rm -f "$SP/tornado/speedups.abi3.so" 2>/dev/null   # no native part on this arch
    # reuse arch-independent dist-info (METADATA/RECORD) so flower sees tornado's version
    ( cd "$SP" && unzip -q -o "$TWREF" 'tornado-*.dist-info/*' )
    rm -rf "$TMP"
    echo "  + tornado sdist pure-python tree (no native speedups; pure-py fallback)"
    ;;
  *) echo "  unknown arch $ARCH"; exit 2 ;;
esac

# musl loader search path for the guest (mirrors python-0 / jupyter)
printf '/lib\n/usr/lib\n' > "$STAGE/etc-ld-musl-$ARCH.path"

# strip host pip bytecode caches (they reference host paths; guest recompiles)
find "$STAGE" -name '__pycache__' -type d -prune -exec rm -rf {} + 2>/dev/null
find "$STAGE" -name '*.pyc' -delete 2>/dev/null

# --- 3. copy base image, grow to 3.5G (closure is small, base already 3G) -------
echo "=== [$ARCH] copy base -> $IMG (resize 3.5G) ==="
cp -f "$BASE" "$IMG"
truncate -s 3584M "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1
resize2fs "$IMG" >/dev/null 2>&1

# --- 4. inject the staged tree into the UNMOUNTED ext4 via debugfs (no mount/sync) -
echo "=== [$ARCH] inject tree into $IMG via debugfs -w ==="
DBG=/tmp/cb-debugfs-$ARCH.cmds
: > "$DBG"
( cd "$STAGE" && find . -type d | sort ) | while read -r d; do
  rel="${d#./}"; [ "$rel" = "." ] && continue
  echo "mkdir /$rel" >> "$DBG"
done
( cd "$STAGE" && find . -type f | sort ) | while read -r f; do
  rel="${f#./}"
  echo "rm /$rel" >> "$DBG"
  echo "write $STAGE/$rel /$rel" >> "$DBG"
done
( cd "$STAGE" && find . -type l | sort ) | while read -r l; do
  rel="${l#./}"; tgt=$(readlink "$STAGE/$rel")
  echo "rm /$rel" >> "$DBG"
  echo "symlink /$rel $tgt" >> "$DBG"
done
echo "rm /etc/ld-musl-$ARCH.path" >> "$DBG"
echo "write $STAGE/etc-ld-musl-$ARCH.path /etc/ld-musl-$ARCH.path" >> "$DBG"

debugfs -w -f "$DBG" "$IMG" >/tmp/cb-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/cb-debugfs-$ARCH.log | grep -viE 'File not found.*rm|rm:.*not found' | head

# --- 5. verify the celery/flower stack landed ----------------------------------
echo "=== [$ARCH] verify celery stack in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
debugfs -R "stat /usr/bin/python3" "$IMG" 2>/dev/null | head -1
for m in celery/__init__.py kombu/__init__.py flower/__init__.py tornado/__init__.py billiard/__init__.py; do
  debugfs -R "stat /usr/lib/python$PYVER/site-packages/$m" "$IMG" 2>/dev/null | grep -q 'Inode' \
    && echo "  OK $m" || echo "  MISSING $m"
done
# kombu filesystem transport (the broker we use)
debugfs -R "stat /usr/lib/python$PYVER/site-packages/kombu/transport/filesystem.py" "$IMG" 2>/dev/null | grep -q 'Inode' \
  && echo "  OK kombu/transport/filesystem.py (filesystem:// broker)" || echo "  MISSING kombu filesystem transport"
echo "free: $(dumpe2fs -h "$IMG" 2>/dev/null | awk -F: '/Free blocks/{print $2}' | head -1) free-blocks"
echo "[$ARCH] DONE -> $IMG"
