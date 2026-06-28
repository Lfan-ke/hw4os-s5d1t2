#!/bin/bash
# fetch-resources.sh — re-acquire the slimmed-away, re-downloadable resources for
# python/pip-uv (consumed by prep-pip-uv-rootfs.sh).
#
# prep-pip-uv-rootfs.sh reads its big binaries from the SHARED external download
# dir DL="$HERE/../../../download/pip-uv" (pip wheel + 4-arch uv) and its offline
# build wheels from WHEELS_SRC="$HERE/offline-wheels" (setuptools/wheel/packaging/
# six). Provenance: ../../../download/pip-uv/PROVENANCE.md. That file documents
# versions + source URLs but no sha256, so:
#   - PyPI wheels (pip/setuptools/wheel/packaging/six): URL + sha256 are resolved
#     from the authoritative PyPI JSON API at run time (not fabricated).
#   - uv static musl tarballs (astral-sh GitHub release) and the loongarch64 uv /
#     libbz2 Alpine apks: deterministic official URLs, downloaded without a hash
#     (none recorded in PROVENANCE.md).
#
# NOTE: the CPython 3.14 base image (rootfs-<arch>-python314.img) that this prep
# layers on top of is produced by the python 3.14 rootfs build (see python/lang /
# the python314 prep), not a downloadable artifact, so it is out of scope here.
#
# Needs network. After this completes, run:  bash prep-pip-uv-rootfs.sh <arch>
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE/../../../download/pip-uv"     # shared external dir (same path prep reads)
WHEELS="$HERE/offline-wheels"           # prep WHEELS_SRC
MIRROR="https://dl-cdn.alpinelinux.org/alpine"
GH="https://github.com/astral-sh/uv/releases/download"

PIP_VER="26.1.2"
UV_VER="0.11.19"
SETUPTOOLS_VER="82.0.1"
WHEEL_VER="0.47.0"
PACKAGING_VER="26.2"
SIX_VER="1.17.0"

command -v python3 >/dev/null 2>&1 || { echo "need python3 (PyPI JSON resolve)" >&2; exit 1; }
command -v curl    >/dev/null 2>&1 || { echo "need curl" >&2; exit 1; }
mkdir -p "$DL" "$WHEELS"

# fetch <url> <dest> [sha256] — skip if present (+sha256 match when given), else
# download with retries and verify. Empty sha256 => download only (no hash recorded).
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
    echo "    (no sha256 in PROVENANCE.md — not verified)"
  fi
}

# fetch_pypi <project> <version> <filename> <dest> — resolve url+sha256 from the
# authoritative PyPI JSON API, then fetch+verify.
fetch_pypi() {
  local proj="$1" ver="$2" fn="$3" dest="$4" meta url sha
  meta="$(curl -fsSL --retry 3 --connect-timeout 30 "https://pypi.org/pypi/$proj/$ver/json")" \
    || { echo "FAIL pypi meta: $proj $ver" >&2; return 1; }
  url="$(printf '%s' "$meta" | python3 -c "import sys,json;d=json.load(sys.stdin);print(next(u['url'] for u in d['urls'] if u['url'].rsplit('/',1)[-1]=='$fn'))")" \
    || { echo "FAIL: $fn not found in PyPI $proj $ver" >&2; return 1; }
  sha="$(printf '%s' "$meta" | python3 -c "import sys,json;d=json.load(sys.stdin);print(next(u['digests']['sha256'] for u in d['urls'] if u['url'].rsplit('/',1)[-1]=='$fn'))")"
  [ -n "$url" ] && [ -n "$sha" ] || { echo "FAIL resolve $fn" >&2; return 1; }
  fetch "$url" "$dest" "$sha"
}

echo "=== pip wheel (PyPI, sha256 from PyPI JSON) ==="
fetch_pypi pip "$PIP_VER" "pip-$PIP_VER-py3-none-any.whl" "$DL/pip-$PIP_VER-py3-none-any.whl"

echo "=== uv $UV_VER static musl tarballs (astral-sh GitHub release) ==="
for tri in x86_64-unknown-linux-musl aarch64-unknown-linux-musl riscv64gc-unknown-linux-musl; do
  fetch "$GH/$UV_VER/uv-$tri.tar.gz" "$DL/uv-$tri.tar.gz" ""
done

echo "=== uv $UV_VER loongarch64 (Alpine edge community apk; astral ships no loong binary) ==="
fetch "$MIRROR/edge/community/loongarch64/uv-$UV_VER-r0.apk" "$DL/uv-loongarch64-uv-$UV_VER-r0.apk" ""

echo "=== libbz2 loongarch64 (runtime dep of the dynamic loong uv; used by apps/starry/pip-uv prebuild) ==="
fetch "$MIRROR/edge/main/loongarch64/libbz2-1.0.8-r6.apk" "$DL/libbz2-loongarch64-1.0.8-r6.apk" "" \
  || echo "  WARN: libbz2 loong apk fetch failed (only needed for loongarch64)"

echo "=== offline build backends -> ./offline-wheels (prep WHEELS_SRC) ==="
fetch_pypi setuptools "$SETUPTOOLS_VER" "setuptools-$SETUPTOOLS_VER-py3-none-any.whl" "$WHEELS/setuptools-$SETUPTOOLS_VER-py3-none-any.whl"
fetch_pypi wheel "$WHEEL_VER" "wheel-$WHEEL_VER-py3-none-any.whl" "$WHEELS/wheel-$WHEEL_VER-py3-none-any.whl"
fetch_pypi packaging "$PACKAGING_VER" "packaging-$PACKAGING_VER-py3-none-any.whl" "$WHEELS/packaging-$PACKAGING_VER-py3-none-any.whl"
fetch_pypi six "$SIX_VER" "six-$SIX_VER-py2.py3-none-any.whl" "$WHEELS/six-$SIX_VER-py2.py3-none-any.whl"

echo "fetch-resources: python/pip-uv OK"
