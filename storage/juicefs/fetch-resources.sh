#!/bin/bash
# fetch-resources.sh — re-fetch the redistributable juicefs release tarballs that
# were removed when this delivery repo was slimmed down. Requires NETWORK ACCESS.
# After it finishes, run the prep script as usual:
#   bash case/prep-juicefs-rootfs.sh <arch>   # arch in x86_64|aarch64|riscv64|loongarch64
#
# x86_64 + aarch64 are the OFFICIAL juicedata GitHub releases (direct download,
# sha256-verified against golang-bins/SOURCES.md §2).
#
# riscv64 + loongarch64 are NOT published by upstream (juicefs ships no riscv64 /
# loong64 release) and CANNOT be downloaded — they were SELF CROSS-COMPILED. juicefs
# FORCES CGO (pkg/meta/sql_sqlite.go pulls mattn/go-sqlite3; DataDog/zstd, gspt,
# go-lz4 are cgo-only), so the build follows the official `juicefs.musl` static path
# and is NON-DETERMINISTIC (a fresh build will NOT reproduce the recorded sha256).
# They are therefore handled as a manual source build below (see comment block), not
# fetched. See golang-bins/SOURCES.md §1.3/§2 for the exact toolchain/ldflags.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
BINS="$HERE/case/bins"
REL="https://github.com/juicedata/juicefs/releases/download/v1.3.1"

# fetch <url> <dest> <sha256>
fetch() {
  local url="$1" dest="$2" want="$3" got
  mkdir -p "$(dirname "$dest")"
  if [ -f "$dest" ]; then
    got="$(sha256sum "$dest" | awk '{print $1}')"
    if [ "$got" = "$want" ]; then
      echo "  = $(basename "$dest") (cached, sha256 ok)"
      return 0
    fi
    echo "  ! $(basename "$dest") present but sha256 mismatch -> re-downloading"
    rm -f "$dest"
  fi
  echo "  + $(basename "$dest")"
  if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 3 -o "$dest" "$url"
  elif command -v wget >/dev/null 2>&1; then
    wget -O "$dest" "$url"
  else
    echo "need curl or wget" >&2; exit 2
  fi
  got="$(sha256sum "$dest" | awk '{print $1}')"
  if [ "$got" != "$want" ]; then
    echo "  ! sha256 MISMATCH for $dest" >&2
    echo "      want $want" >&2
    echo "      got  $got" >&2
    rm -f "$dest"
    exit 1
  fi
  echo "    sha256 ok"
}

# ---- official prebuilt releases (direct download) ----
fetch "$REL/juicefs-1.3.1-linux-amd64.tar.gz" "$BINS/x86_64/juicefs-1.3.1-linux-amd64.tar.gz"  eb67a7be5d174b420cb3734d441971b3a462ab522b78ad2a6ed993e7deddcd44
fetch "$REL/juicefs-1.3.1-linux-arm64.tar.gz" "$BINS/aarch64/juicefs-1.3.1-linux-arm64.tar.gz" c29bff8f609366011cee03b9abcc76c11a06308b2c314364b8c340a2bfbc6c48

# ---- riscv64 / loongarch64: SOURCE CROSS-BUILD (no upstream release, NOT fetched) ----
# There is no official juicefs riscv64/loong64 binary to download, and the self-built
# artifacts are non-deterministic, so these are NOT fetched automatically. To rebuild
# them, clone juicefs v1.3.1 and run the official `juicefs.musl` static CGO build per
# golang-bins/SOURCES.md §1.3, then place the resulting `juicefs` into the SAME flat
# tar.gz layout (LICENSE / README.md / README_CN.md / juicefs) at the dest paths below:
#
#   riscv64  -> CGO_ENABLED=1 GOOS=linux GOARCH=riscv64 CC=riscv64-linux-musl-gcc \
#               CGO_LDFLAGS=-static go build \
#               -ldflags "-s -w -X github.com/juicedata/juicefs/pkg/version.revision=e0032b2 \
#                         -X github.com/juicedata/juicefs/pkg/version.revisionDate=2025-12-02"
#       dest: $BINS/riscv64/juicefs-1.3.1-linux-riscv64.tar.gz
#       reference sha256 (original delivered artifact; build is non-deterministic):
#         6139f908b56f1c903eba6b65f298a8f7728f3184bee408f3fec3bdd8cb3e17cb
#
#   loongarch64 -> same as riscv64 but GOARCH=loong64 and CC="zig cc -target loongarch64-linux-musl"
#       dest: $BINS/loongarch64/juicefs-1.3.1-linux-loong64.tar.gz
#       reference sha256 (original delivered artifact; build is non-deterministic):
#         17f5b40d004c47fe501281b9ddf05181fce87b2307224f77ad95c2094471b4f1
echo "fetch-resources: storage/juicefs note: riscv64/loongarch64 are source cross-builds"
echo "  (no upstream release; rebuild per golang-bins SOURCES.md §1.3 — see comments above)"

echo "fetch-resources: storage/juicefs OK"
