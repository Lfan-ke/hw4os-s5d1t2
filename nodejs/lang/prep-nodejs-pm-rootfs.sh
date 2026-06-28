#!/bin/bash
# Build rootfs-<arch>-nodejs-pm.img: package-manager coverage (#764 nodejs = npm + yarn,
# the node analogue of java's maven + gradle). Start from the nodejs runtime img
# (node + .so deps closure already injected by prep-nodejs-rootfs.sh), then add:
#   - yarn (Yarn Classic v1.22.22, Alpine v3.22/community apk). PURE JS, zero native
#     addons -> the SAME apk's JS runs on any arch via node. Entry:
#     /usr/share/node_modules/yarn/bin/yarn.js (+ /usr/bin/yarn wrapper + symlink).
#   - npm: already bundled with node v22.22.2 at /usr/lib/node_modules/npm (pure JS);
#     we inject it the same way prep-nodejs-fw-rootfs.sh does so the npm path can run.
#   - /proj/pm-sample: a TINY pure-JS project (is-odd -> is-number, zero native deps)
#     shipped with BOTH offline sources so each PM can install with NO network:
#       * yarn:  yarn.lock + .yarnrc + .yarn-offline-mirror/*.tgz  (yarn install --offline)
#       * npm:   package-lock.json + .npm-cache/  (npm install --offline)
#     No node_modules is shipped — each PM materialises it from its offline source, which
#     is exactly what we want to test (the install action itself), then runs index.js.
#
# On StarryOS the nodejs-pm-0 test runs BOTH managers via DIRECT node-invoke (no shell
# wrapper, same pattern as the fw bundler invokes):
#   yarn: node /usr/share/node_modules/yarn/bin/yarn.js install --offline   (GATE)
#   npm:  node /usr/lib/node_modules/npm/bin/npm-cli.js install --offline   (non-gating)
# npm-cli currently SIGSEGVs on Starry x86_64 (rip=0x60164cdd, --jitless same address ->
# JIT-independent; a large-require-graph mmap/signal/TLS limitation). The test records that
# honestly and does NOT gate on it. yarn (smaller pure-JS program) is the gate.
#
# Usage: bash prep-nodejs-pm-rootfs.sh <arch>   (default x86_64)
#
# PREREQUISITES (run on host first):
#   1. prep-nodejs-rootfs.sh <arch>            -> rootfs-<arch>-nodejs.img (node runtime base)
#   2. yarn-1.22.22-r1.apk present in nodejs-apks/<arch>/ (apk fetch v3.22/community)
#   3. "$NODEJS_FW/pm-sample" prepared on host (package.json, index.js, yarn.lock +
#      .yarn-offline-mirror/, package-lock.json + .npm-cache/, .yarnrc).
set -uo pipefail
ARCH="${1:-x86_64}"
# Host sudo password for the `sudo -S` wrapper below — supply via env, never hardcode.
PW="${SUDO_PW:?set SUDO_PW to your host sudo password (used by the sudo -S wrapper)}"
# Portable layout: TGOSKITS_ROOT = your tgoskits checkout; NODEJS_FW = host-vendored nodejs
# test material dir; NODE_HOME = a host Node v22.22.2 install (its bundled npm is injected);
# apk closure ships under ./apks/<arch>/ (override via NODEJS_APKDIR).
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
FW="${NODEJS_FW:?set NODEJS_FW to the host-vendored nodejs test material directory}"
NODEHOME="${NODE_HOME:?set NODE_HOME to a host Node v22.22.2 install (provides bundled npm)}"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-nodejs.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-nodejs-pm.img
MP=/tmp/nodepmmnt-$ARCH
APKDIR="${NODEJS_APKDIR:-$HERE/apks/$ARCH}"
YARNAPK="$APKDIR/yarn-1.22.22-r1.apk"
sudo() { echo "$PW" | command sudo -S "$@" 2>/dev/null; }

[ -f "$BASE" ] || { echo "missing base $BASE (run prep-nodejs-rootfs.sh first)"; exit 2; }
[ -f "$YARNAPK" ] || { echo "missing $YARNAPK (apk fetch yarn from v3.22/community)"; exit 2; }
[ -d "$NODEHOME/lib/node_modules/npm" ] || { echo "missing host npm at $NODEHOME"; exit 2; }
[ -f "$FW/pm-sample/package.json" ] || { echo "missing pm-sample project (prepare on host)"; exit 2; }
[ -f "$FW/pm-sample/yarn.lock" ] || { echo "missing pm-sample yarn.lock (host yarn install)"; exit 2; }
[ -d "$FW/pm-sample/.yarn-offline-mirror" ] || { echo "missing pm-sample .yarn-offline-mirror"; exit 2; }
[ -f "$FW/pm-sample/package-lock.json" ] || { echo "missing pm-sample package-lock.json (host npm install)"; exit 2; }
[ -d "$FW/pm-sample/.npm-cache" ] || { echo "missing pm-sample .npm-cache (host npm install --cache)"; exit 2; }

