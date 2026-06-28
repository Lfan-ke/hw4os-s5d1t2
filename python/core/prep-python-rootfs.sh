#!/bin/bash
# Build rootfs-<arch>-python.img for the python-0 test case (native-extension
# CORRECTNESS: numpy / scikit-learn / opencv on musl-native CPython).
#
# Copies the alpine base img, injects the musl-native python3 + py3-numpy +
# py3-scikit-learn + py3-opencv closures (Alpine apks extracted into /usr), sets the
# musl loader path. NO source build: Alpine ships musl binaries for all 4 arches.
#
# Usage:
#   bash prep-python-rootfs.sh <arch>            # default x86_64, ROUTE A (v3.23, py3.12)
#   PYBRANCH=edge bash prep-python-rootfs.sh <arch>   # ROUTE B (edge, py3.14) -- see SOURCES.md §2/§8
#
# Two routes (see apks/SOURCES-python-apks.md):
#   ROUTE A (default): v3.23 = python 3.12.13, SAME branch as the base image -> no ABI risk.
#   ROUTE B (--edge ): edge  = python 3.14.3 (matches #764 "python 3.14"); MUST take the
#                      WHOLE native closure (incl. musl/libstdc++/libgfortran/openblas) from
#                      edge, or .so versions will not match the v3.23 base image.
#
# This script does NOT run any QEMU test and does NOT touch images other than the
# python.img it creates. It DOES download apks into ./<arch>/ if missing.
set -uo pipefail
ARCH="${1:-x86_64}"
PYBRANCH="${PYBRANCH:-v3.23}"          # v3.23 (py3.12, ROUTE A) | edge (py3.14, ROUTE B)
# sudo password for the loop-mount steps: set SUDO_PASS in the environment, or leave
# empty on a passwordless-sudo host. Never hardcode a credential in a delivered script.
PW="${SUDO_PASS:-}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs live under tmp/axbuild/rootfs/); the apk closure + apk-closure.py
# ship alongside this script under ./apks/ (Git LFS). No machine-specific absolute paths.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE/apks"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-python.img
MP=/tmp/pymnt-$ARCH
APKDIR=$DL/$ARCH
MIRROR="https://dl-cdn.alpinelinux.org/alpine"
sudo() { echo "$PW" | command sudo -S "$@" 2>/dev/null; }

[ -f "$BASE" ] || { echo "missing base $BASE"; exit 2; }
mkdir -p "$APKDIR"

# --- python.X version per branch (keep in sync with SOURCES.md §2/§3) ---
if [ "$PYBRANCH" = "edge" ]; then PYVER=3.14; MAINB=edge; COMMB=edge
else PYVER=3.12; MAINB=$PYBRANCH; COMMB=$PYBRANCH; fi

# Package set for python-0 (numpy + scikit-learn + opencv closures). We fetch each
# apk by resolving its transitive closure from APKINDEX with apk-tools if available,
# else download a documented explicit list. Simplest robust path: use `apk fetch -R`
# against an apk binary if present; otherwise pull the named roots and let the
# extract step's ld-musl path cover the libs (the closures are documented in
# SOURCES.md §4 for a fully-explicit fallback).
ROOTS_MAIN="python3 py3-pip"
ROOTS_COMM="py3-numpy py3-scikit-learn py3-opencv"

# Resolve the FULL transitive closure (musl/openblas/libgfortran/libstdc++ + every so:
# dep, incl. py3-opencv's libopencv_*/Qt6 stack) and download every apk. We use a
# self-contained APKINDEX resolver (apk-closure.py) because this host has no `apk`; the
# old curl fallback only grabbed the NAMED ROOTS and silently dropped the whole
# transitive closure, which would produce an un-importable python (missing .so).
# When `apk` IS present we still prefer it (apk fetch -R), but the resolver works without.
RESOLVER="$DL/apk-closure.py"
fetch_all() {
  if command -v apk >/dev/null 2>&1; then
    echo "  [apk fetch -R] main: $ROOTS_MAIN ; community: $ROOTS_COMM"
    apk fetch -R --no-cache --repository "$MIRROR/$MAINB/main/$ARCH" --arch "$ARCH" -o "$APKDIR" $ROOTS_MAIN 2>/dev/null
    apk fetch -R --no-cache --repository "$MIRROR/$COMMB/community/$ARCH" --arch "$ARCH" -o "$APKDIR" $ROOTS_COMM 2>/dev/null
  else
    echo "  [apk-closure.py] resolving full closure (no host apk)"
    python3 "$RESOLVER" --arch "$ARCH" --out "$APKDIR" \
      --repo "$MAINB/main" $ROOTS_MAIN \
      --repo "$COMMB/community" $ROOTS_COMM
    rc=$?
    [ "$rc" -ge 2 ] && { echo "  RESOLVER FATAL ($rc)"; exit 2; }
    [ "$rc" = 1 ] && echo "  WARN: some apk downloads failed (see log above)"
  fi
}

echo "=== [$ARCH] ROUTE=$PYBRANCH (python $PYVER) : fetch apk closures into $APKDIR ==="
fetch_all

echo "=== [$ARCH] copy base -> $IMG, resize to 3G ==="
cp -f "$BASE" "$IMG"
sudo truncate -s 3G "$IMG"
sudo e2fsck -f -y "$IMG" >/dev/null 2>&1
sudo resize2fs "$IMG" >/dev/null 2>&1

echo "=== [$ARCH] mount + extract apks into /usr ==="
mkdir -p "$MP"
sudo mount -o loop,rw "$IMG" "$MP" || { echo "mount FAIL"; exit 2; }
shopt -s nullglob
for apk in "$APKDIR"/*.apk; do
  name=$(basename "$apk")
  sudo tar xzf "$apk" -C "$MP" \
    --exclude='.PKGINFO' --exclude='.SIGN.*' --exclude='.pre-install' \
    --exclude='.post-install' --exclude='.pre-upgrade' --exclude='.post-upgrade' \
    --exclude='.trigger' 2>/dev/null && echo "  + $name" || echo "  ! $name (partial)"
done

echo "=== [$ARCH] write /etc/ld-musl-$ARCH.path ==="
printf '/lib\n/usr/lib\n' | sudo tee "$MP/etc/ld-musl-$ARCH.path" >/dev/null

echo "=== [$ARCH] verify python + native modules ==="
sudo ls -la "$MP/usr/bin/python3" 2>&1 | head -1
sudo sh -c "ls $MP/usr/lib/python$PYVER/site-packages 2>/dev/null | grep -iE 'numpy|sklearn|cv2|scipy'" 2>&1 | head
sudo ls "$MP/usr/lib/libopenblas"* "$MP/usr/lib/libgfortran"* 2>&1 | head
echo "free: $(df -h "$MP" | tail -1 | awk '{print $4}')"

sudo sync
sudo umount "$MP" && echo "[$ARCH] DONE -> $IMG"
