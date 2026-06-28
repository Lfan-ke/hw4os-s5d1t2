#!/bin/bash
# fetch-resources.sh — pre-provision the external resource the golang-lang carpet needs.
# NEEDS NETWORK. After this finishes, the normal flow is:
#   cargo xtask starry app qemu -t go-lang --arch <x86_64|aarch64|riscv64|loongarch64>
# (prebuild.sh cross-compiles go/*.go on demand using the toolchain staged here.)
#
# NOTE ON SLIMMING
#   golang-lang stored NO large prebuilt binary in the repo to begin with: the static
#   ~100MB/arch binary is cross-compiled on demand by prebuild.sh, which itself fetches
#   the official go1.26.3 toolchain (and the pinned framework deps via go.mod/go.sum).
#   Nothing was removed from this app dir during slimming. This script merely PRE-STAGES
#   the official toolchain into the same cache prebuild.sh uses, so a later build runs
#   without re-downloading (useful for offline / air-gapped provisioning).
#
# sha256: taken from prebuild.sh (the in-repo authoritative gate). URL: SOURCES.md
#   (https://go.dev/dl/go1.26.3.linux-<host-arch>.tar.gz).
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

GO_VER=1.26.3
# Same cache location prebuild.sh checks/uses (overridable via GO_CARPET_CACHE).
cache_root="${GO_CARPET_CACHE:-${HOME:-/root}/.cache/starry-go-carpet}"
goroot="$cache_root/go"

# Toolchain is for the BUILD HOST arch (used to cross-compile to all StarryOS targets),
# mirroring prebuild.sh exactly (same arch map + sha256).
case "$(uname -m)" in
  x86_64)  ha=amd64; sha=2b2cfc7148493da5e73981bffbf3353af381d5f93e789c82c79aff64962eb556 ;;
  aarch64) ha=arm64; sha=9d89a3ea57d141c2b22d70083f2c8459ba3890f2d9e818e7e933b75614936565 ;;
  *) echo "unsupported build host $(uname -m) — see prebuild.sh" >&2; exit 1 ;;
esac

if [ -x "$goroot/bin/go" ]; then
  echo "  skip (toolchain already staged): $goroot/bin/go"
else
  mkdir -p "$cache_root"
  fetch "https://go.dev/dl/go${GO_VER}.linux-${ha}.tar.gz" "$cache_root/go.tgz" "$sha"
  echo "  extracting toolchain -> $goroot"
  tar -C "$cache_root" -xzf "$cache_root/go.tgz"
fi

# Optionally warm the pinned framework module cache (gin/grpc/go-zero/gorm/modernc sqlite)
# so the on-demand build needs no further network. Best-effort; safe to skip if it fails.
if [ -x "$goroot/bin/go" ]; then
  echo "  warming module cache (go.mod/go.sum pinned deps)"
  ( cd "$HERE/go" && GOROOT="$goroot" PATH="$goroot/bin:$PATH" GOTOOLCHAIN=local \
      GOPATH="$cache_root/gopath" GOCACHE="$cache_root/gocache" \
      go mod download ) || echo "  WARN: module warm-up skipped (will fetch at build time)"
fi

echo "fetch-resources: golang-lang OK"
