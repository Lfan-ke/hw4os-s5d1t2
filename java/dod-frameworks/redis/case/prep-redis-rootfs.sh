#!/bin/bash
# prep-redis-rootfs.sh — build rootfs-<arch>-redis.img by injecting redis-server
# + redis-cli (Alpine musl 8.8.0-r0 apk) into the alpine base rootfs.
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable: ROOT from env (your tgoskits checkout); redis apks co-located under ../apks.
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
APK="$HERE/../apks/$ARCH/redis.apk"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-redis.img
STAGE=/tmp/redis-stage-$ARCH

[ -f "$APK" ]  || { echo "missing $APK"; exit 2; }
[ -f "$BASE" ] || { echo "missing $BASE"; exit 2; }

echo "=== [$ARCH] copy base -> $IMG ==="
cp -f "$BASE" "$IMG"
echo "=== [$ARCH] extract apk -> $STAGE ==="
rm -rf "$STAGE"; mkdir -p "$STAGE"
tar -xzf "$APK" -C "$STAGE" 2>/dev/null
ls $STAGE/usr/bin/redis-server $STAGE/usr/bin/redis-cli >/dev/null

echo "=== [$ARCH] inject via debugfs -w (no mount/sync) ==="
DBG=/tmp/redis-debugfs-$ARCH.cmds
: > "$DBG"
for f in $STAGE/usr/bin/redis-server $STAGE/usr/bin/redis-cli; do
  n=$(basename $f)
  echo "rm /usr/bin/$n" >> "$DBG"
  echo "write $f /usr/bin/$n" >> "$DBG"
  echo "set_inode_field /usr/bin/$n mode 0100755" >> "$DBG"
done
debugfs -w -f "$DBG" "$IMG" >/tmp/redis-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/redis-debugfs-$ARCH.log | head

echo "=== [$ARCH] verify ==="
debugfs -R "stat /usr/bin/redis-server" "$IMG" 2>/dev/null | grep -iE "Size|Mode" | head -2
debugfs -R "stat /usr/bin/redis-cli"    "$IMG" 2>/dev/null | grep -iE "Size|Mode" | head -2
echo "[$ARCH] DONE -> $IMG"
