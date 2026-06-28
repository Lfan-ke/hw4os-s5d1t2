#!/bin/bash
# fetch-resources.sh — re-acquire the slimmed-away, re-downloadable resources for
# python/core (the Alpine musl-native CPython 3.12 + numpy/scikit-learn/opencv apk
# closure consumed by prep-python-rootfs.sh).
#
# The slim delivery dropped apks/<arch>/*.apk (the ~288-291-package transitive
# closure per arch, 1156 .apk total). They are NOT enumerated with per-file
# sha256 in SOURCES-python-apks.md — that file documents only the Alpine CDN URL
# pattern + the closure roots. The authoritative re-fetch path (the very one
# prep-python-rootfs.sh uses) is the retained resolver apks/apk-closure.py, which
# resolves the closure from the signed Alpine APKINDEX and downloads each .apk.
#
# Needs network. After this completes, run:  bash prep-python-rootfs.sh <arch>
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
RESOLVER="$HERE/apks/apk-closure.py"
MIRROR="https://dl-cdn.alpinelinux.org/alpine"
ARCHES="x86_64 aarch64 riscv64 loongarch64"

# ROUTE A (default, matches prep-python-rootfs.sh PYBRANCH=v3.23 — same branch as
# the Alpine base image, zero ABI risk). For ROUTE B (edge/python 3.14) re-run the
# resolver with BRANCH=edge per SOURCES-python-apks.md §2/§8.
BRANCH="v3.23"
ROOTS_MAIN="python3 py3-pip"
ROOTS_COMM="py3-numpy py3-scikit-learn py3-opencv"
# uv apk shipped alongside the closure (v3.23/community). It is not a closure root,
# so the resolver does not pull it; fetch it explicitly. No sha256 in SOURCES.md.
UV_APK="uv-0.10.2-r0.apk"

command -v python3 >/dev/null 2>&1 || { echo "need python3 to run the resolver" >&2; exit 1; }
command -v curl    >/dev/null 2>&1 || { echo "need curl" >&2; exit 1; }
[ -f "$RESOLVER" ] || { echo "missing resolver $RESOLVER" >&2; exit 1; }

# fetch <url> <dest> [sha256]  — skip if present (+sha256 match when given), else
# download with retries and verify. Empty sha256 => download only (SOURCES.md has
# no hash for this file; integrity is via Alpine's signed index, not verified here).
fetch() {
  local url="$1" dest="$2" want="${3:-}" have
  if [ -f "$dest" ]; then
    if [ -n "$want" ]; then
      have="$(sha256sum "$dest" | awk '{print $1}')"
      if [ "$have" = "$want" ]; then echo "  = $(basename "$dest") (cached, sha256 ok)"; return 0; fi
      echo "  ! $(basename "$dest") cached sha256 mismatch -> re-download"; rm -f "$dest"
    elif [ -s "$dest" ]; then
      echo "  = $(basename "$dest") (cached)"; return 0
    fi
  fi
  mkdir -p "$(dirname "$dest")"
  echo "  + $(basename "$dest")"
  curl -fL --retry 3 --connect-timeout 30 "$url" -o "$dest" || { echo "FAIL download: $url" >&2; rm -f "$dest"; return 1; }
  if [ -n "$want" ]; then
    have="$(sha256sum "$dest" | awk '{print $1}')"
    [ "$have" = "$want" ] || { echo "FAIL sha256: $dest want=$want got=$have" >&2; rm -f "$dest"; return 1; }
  else
    echo "    (no sha256 in SOURCES.md — not verified)"
  fi
}

for A in $ARCHES; do
  OUT="$HERE/apks/$A"
  mkdir -p "$OUT"
  echo "=== [$A] resolve + download apk closure (ROUTE A, $BRANCH) ==="
  rc=0
  python3 "$RESOLVER" --arch "$A" --out "$OUT" \
    --repo "$BRANCH/main" $ROOTS_MAIN \
    --repo "$BRANCH/community" $ROOTS_COMM || rc=$?
  if [ "$rc" -ge 2 ]; then echo "resolver FATAL ($rc) for $A" >&2; exit 2; fi
  if [ "$rc" = 1 ]; then echo "  WARN: some apk downloads failed for $A (see log)"; fi
  fetch "$MIRROR/$BRANCH/community/$A/$UV_APK" "$OUT/$UV_APK" "" || echo "  WARN: uv apk fetch failed for $A"
done

echo "fetch-resources: python/core OK"
