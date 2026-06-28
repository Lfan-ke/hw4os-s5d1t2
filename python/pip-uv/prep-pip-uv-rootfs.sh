#!/bin/bash
# Build rootfs-<arch>-pip314.img for the pip-uv carpet test by injecting the LATEST
# pip (26.1.2) + uv (0.11.19) + offline build wheels into a CPython 3.14 musl base.
#
# All packages are pre-downloaded (no apt/dpkg/apk, no network at build time):
#   pip wheel + 4-arch uv binaries -> ../../../download/pip-uv/  (see PROVENANCE.md)
#   setuptools/wheel/packaging/six offline wheels -> $WHEELS_SRC (staged into guest /opt/wheels)
# uv loongarch64 is the Alpine edge community apk (astral-sh ships no loong binary).
#
# Usage:  bash prep-pip-uv-rootfs.sh <arch>      # arch in: x86_64 aarch64 riscv64 loongarch64
set -uo pipefail
ARCH="${1:?usage: prep-pip-uv-rootfs.sh <x86_64|aarch64|riscv64|loongarch64>}"
# sudo password for the loop-mount steps: set SUDO_PASS in the environment, or leave
# empty on a passwordless-sudo host. Never hardcode a credential in a delivered script.
PW="${SUDO_PASS:-}"
sudo() { echo "$PW" | command sudo -S "$@" 2>/dev/null; }
HERE=$(cd "$(dirname "$0")" && pwd)
DL="$HERE/../../../download/pip-uv"          # pip wheel + uv binaries (+PROVENANCE.md)
ROOT="${TGOSKITS_ROOT:-$HOME/tgoskits}"      # tgoskits checkout (override via TGOSKITS_ROOT)
ROOTFS="$ROOT/tmp/axbuild/rootfs"
BASE="$ROOTFS/rootfs-$ARCH-python314.img"    # CPython 3.14 musl base (built upstream/prior)
IMG="$ROOTFS/rootfs-$ARCH-pip314.img"
WHEELS_SRC="${WHEELS_SRC:-$HERE/offline-wheels}"  # setuptools/wheel/packaging/six (override via WHEELS_SRC)
MP="/tmp/pipuvmnt-$ARCH"
PIPWHL="$DL/pip-26.1.2-py3-none-any.whl"

# resolve the uv binary for this arch (astral static musl for x86/aa/rv; Alpine apk for loong)
case "$ARCH" in
  x86_64)      UVTGZ="$DL/uv-x86_64-unknown-linux-musl.tar.gz";  UVSUB="uv-x86_64-unknown-linux-musl/uv" ;;
  aarch64)     UVTGZ="$DL/uv-aarch64-unknown-linux-musl.tar.gz"; UVSUB="uv-aarch64-unknown-linux-musl/uv" ;;
  riscv64)     UVTGZ="$DL/uv-riscv64gc-unknown-linux-musl.tar.gz"; UVSUB="uv-riscv64gc-unknown-linux-musl/uv" ;;
  loongarch64) UVAPK="$DL/uv-loongarch64-uv-0.11.19-r0.apk" ;;
  *) echo "unknown arch $ARCH"; exit 2 ;;
esac

[ -f "$BASE" ] || { echo "missing base $BASE (build the python3.14 base rootfs first)"; exit 2; }
[ -f "$PIPWHL" ] || { echo "missing $PIPWHL — run download (see PROVENANCE.md / dl_latest.sh)"; exit 2; }

echo "[$ARCH] copy base -> $IMG"
cp -f "$BASE" "$IMG"

# extract the uv binary to a temp file
UVBIN="/tmp/uv-$ARCH.bin"; rm -f "$UVBIN"
if [ "$ARCH" = loongarch64 ]; then
  rm -rf /tmp/uvapk-$ARCH && mkdir -p /tmp/uvapk-$ARCH
  tar xzf "$UVAPK" -C /tmp/uvapk-$ARCH 2>/dev/null
  find /tmp/uvapk-$ARCH -name uv -type f -exec cp {} "$UVBIN" \;
else
  rm -rf /tmp/uvtgz-$ARCH && mkdir -p /tmp/uvtgz-$ARCH
  tar xzf "$UVTGZ" -C /tmp/uvtgz-$ARCH
  cp "/tmp/uvtgz-$ARCH/$UVSUB" "$UVBIN"
fi
[ -s "$UVBIN" ] || { echo "failed to extract uv for $ARCH"; exit 2; }

echo "[$ARCH] mount + inject pip wheel / uv / offline wheels"
mkdir -p "$MP"
sudo e2fsck -f -y "$IMG" >/dev/null 2>&1
sudo mount -o loop,rw "$IMG" "$MP" || { echo "mount FAIL"; exit 2; }
# pip: replace the ensurepip bundled wheel so the carpet bootstraps pip 26.1.2
sudo rm -f "$MP"/usr/lib/python3.14/ensurepip/_bundled/pip-*.whl
sudo cp "$PIPWHL" "$MP/usr/lib/python3.14/ensurepip/_bundled/"
# uv: drop the binary into /usr/bin
sudo cp "$UVBIN" "$MP/usr/bin/uv"; sudo chmod 0755 "$MP/usr/bin/uv"
# offline build backends for the carpet ($WHEELS=/opt/wheels)
sudo mkdir -p "$MP/opt/wheels"
[ -d "$WHEELS_SRC" ] && sudo cp "$WHEELS_SRC"/*.whl "$MP/opt/wheels/" 2>/dev/null
sudo sync
sudo umount "$MP" && echo "[$ARCH] DONE -> $IMG  (pip 26.1.2 + uv 0.11.19 + /opt/wheels)"
