#!/bin/bash
# Build rootfs-<arch>-openwrt.img for the #764 "openwrt" category stress cases
# (dropbear-0 = SSH server, dnsmasq-0 = DNS/DHCP server). ONE shared image serves
# both cases.
#
# Starts from the Alpine musl base image (rootfs-<arch>-alpine.img, Alpine v3.23 —
# the SAME branch the openwrt apks are fetched from, so the musl ABI matches), then
# extracts the dropbear + dnsmasq apks + their full dependency closure into / :
#   dropbear  /usr/sbin/dropbear + /usr/bin/dropbearkey  (NEEDED: libz, libutmps, libc.musl)
#   dnsmasq   /usr/sbin/dnsmasq                          (NEEDED: libc.musl only)
#   zlib            -> libz.so.1            (dropbear)
#   utmps-libs      -> libutmps.so.0.1      (dropbear; utmp/wtmp records)
#   skalibs-libs    -> libskarnet.so.2.14   (utmps dep)
#   dnsmasq-common  -> /etc/dnsmasq.conf    (we run dnsmasq with explicit flags, not this)
#   musl/busybox/busybox-binsh already in the base (re-extract is harmless)
#
# Users/dirs: the apk .pre-install scripts (excluded by tar) would `adduser -S dnsmasq`.
# We instead run both daemons as ROOT (--user=root for dnsmasq, dropbear -R generates
# an ephemeral key as root) to avoid setuid()/getpwnam() of a service account — the same
# approach gateway-nginx used. We still add a dnsmasq passwd/group entry as a fallback.
# /etc/dropbear (host-key dir) is created here so dropbearkey can write into it.
#
# Uses the WSL2 rootfs-build workaround: write into a loop mount, then `umount` DIRECTLY
# (per-fs flush, rc=0). NEVER a bare global `sync` (it wedges in D-state on this host).
# The sudo() wrapper pipes the password into stdin, which clobbers a `tee` heredoc, so
# any file content is written to a host temp file first, then `sudo cp`-ed in.
#
# Usage:
#   bash prep-openwrt-rootfs.sh <arch>      # default x86_64
#
# Does NOT run any QEMU test; only touches the openwrt.img it creates.
set -uo pipefail
ARCH="${1:-x86_64}"
# Host sudo password for the `sudo -S` wrapper below — supply via env, never hardcode.
PW="${SUDO_PW:?set SUDO_PW to your host sudo password (used by the sudo -S wrapper)}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs under tmp/axbuild/rootfs/); apks ship alongside this script under ./apks/.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE/apks"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-openwrt.img
MP=/tmp/openwrtmnt-$ARCH
APKDIR=$DL/$ARCH
sudo() { echo "$PW" | command sudo -S "$@" 2>/dev/null; }

[ -f "$BASE" ] || { echo "missing base $BASE"; exit 2; }
[ -d "$APKDIR" ] || { echo "missing apk dir $APKDIR (run apk-closure.py first)"; exit 2; }

echo "=== [$ARCH] copy alpine base -> $IMG, resize to 1280M ==="
cp -f "$BASE" "$IMG"
sudo truncate -s 1280M "$IMG"
sudo e2fsck -f -y "$IMG" >/dev/null 2>&1
sudo resize2fs "$IMG" >/dev/null 2>&1

echo "=== [$ARCH] mount + extract dropbear/dnsmasq apk closure into / ==="
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

echo "=== [$ARCH] create runtime dirs + dnsmasq service account fallback ==="
# dropbear host-key dir; /run for pidfiles; /var/empty is dropbear's privsep chroot dir.
sudo mkdir -p "$MP/etc/dropbear" "$MP/run" "$MP/var/empty"
# Fallback dnsmasq user/group (the daemons are launched as root in the DoDs, so this is
# only a safety net if a flag path still calls getpwnam("dnsmasq")).
sudo grep -q "^dnsmasq:" "$MP/etc/passwd" 2>/dev/null || \
  sudo sh -c "echo 'dnsmasq:x:101:101:dnsmasq:/dev/null:/sbin/nologin' >> '$MP/etc/passwd'"
sudo grep -q "^dnsmasq:" "$MP/etc/group" 2>/dev/null || \
  sudo sh -c "echo 'dnsmasq:x:101:' >> '$MP/etc/group'"

echo "=== [$ARCH] write /etc/ld-musl-$ARCH.path (/lib + /usr/lib) ==="
LDP_TMP=$(mktemp); printf '/lib\n/usr/lib\n' > "$LDP_TMP"
sudo cp "$LDP_TMP" "$MP/etc/ld-musl-$ARCH.path"; rm -f "$LDP_TMP"

echo "=== [$ARCH] verify binaries + libs present ==="
sudo ls -la "$MP/usr/sbin/dropbear" "$MP/usr/bin/dropbearkey" "$MP/usr/sbin/dnsmasq" 2>&1 | head
sudo ls "$MP/usr/lib/"libz.so* "$MP/usr/lib/"libutmps.so* "$MP/usr/lib/"libskarnet.so* 2>&1 | head
echo "free: $(df -h "$MP" | tail -1 | awk '{print $4}')"

# WSL2 workaround: umount directly (per-fs flush). NEVER bare global `sync`.
sudo umount "$MP" && echo "[$ARCH] DONE -> $IMG"
