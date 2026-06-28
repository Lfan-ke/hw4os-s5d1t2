#!/bin/bash
# Build rootfs-<arch>-nodejs.img: copy the alpine base img, inject musl-native
# nodejs + its runtime-deps closure (apks extracted into /usr), set ld-musl path.
# Usage: bash prep-nodejs-rootfs.sh <arch>   (default x86_64)
set -uo pipefail
ARCH="${1:-x86_64}"
# Host sudo password for the `sudo -S` wrapper below — supply via env, never hardcode.
PW="${SUDO_PW:?set SUDO_PW to your host sudo password (used by the sudo -S wrapper)}"
# Portable layout: TGOSKITS_ROOT = your tgoskits checkout (rootfs imgs live under
# tmp/axbuild/rootfs/); the node apk closure ships alongside this script under ./apks/<arch>/
# (override with NODEJS_APKDIR if you keep it elsewhere). No machine-specific absolute paths.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-nodejs.img
MP=/tmp/nodemnt-$ARCH
APKDIR="${NODEJS_APKDIR:-$HERE/apks/$ARCH}"
sudo() { echo "$PW" | command sudo -S "$@" 2>/dev/null; }

[ -f "$BASE" ] || { echo "missing base $BASE"; exit 2; }
[ -d "$APKDIR" ] || { echo "missing apk dir $APKDIR"; exit 2; }

echo "=== [$ARCH] copy base -> $IMG, resize to 2G ==="
cp -f "$BASE" "$IMG"
cursize=$(stat -c %s "$IMG")
if [ "$cursize" -lt 2000000000 ]; then
  sudo truncate -s 2G "$IMG"
  sudo e2fsck -f -y "$IMG" >/dev/null 2>&1
  sudo resize2fs "$IMG" >/dev/null 2>&1
fi

echo "=== [$ARCH] mount + extract apks into /usr ==="
mkdir -p "$MP"
sudo mount -o loop,rw "$IMG" "$MP" || { echo "mount FAIL"; exit 2; }

for apk in "$APKDIR"/*.apk; do
  name=$(basename "$apk")
  # apk is gzip-tar; payload lives under usr/ lib/ etc. Skip apk metadata members
  # (.PKGINFO, .SIGN.*, .pre-install, ...). Extract everything else into the rootfs.
  sudo tar xzf "$apk" -C "$MP" \
    --exclude='.PKGINFO' --exclude='.SIGN.*' --exclude='.pre-install' \
    --exclude='.post-install' --exclude='.pre-upgrade' --exclude='.post-upgrade' \
    --exclude='.trigger' 2>/dev/null && echo "  + $name" || echo "  ! $name (partial)"
done

echo "=== [$ARCH] write /etc/ld-musl-$ARCH.path ==="
printf '/lib\n/usr/lib\n' | sudo tee "$MP/etc/ld-musl-$ARCH.path" >/dev/null

echo "=== [$ARCH] verify node ==="
sudo ls -la "$MP/usr/bin/node" 2>&1 | head -1
sudo ls "$MP/usr/lib/libstdc++.so.6" "$MP/usr/lib/libicuuc.so.76" "$MP/usr/lib/libcares.so.2" 2>&1
echo "free: $(df -h "$MP" | tail -1 | awk '{print $4}')"

sudo sync
sudo umount "$MP" && echo "[$ARCH] DONE -> $IMG"
