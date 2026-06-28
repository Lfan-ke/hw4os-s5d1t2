#!/bin/bash
# fetch-resources.sh — re-fetch the htop apks that were removed when this delivery repo
# was slimmed down. Run this (needs network) BEFORE running prep-htop-rootfs.sh;
# afterwards the prep script finds each apk at the exact path/name it expects
# (./apks/htop-<arch>.apk) and can build rootfs-<arch>-htop.img offline.
#
# WHAT IT RESTORES (per ./SOURCES.md):
#   apks/htop-<arch>.apk  <-  Alpine v3.23 main  htop-3.4.1-r1.apk
#   for arch in x86_64 aarch64 riscv64 loongarch64.
#   The CDN file is named htop-3.4.1-r1.apk; the prep script consumes it renamed to
#   htop-<arch>.apk, so we download straight into that destination name.
#
# sha256 PROVENANCE: SOURCES.md does not list per-file sha256, so the hashes below are
# the genuine sha256 of the originally delivered apks (ground truth for integrity).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"

MIRROR="https://dl-cdn.alpinelinux.org/alpine"
BRANCH="v3.23"
HTOP="htop-3.4.1-r1.apk"

sha_of() { sha256sum "$1" | awk '{print $1}'; }

# fetch <url> <dest> <sha256>
fetch() {
  local url="$1" dest="$2" want="$3"
  if [ -f "$dest" ] && [ "$(sha_of "$dest")" = "$want" ]; then
    echo "  skip (already present + verified): $dest"; return 0
  fi
  mkdir -p "$(dirname "$dest")"
  echo "  GET $url"
  local tmp="$dest.part"; rm -f "$tmp"
  if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 3 -o "$tmp" "$url"
  else
    wget -O "$tmp" "$url"
  fi
  local got; got="$(sha_of "$tmp")"
  if [ "$got" != "$want" ]; then
    rm -f "$tmp"
    echo "  ERROR: sha256 mismatch for $dest" >&2
    echo "    want $want" >&2
    echo "    got  $got" >&2
    exit 1
  fi
  mv -f "$tmp" "$dest"
  echo "  OK   $dest"
}

echo "=== htop 3.4.1-r1 apks (Alpine ${BRANCH} main, 4 arch) ==="
fetch "${MIRROR}/${BRANCH}/main/x86_64/${HTOP}" \
      "$HERE/apks/htop-x86_64.apk" \
      a72560cd613b0e063cbc28977a0dbbe9825e36c9d7f0ade0a1eff1224b3695a9
fetch "${MIRROR}/${BRANCH}/main/aarch64/${HTOP}" \
      "$HERE/apks/htop-aarch64.apk" \
      b54b88e614715196cb995edfdf6610617ac3b096e0c053ab28f7dabaa2ae779e
fetch "${MIRROR}/${BRANCH}/main/riscv64/${HTOP}" \
      "$HERE/apks/htop-riscv64.apk" \
      8bec92f4b231c1b83e5d951dbf1164977049acdb7dbe6cf267542e4a8d5df055
fetch "${MIRROR}/${BRANCH}/main/loongarch64/${HTOP}" \
      "$HERE/apks/htop-loongarch64.apk" \
      fce057b8506fed73c506e7df1a678805830d8f0676639985ca25f96402cb2ee6

echo "fetch-resources: htop OK"
