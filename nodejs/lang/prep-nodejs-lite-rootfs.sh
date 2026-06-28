#!/bin/bash
# Build rootfs-<arch>-nodejs-lite.img: start from the nodejs runtime img (node + .so deps
# closure already injected by prep-nodejs-rootfs.sh), then add THREE LIGHT projects that
# each run as a small, self-contained node program — so they stay below the
# node-heavy-JS-load threshold of StarryOS bug #242 and can pass NOW (unlike the heavy
# vite/astro builds in prep-nodejs-fw-rootfs.sh, which trip #242):
#   /proj/pp-standalone  — compile less/stylus/scss/pug via each preprocessor's DIRECT
#                          Node API (no vite/rollup graph). Ships vendored node_modules
#                          (the 4 pure-JS preprocessors + transitive deps, ~31MB).
#   /proj/cjs-esm        — CommonJS <-> ESM interop (default/named/dynamic-import, .cjs/.mjs).
#                          NO node_modules (Node builtins only).
#   /proj/kotlin-js      — run a HOST-precompiled Kotlin/JS module (dist-host-REF/app.js)
#                          on node and compare stdout to REF.out. NO node_modules (the
#                          Kotlin/JS stdlib is inlined into the emitted module). The Kotlin
#                          compiler is a JVM tool and does NOT run on the guest; build the
#                          emitted JS on the host first via kotlin-js/compile-host.sh.
# Each project ships a host REF: pp-standalone + cjs-esm get REF.sha256 (byte manifest of
# dist-host-REF/); kotlin-js ships app.js + REF.out directly (guest runs app.js, diffs out).
# On the guest the nodejs-lite-0 test rebuilds pp/cjs and re-runs kotlin-js, comparing to REF.
#
# Usage: bash prep-nodejs-lite-rootfs.sh <arch>   (default x86_64)
#
# PREREQUISITES (run on host first):
#   1. prep-nodejs-rootfs.sh <arch>   -> rootfs-<arch>-nodejs.img (node runtime base)
#   2. cd "$NODEJS_FW/pp-standalone" && npm install --no-save --prefer-offline
#      (pure-JS preprocessors; ~10s from cache, no native build)
#   3. bash "$NODEJS_FW/kotlin-js/compile-host.sh"
#      (emits dist-host-REF/app.js + REF.out; needs JDK17 + the bundled kotlinc-js)
#   4. host REF dirs already present for pp-standalone (dist-host-REF/) and cjs-esm
#      (dist-host-REF/result.json) — produced by running their build once on host node.
set -uo pipefail
ARCH="${1:-x86_64}"
# Host sudo password for the `sudo -S` wrapper below — supply via env, never hardcode.
PW="${SUDO_PW:?set SUDO_PW to your host sudo password (used by the sudo -S wrapper)}"
# Portable layout: TGOSKITS_ROOT = your tgoskits checkout; NODEJS_FW = the host-vendored
# nodejs test material dir (pp-standalone / cjs-esm / kotlin-js). No machine-specific paths.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
FW="${NODEJS_FW:?set NODEJS_FW to the host-vendored nodejs test material directory}"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-nodejs.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-nodejs-lite.img
MP=/tmp/nodelitemnt-$ARCH
sudo() { echo "$PW" | command sudo -S "$@" 2>/dev/null; }

[ -f "$BASE" ] || { echo "missing base $BASE (run prep-nodejs-rootfs.sh first)"; exit 2; }
# pp-standalone needs vendored node_modules + dist-host-REF
[ -d "$FW/pp-standalone/node_modules" ] || { echo "missing pp-standalone node_modules (npm install on host)"; exit 2; }
[ -d "$FW/pp-standalone/dist-host-REF" ] || { echo "missing pp-standalone dist-host-REF (host build first)"; exit 2; }
# cjs-esm needs dist-host-REF (no node_modules)
[ -d "$FW/cjs-esm/dist-host-REF" ] || { echo "missing cjs-esm dist-host-REF (host build first)"; exit 2; }
# kotlin-js needs the host-emitted app.js + REF.out
[ -f "$FW/kotlin-js/dist-host-REF/app.js" ] || { echo "missing kotlin-js dist-host-REF/app.js (run compile-host.sh)"; exit 2; }
[ -f "$FW/kotlin-js/dist-host-REF/REF.out" ] || { echo "missing kotlin-js dist-host-REF/REF.out (run compile-host.sh)"; exit 2; }

