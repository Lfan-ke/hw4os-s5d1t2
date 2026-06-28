#!/bin/bash
# fetch-resources.sh — re-fetch the redownloadable etcd assets that were removed
# when this delivery repo was slimmed down. NEEDS NETWORK.
#
# After this script finishes, run:   bash prep-etcd-rootfs.sh <arch>
#
# WHAT THIS FETCHES
#   prep-etcd-rootfs.sh consumes one OFFICIAL etcd v3.6.11 release tarball per arch
#   from ./bins/<arch>/ . etcd v3.6.11 ships all four arches officially (amd64/arm64/
#   riscv64/loong64), so every tarball is a direct official download:
#     x86_64      -> bins/x86_64/etcd-v3.6.11-linux-amd64.tar.gz
#     aarch64     -> bins/aarch64/etcd-v3.6.11-linux-arm64.tar.gz
#     riscv64     -> bins/riscv64/etcd-v3.6.11-linux-riscv64.tar.gz
#     loongarch64 -> bins/loongarch64/etcd-v3.6.11-linux-loong64.tar.gz
#
# URL: from SOURCES.md (etcd-io GitHub releases). No sha256 is recorded in SOURCES.md
# or the qemu tomls for these tarballs, so checksum is skipped (verify against the
# upstream release SHA256SUMS if required).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"

have() { command -v "$1" >/dev/null 2>&1; }

dl() { # dl <url> <dest>
  local url="$1" dest="$2"
  mkdir -p "$(dirname "$dest")"
  if have curl; then curl -fL --retry 3 --retry-delay 2 -o "$dest" "$url"
  elif have wget; then wget -O "$dest" "$url"
  else echo "need curl or wget" >&2; exit 2; fi
}

fetch() { # fetch <url> <dest> <sha256|->
  local url="$1" dest="$2" want="$3" got
  if [ -f "$dest" ]; then
    if [ "$want" != "-" ] && [ -n "$want" ]; then
      got="$(sha256sum "$dest" | awk '{print $1}')"
      if [ "$got" = "$want" ]; then echo "  skip (present, sha256 OK): $dest"; return 0; fi
      echo "  re-download (sha256 changed): $dest"
    else echo "  skip (present): $dest"; return 0; fi
  fi
  echo "  fetch $url"
  dl "$url" "$dest"
  if [ "$want" = "-" ] || [ -z "$want" ]; then
    echo "  WARN: no authoritative sha256 in SOURCES.md — verify manually"
  else
    got="$(sha256sum "$dest" | awk '{print $1}')"
    if [ "$got" != "$want" ]; then
      echo "  sha256 MISMATCH for $(basename "$dest"): got $got want $want" >&2
      rm -f "$dest"; exit 1
    fi
    echo "  sha256 OK: $want"
  fi
}

EV=3.6.11
BASE="https://github.com/etcd-io/etcd/releases/download/v$EV"

# arch -> goarch token (== tarball name suffix == top dir inside the tarball)
fetch "$BASE/etcd-v$EV-linux-amd64.tar.gz"   "$HERE/bins/x86_64/etcd-v$EV-linux-amd64.tar.gz"        "-"
fetch "$BASE/etcd-v$EV-linux-arm64.tar.gz"   "$HERE/bins/aarch64/etcd-v$EV-linux-arm64.tar.gz"       "-"
fetch "$BASE/etcd-v$EV-linux-riscv64.tar.gz" "$HERE/bins/riscv64/etcd-v$EV-linux-riscv64.tar.gz"     "-"
fetch "$BASE/etcd-v$EV-linux-loong64.tar.gz" "$HERE/bins/loongarch64/etcd-v$EV-linux-loong64.tar.gz" "-"

echo "fetch-resources: etcd OK"