echo "=== [$ARCH] copy nodejs img -> $IMG, resize to 2G ==="
cp -f "$BASE" "$IMG"
sudo truncate -s 2G "$IMG"
sudo e2fsck -f -y "$IMG" >/dev/null 2>&1
sudo resize2fs "$IMG" >/dev/null 2>&1

echo "=== [$ARCH] mount ==="
mkdir -p "$MP"
sudo mount -o loop,rw "$IMG" "$MP" || { echo "mount FAIL"; exit 2; }

echo "=== [$ARCH] inject yarn (Yarn Classic v1.22.22, pure JS) ==="
# apk is gzip-tar; payload under usr/ (yarn.js + wrappers + /usr/bin/yarn symlink). Skip
# the apk metadata members, extract the rest straight into the rootfs.
sudo tar xzf "$YARNAPK" -C "$MP" \
  --exclude='.PKGINFO' --exclude='.SIGN.*' --exclude='.pre-install' \
  --exclude='.post-install' --exclude='.pre-upgrade' --exclude='.post-upgrade' \
  --exclude='.trigger' 2>/dev/null
sudo ls -la "$MP/usr/share/node_modules/yarn/bin/yarn.js" "$MP/usr/bin/yarn" 2>&1 | head

echo "=== [$ARCH] inject ICU data (icu-data-full: real ~32MB icudt76l.dat) ==="
# CRITICAL: Alpine's icu-libs ships only a 9KB STUB libicudata.so.76.1; the real
# locale/collation tables live in the separate icu-data-full apk at
# /usr/share/icu/76.1/icudt76l.dat. Without it, Intl.Collator (which
# String.localeCompare uses, e.g. yarn's PackageLinker `.sort((a,b)=>a.localeCompare(b))`)
# throws "RangeError: Internal error. Icu error." (NOT a kernel bug) and breaks
# `yarn install`. Mirror the prep-nodejs-fw-rootfs.sh injection exactly.
ICUAPK="$APKDIR/icu-data-full-76.1-r1.apk"
if [ -f "$ICUAPK" ]; then
  sudo tar xzf "$ICUAPK" -C "$MP" \
    --exclude='.PKGINFO' --exclude='.SIGN.*' --exclude='.pre-install' \
    --exclude='.post-install' --exclude='.pre-upgrade' --exclude='.post-upgrade' \
    --exclude='.trigger' 2>/dev/null
  sudo ls -la "$MP/usr/share/icu/76.1/icudt76l.dat" 2>&1 | head -1
else
  echo "WARN: missing $ICUAPK (Intl/ICU collation will fail -> yarn localeCompare crash)"
fi

echo "=== [$ARCH] inject npm (bundled with node v$($NODEHOME/bin/npm --version)) ==="
sudo mkdir -p "$MP/usr/lib/node_modules"
sudo cp -a "$NODEHOME/lib/node_modules/npm" "$MP/usr/lib/node_modules/npm"
sudo ln -sf ../lib/node_modules/npm/bin/npm-cli.js "$MP/usr/bin/npm"
sudo ln -sf ../lib/node_modules/npm/bin/npx-cli.js "$MP/usr/bin/npx"
sudo chmod +x "$MP/usr/lib/node_modules/npm/bin/npm-cli.js" "$MP/usr/lib/node_modules/npm/bin/npx-cli.js"

echo "=== [$ARCH] inject /proj/pm-sample (offline sources for BOTH managers, no node_modules) ==="
sudo mkdir -p "$MP/proj"
sudo rm -rf "$MP/proj/pm-sample"
sudo mkdir -p "$MP/proj/pm-sample"
# rsync everything EXCEPT a stray node_modules (guest PM creates it). Keep the dotfiles:
# .yarnrc, .yarn-offline-mirror/, .npm-cache/ — they are the offline install sources.
sudo rsync -a --exclude='/node_modules/' "$FW/pm-sample/" "$MP/proj/pm-sample/"

echo "=== [$ARCH] verify ==="
sudo ls -la "$MP/usr/bin/yarn" "$MP/usr/bin/npm" "$MP/usr/bin/node" 2>&1 | head
sudo ls -la "$MP/usr/share/icu/76.1/icudt76l.dat" 2>&1 | head -1
sudo sh -c "ls $MP/proj/pm-sample/.yarn-offline-mirror/*.tgz" 2>&1
sudo sh -c "ls -d $MP/proj/pm-sample/.npm-cache/_cacache" 2>&1
sudo sh -c "test -e $MP/proj/pm-sample/node_modules && echo 'WARN: node_modules shipped (should be absent)' || echo 'OK: no node_modules (guest PM installs)'"
echo "free: $(df -h "$MP" | tail -1 | awk '{print $4}')"

sudo sync
sudo umount "$MP" && echo "[$ARCH] DONE -> $IMG"
