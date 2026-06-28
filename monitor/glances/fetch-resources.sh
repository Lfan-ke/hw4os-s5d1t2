#!/bin/bash
# fetch-resources.sh — re-fetch the glances apk closure that was removed when this
# delivery repo was slimmed down. Run this (needs network) BEFORE running
# prep-glances-rootfs.sh; afterwards every apk sits under ./apks/<arch>/ at the exact
# name the prep script expects and the rootfs build runs offline.
#
# WHAT IT RESTORES (per ./SOURCES.md and ../SOURCES.md):
#   apks/<arch>/*.apk  —  the full transitive closure of
#       glances 4.4.1-r1   (Alpine v3.23 community)
#       py3-psutil 7.1.3-r0 (Alpine v3.23 main, native _psutil_linux.abi3.so)
#       + CPython 3.12.13 runtime closure
#   = 49 apks per arch, for arch in x86_64 aarch64 riscv64 loongarch64 (196 files).
#
# HOW: the closure is resolved + downloaded by the shipped, self-contained resolver
# ./apks/apk-closure.py (the same tool prep-glances-rootfs.sh invokes when the host has
# no `apk`). It reads each branch's APKINDEX from the Alpine CDN
# (https://dl-cdn.alpinelinux.org/alpine/v3.23/{main,community}/<arch>/), walks the
# dependency graph, and downloads each missing .apk into the output dir. This is the
# authoritative fetch path; SOURCES.md carries no per-file sha256 (it delegates to a
# non-shipped download-cache SOURCES.md), so version pinning is via the exact
# version-release names the resolver selects from the v3.23 APKINDEX.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
RESOLVER="$HERE/apks/apk-closure.py"

command -v python3 >/dev/null 2>&1 || { echo "need python3 to run the apk closure resolver" >&2; exit 2; }
[ -f "$RESOLVER" ] || { echo "missing resolver $RESOLVER" >&2; exit 2; }

BRANCH="v3.23"
ROOTS_MAIN="py3-psutil"      # native /proc reader (main)
ROOTS_COMM="glances"         # the monitor; pulls its whole pure-python plugin stack (community)

for ARCH in x86_64 aarch64 riscv64 loongarch64; do
  OUT="$HERE/apks/$ARCH"
  mkdir -p "$OUT"
  echo "=== [$ARCH] resolve + download glances closure into apks/$ARCH ==="
  python3 "$RESOLVER" --arch "$ARCH" --out "$OUT" \
    --repo "$BRANCH/main"      $ROOTS_MAIN \
    --repo "$BRANCH/community" $ROOTS_COMM
done

echo "fetch-resources: glances OK"