echo "=== [$ARCH] copy nodejs img -> $IMG, resize to 2G ==="
cp -f "$BASE" "$IMG"
sudo truncate -s 2G "$IMG"
sudo e2fsck -f -y "$IMG" >/dev/null 2>&1
sudo resize2fs "$IMG" >/dev/null 2>&1

echo "=== [$ARCH] mount ==="
mkdir -p "$MP"
sudo mount -o loop,rw "$IMG" "$MP" || { echo "mount FAIL"; exit 2; }

echo "=== [$ARCH] inject light projects ==="
sudo mkdir -p "$MP/proj"

# -- pp-standalone: src + build.mjs + vendored node_modules + REF.sha256 (NO live dist/) --
echo "  -> /proj/pp-standalone"
sudo rm -rf "$MP/proj/pp-standalone"; sudo mkdir -p "$MP/proj/pp-standalone"
sudo rsync -a --exclude='/dist/' --exclude='/dist-host-REF/' "$FW/pp-standalone/" "$MP/proj/pp-standalone/"
( cd "$FW/pp-standalone/dist-host-REF" && find . -type f -exec sha256sum {} \; | sort -k2 ) > "/tmp/REF-pp.sha256"
sudo cp "/tmp/REF-pp.sha256" "$MP/proj/pp-standalone/REF.sha256"
echo "    REF.sha256: $(wc -l < /tmp/REF-pp.sha256) files"

# -- cjs-esm: source + run.mjs + REF.sha256 (NO node_modules, NO live dist/) --
echo "  -> /proj/cjs-esm"
sudo rm -rf "$MP/proj/cjs-esm"; sudo mkdir -p "$MP/proj/cjs-esm"
sudo rsync -a --exclude='/dist/' --exclude='/dist-host-REF/' "$FW/cjs-esm/" "$MP/proj/cjs-esm/"
( cd "$FW/cjs-esm/dist-host-REF" && find . -type f -exec sha256sum {} \; | sort -k2 ) > "/tmp/REF-cjsesm.sha256"
sudo cp "/tmp/REF-cjsesm.sha256" "$MP/proj/cjs-esm/REF.sha256"
echo "    REF.sha256: $(wc -l < /tmp/REF-cjsesm.sha256) files"

# -- kotlin-js: ship the host-emitted app.js + REF.out at the project root for the guest run --
echo "  -> /proj/kotlin-js"
sudo rm -rf "$MP/proj/kotlin-js"; sudo mkdir -p "$MP/proj/kotlin-js"
sudo cp "$FW/kotlin-js/dist-host-REF/app.js" "$MP/proj/kotlin-js/app.js"
sudo cp "$FW/kotlin-js/dist-host-REF/REF.out" "$MP/proj/kotlin-js/REF.out"
# also drop the Kotlin source for provenance (not used at runtime)
sudo cp "$FW/kotlin-js/src/Main.kt" "$MP/proj/kotlin-js/Main.kt" 2>/dev/null || true
echo "    app.js: $(wc -c < "$FW/kotlin-js/dist-host-REF/app.js") bytes; REF.out: $(wc -l < "$FW/kotlin-js/dist-host-REF/REF.out") lines"

echo "=== [$ARCH] verify ==="
sudo ls "$MP/proj/" 2>&1
echo "free: $(df -h "$MP" | tail -1 | awk '{print $4}')"

sudo sync
sudo umount "$MP" && echo "[$ARCH] DONE -> $IMG"
