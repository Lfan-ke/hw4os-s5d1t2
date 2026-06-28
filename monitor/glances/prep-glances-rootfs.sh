#!/bin/bash
# Build rootfs-<arch>-glances.img for the glances-0 stress test case (#764 "monitor"
# category: glances = a psutil-based Python system monitor; tui / client-server / web).
#
# Starts from the python runtime img (rootfs-<arch>-python.img, already has python3 +
# numpy + libstdc++ + openblas injected by prep-python-rootfs.sh), then adds the FULL
# glances + py3-psutil transitive closure (Alpine v3.23) extracted into /usr.
#
# glances 4.4.1-r1 (v3.23/community) + py3-psutil 7.1.3-r0 (v3.23/main). The closure is
# 49 apks (identical package set on all 4 arches). It includes the pure-python plugin
# stack glances needs even in HEADLESS one-shot mode (jinja2/markupsafe, requests/urllib3/
# idna/charset-normalizer/certifi, ujson, bottle, defusedxml, packaging, future, batinfo,
# the snmp/asn1/pycryptodome stack, docker-py/websocket-client) plus the native
# psutil C extension (_psutil_linux.abi3.so, per-arch). curses/ncursesw + readline come
# along for the TUI but are NOT exercised by the headless smoke. Several closure members
# overlap the python base (musl, python3, libstdc++, libffi, sqlite-libs, readline, ...);
# re-extracting them just overwrites identical files (same v3.23 versions) and is harmless.
# See ../SOURCES.md for the full dependency facts.
#
# NO source build: Alpine ships musl-native glances + py3-psutil + every dep for all 4
# arches (x86_64/aarch64/riscv64/loongarch64), verified 2026-05-24.
#
# Usage:
#   bash prep-glances-rootfs.sh <arch>          # default x86_64 (v3.23 / python 3.12)
#
# WSL2 caveat: NEVER bare global `sync` (D-state hang). We `umount` directly (per-fs
# flush). Does NOT run any QEMU test; only touches the glances.img it creates.
set -uo pipefail
ARCH="${1:-x86_64}"
PYBRANCH="${PYBRANCH:-v3.23}"          # v3.23 (py3.12) only; glances closure matches base
PW=360123
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs live under tmp/axbuild/rootfs/); the glances apk closure +
# apk-closure.py ship alongside this script under ./apks/ (Git LFS). No machine-specific paths.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE/apks"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-python.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-glances.img
MP=/tmp/glancesmnt-$ARCH
APKDIR=$DL/$ARCH
MIRROR="https://dl-cdn.alpinelinux.org/alpine"
sudo() { echo "$PW" | command sudo -S "$@" 2>/dev/null; }

[ -f "$BASE" ] || { echo "missing base $BASE (run python-apks/prep-python-rootfs.sh $ARCH first)"; exit 2; }
mkdir -p "$APKDIR"

if [ "$PYBRANCH" = "edge" ]; then PYVER=3.14; MAINB=edge; COMMB=edge
else PYVER=3.12; MAINB=$PYBRANCH; COMMB=$PYBRANCH; fi

# --- glances apk roots (closure resolved from APKINDEX; see SOURCES.md) ---
#   main:      py3-psutil (the native /proc reader)
#   community: glances (the monitor; pulls its whole pure-python plugin stack)
ROOTS_MAIN="py3-psutil"
ROOTS_COMM="glances"

# Full transitive closure via the self-contained APKINDEX resolver (this host has no
# `apk`). 49 apks.
RESOLVER="$DL/apk-closure.py"
fetch_all() {
  if command -v apk >/dev/null 2>&1; then
    echo "  [apk fetch -R] main: $ROOTS_MAIN ; community: $ROOTS_COMM"
    apk fetch -R --no-cache --repository "$MIRROR/$MAINB/main/$ARCH" --arch "$ARCH" -o "$APKDIR" $ROOTS_MAIN 2>/dev/null
    apk fetch -R --no-cache --repository "$MIRROR/$COMMB/community/$ARCH" --arch "$ARCH" -o "$APKDIR" $ROOTS_COMM 2>/dev/null
  else
    echo "  [apk-closure.py] resolving full glances closure (no host apk)"
    python3 "$RESOLVER" --arch "$ARCH" --out "$APKDIR" \
      --repo "$MAINB/main" $ROOTS_MAIN \
      --repo "$COMMB/community" $ROOTS_COMM
    rc=$?
    [ "$rc" -ge 2 ] && { echo "  RESOLVER FATAL ($rc)"; exit 2; }
    [ "$rc" = 1 ] && echo "  WARN: some apk downloads failed (see log above)"
  fi
}

echo "=== [$ARCH] ROUTE=$PYBRANCH (python $PYVER) : fetch glances apk closure into $APKDIR ==="
fetch_all

echo "=== [$ARCH] copy python img -> $IMG, resize to 3.5G ==="
cp -f "$BASE" "$IMG"
sudo truncate -s 3584M "$IMG"
sudo e2fsck -f -y "$IMG" >/dev/null 2>&1
sudo resize2fs "$IMG" >/dev/null 2>&1

echo "=== [$ARCH] mount + extract glances apks into /usr ==="
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
# WSL2 caveat (c): a heredoc/stdin -> `sudo tee` is CLOBBERED by `echo PW | sudo -S`
# stdin (the password feed eats the data and the file lands 0 bytes). Write a host temp
# then `sudo cp` it in (cp takes no stdin). The runtime TOML also re-writes this file, so
# this is belt-and-suspenders, but we keep the image self-consistent.
printf '/lib\n/usr/lib\n' > /tmp/ldmusl-$ARCH.path
sudo cp /tmp/ldmusl-$ARCH.path "$MP/etc/ld-musl-$ARCH.path"

echo "=== [$ARCH] verify glances + psutil present ==="
SP="$MP/usr/lib/python$PYVER/site-packages"
sudo sh -c "ls -d $SP/glances $SP/psutil 2>/dev/null"
sudo sh -c "ls $MP/usr/bin/glances 2>/dev/null"
sudo sh -c "ls $SP/psutil/_psutil_linux*.so 2>/dev/null"
echo "free: $(df -h "$MP" | tail -1 | awk '{print $4}')"

# WSL2: NEVER bare global `sync` (D-state hang). umount flushes this fs directly.
sudo umount "$MP" && echo "[$ARCH] DONE -> $IMG"
