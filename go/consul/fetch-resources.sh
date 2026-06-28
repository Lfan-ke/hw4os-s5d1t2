#!/bin/bash
# fetch-resources.sh — re-fetch the redownloadable consul assets that were removed
# when this delivery repo was slimmed down. NEEDS NETWORK.
#
# After this script finishes, run:   bash prep-consul-rootfs.sh <arch>
#
# WHAT THIS FETCHES
#   prep-consul-rootfs.sh consumes per-arch binaries from ./bins/<arch>/:
#     x86_64  -> bins/x86_64/consul_1.22.7_linux_amd64.zip   (OFFICIAL HashiCorp release)
#     aarch64 -> bins/aarch64/consul_1.22.7_linux_arm64.zip  (OFFICIAL HashiCorp release)
#     riscv64 -> bins/riscv64/consul                         (SELF cross-compiled, KEPT in repo)
#     loongarch64 -> bins/loongarch64/consul                 (SELF cross-compiled, KEPT in repo)
#   Only the two official release ZIPs were removed during slimming; this script
#   re-downloads them and verifies the SOURCES/toml sha256. The riscv64/loongarch64
#   binaries are NOT official releases (HashiCorp ships amd64/arm64 only) — they are
#   patched-dep cross-builds kept in the repo; rebuild them with build-consul-riscv-loong.sh
#   if ever needed (see SOURCES.md for the boltdb/gopsutil patch rationale).
#
# sha256 source: each consul case/consul-0/qemu-<arch>.toml "Provenance:" header.
# URL: HashiCorp canonical release host (SOURCES.md states "官方 release").
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
    echo "  WARN: no authoritative sha256 — verify manually"
  else
    got="$(sha256sum "$dest" | awk '{print $1}')"
    if [ "$got" != "$want" ]; then
      echo "  sha256 MISMATCH for $(basename "$dest"): got $got want $want" >&2
      rm -f "$dest"; exit 1
    fi
    echo "  sha256 OK: $want"
  fi
}

CV=1.22.7
BASE="https://releases.hashicorp.com/consul/$CV"

# --- official release ZIPs (removed during slimming) -------------------------
fetch "$BASE/consul_${CV}_linux_amd64.zip" \
      "$HERE/bins/x86_64/consul_${CV}_linux_amd64.zip" \
      "fe25cecd8dd3552a8e5b0941cde1d79bb6004eac384aa45679dd1398f947201d"
fetch "$BASE/consul_${CV}_linux_arm64.zip" \
      "$HERE/bins/aarch64/consul_${CV}_linux_arm64.zip" \
      "db54c5fb7c5ceaef97a38ca45dcc0f649ff592a48c73ab320e2d535c78e136cc"

# --- riscv64 / loongarch64: self cross-built, KEPT in repo (no download) ------
# No official HashiCorp release exists for these arches. The binaries under
# bins/riscv64/consul and bins/loongarch64/consul are retained in this repo.
# To regenerate from source (needs go>=1.20 + network):  bash build-consul-riscv-loong.sh
for a in riscv64 loongarch64; do
  if [ -f "$HERE/bins/$a/consul" ]; then
    echo "  present (self-built, kept): bins/$a/consul"
  else
    echo "  MISSING bins/$a/consul — rebuild with: bash build-consul-riscv-loong.sh" >&2
  fi
done

echo "fetch-resources: consul OK"
