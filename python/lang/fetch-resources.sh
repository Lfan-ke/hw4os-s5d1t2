#!/bin/bash
# fetch-resources.sh — re-acquire the slimmed-away, re-downloadable resources for
# python/lang (the Alpine-edge pure-interpreter CPython 3.14 closure consumed by
# prebuild.sh from ./apks/<arch>).
#
# The slim delivery dropped apks/<arch>/*.apk (21 .apk per arch, 84 total: python3
# 3.14.x + its pure-interpreter so deps — musl/libcrypto3/libssl3/ncurses/readline/
# sqlite-libs/libffi/mpdecimal/gdbm/xz/zlib/libbz2/libstdc++/libgcc/libexpat).
# SOURCES.md documents the Alpine edge/main CDN URL + the apk-closure.py resolver
# but lists no per-file sha256. python/lang ships no resolver of its own, so this
# uses the retained shared resolver ../core/apks/apk-closure.py (it resolves the
# closure from the signed Alpine APKINDEX and downloads each .apk). ROUTE B / pure
# interpreter => the single root is python3 from edge/main.
#
# Needs network. After this completes, the app runner invokes prebuild.sh
# (cargo xtask starry app qemu) which extracts ./apks/<arch> into the rootfs.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
RESOLVER="$HERE/../core/apks/apk-closure.py"
ARCHES="x86_64 aarch64 riscv64 loongarch64"
BRANCH="edge"          # SOURCES.md: python3 3.14.3-r0 is in Alpine edge/main
ROOT_PKG="python3"     # pure interpreter closure (no numpy/scipy native stack)

command -v python3 >/dev/null 2>&1 || { echo "need python3 to run the resolver" >&2; exit 1; }
[ -f "$RESOLVER" ] || { echo "missing shared resolver $RESOLVER" >&2; exit 1; }

for A in $ARCHES; do
  OUT="$HERE/apks/$A"
  mkdir -p "$OUT"
  echo "=== [$A] resolve + download CPython 3.14 pure-interpreter closure ($BRANCH/main) ==="
  rc=0
  python3 "$RESOLVER" --arch "$A" --out "$OUT" \
    --repo "$BRANCH/main" $ROOT_PKG || rc=$?
  if [ "$rc" -ge 2 ]; then echo "resolver FATAL ($rc) for $A" >&2; exit 2; fi
  if [ "$rc" = 1 ]; then echo "  WARN: some apk downloads failed for $A (see log)"; fi
done

echo "fetch-resources: python/lang OK"
