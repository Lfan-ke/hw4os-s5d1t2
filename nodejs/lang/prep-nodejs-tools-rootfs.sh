#!/bin/bash
# Build rootfs-<arch>-nodejs-tools.img: the pure-JS node TOOLING tier of #764 nodejs that
# is NOT blocked by the V8 heavy-load mmap limitation (npm/vue/astro ARE blocked and are
# excluded). Start
# from the nodejs runtime img (node + .so deps closure injected by prep-nodejs-rootfs.sh),
# then add ONE shared vendored node_modules + four tiny tool projects under /proj/tools-suite:
#   eslint-proj  — lint a clean sample.js against a CommonJS flat config -> exit 0, 0 errors.
#   babel-proj   — @babel/cli transpile an ES2020 sample to stdout -> EXACT bytes vs REF.
#   terser-proj  — terser minify a sample to stdout -> EXACT bytes vs REF.
#   express-proj — start express on 127.0.0.1:PORT, loopback HTTP GET -> 200 + EXACT JSON.
# All four packages are PURE JS (no native addons), so the SAME node_modules runs on every
# arch via the working node runtime (/usr/bin/node, node v22.22.2). REF outputs were built
# on the host with the SAME node v22.22.2 so guest output is byte-comparable. See
# SOURCES-tools-suite.md.
#
# WSL2 GOTCHA: a bare global `sync` enters uninterruptible D-state and never returns on this
# host. We therefore NEVER call `sync`; the per-fs flush done by `umount` is sufficient.
#
# Usage: bash prep-nodejs-tools-rootfs.sh <arch>   (default x86_64)
#
# PREREQUISITES (run on host first):
#   1. prep-nodejs-rootfs.sh <arch>          -> rootfs-<arch>-nodejs.img (node runtime base)
#   2. "$NODEJS_FW/tools-suite" prepared: npm install (node_modules) + REF/ host outputs
#      (see SOURCES-tools-suite.md). 319 pkgs, ~50MB, 0 *.node addons.
#   3. icu-data-full-76.1-r1.apk present in apks/<arch>/ (real icudt76l.dat).
set -uo pipefail
ARCH="${1:-x86_64}"
# Host sudo password for the `sudo -S` wrapper below — supply via env, never hardcode.
PW="${SUDO_PW:?set SUDO_PW to your host sudo password (used by the sudo -S wrapper)}"
# Portable layout: TGOSKITS_ROOT = your tgoskits checkout; NODEJS_FW = host-vendored nodejs
# test material dir; apk closure ships under ./apks/<arch>/ (override via NODEJS_APKDIR).
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
TS="${NODEJS_FW:?set NODEJS_FW to the host-vendored nodejs test material directory}/tools-suite"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-nodejs.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-nodejs-tools.img
MP=/tmp/nodetoolsmnt-$ARCH
APKDIR="${NODEJS_APKDIR:-$HERE/apks/$ARCH}"
ICUAPK="$APKDIR/icu-data-full-76.1-r1.apk"
sudo() { echo "$PW" | command sudo -S "$@" 2>/dev/null; }

[ -f "$BASE" ] || { echo "missing base $BASE (run prep-nodejs-rootfs.sh first)"; exit 2; }
[ -d "$TS/node_modules" ] || { echo "missing tools-suite node_modules (npm install on host)"; exit 2; }
[ -d "$TS/REF" ] || { echo "missing tools-suite REF (host build first)"; exit 2; }
for d in eslint-proj babel-proj terser-proj express-proj; do
  [ -d "$TS/$d" ] || { echo "missing tools-suite/$d"; exit 2; }
done

echo "=== [$ARCH] copy nodejs img -> $IMG, resize to 3G ==="
cp -f "$BASE" "$IMG"
sudo truncate -s 3G "$IMG"
sudo e2fsck -f -y "$IMG" >/dev/null 2>&1
sudo resize2fs "$IMG" >/dev/null 2>&1

echo "=== [$ARCH] mount ==="
mkdir -p "$MP"
sudo mount -o loop,rw "$IMG" "$MP" || { echo "mount FAIL"; exit 2; }

echo "=== [$ARCH] inject ICU data (real ~32MB icudt76l.dat) ==="
# Alpine's icu-libs ships only a 9KB STUB libicudata; the real locale tables live in
# icu-data-full at /usr/share/icu/76.1/icudt76l.dat. Without it any Intl/Collator path
# throws "RangeError: Internal error. Icu error." (NOT a kernel bug). Mirror prep-nodejs-pm.
if [ -f "$ICUAPK" ]; then
  sudo tar xzf "$ICUAPK" -C "$MP" \
    --exclude='.PKGINFO' --exclude='.SIGN.*' --exclude='.pre-install' \
    --exclude='.post-install' --exclude='.pre-upgrade' --exclude='.post-upgrade' \
    --exclude='.trigger' 2>/dev/null
  sudo ls -la "$MP/usr/share/icu/76.1/icudt76l.dat" 2>&1 | head -1
else
  echo "WARN: missing $ICUAPK (Intl/ICU collation may fail)"
fi

echo "=== [$ARCH] inject /proj/tools-suite (shared node_modules + 4 tool projects + REF) ==="
sudo mkdir -p "$MP/proj"
sudo rm -rf "$MP/proj/tools-suite"
sudo mkdir -p "$MP/proj/tools-suite"
# rsync the whole suite: package.json + node_modules + the 4 *-proj dirs + REF.
sudo rsync -a \
  "$TS/package.json" \
  "$TS/node_modules" \
  "$TS/eslint-proj" "$TS/babel-proj" "$TS/terser-proj" "$TS/express-proj" \
  "$TS/REF" \
  "$MP/proj/tools-suite/"

# express resolves its deps via createRequire(__dirname/package-anchor.js); make the anchor
# resolve to the shared node_modules by giving express-proj its own package.json shim that
# points node's resolver at the parent node_modules (express-proj has no node_modules of its
# own, so resolution walks up to /proj/tools-suite/node_modules). The anchor file just needs
# to exist as a path base; create an empty one.
sudo sh -c "echo '// resolver anchor (empty)' > $MP/proj/tools-suite/express-proj/package-anchor.js"

echo "=== [$ARCH] verify ==="
sudo ls -d "$MP/proj/tools-suite/node_modules/eslint" "$MP/proj/tools-suite/node_modules/@babel/core" "$MP/proj/tools-suite/node_modules/terser" "$MP/proj/tools-suite/node_modules/express" 2>&1
sudo ls "$MP/proj/tools-suite/REF/" 2>&1
sudo sh -c "find $MP/proj/tools-suite/node_modules -name '*.node' | head" 2>&1
echo "free: $(df -h "$MP" | tail -1 | awk '{print $4}')"

# NO global sync (WSL2 D-state hang). umount does its own per-fs flush and returns rc=0.
sudo umount "$MP" && echo "[$ARCH] DONE -> $IMG"
