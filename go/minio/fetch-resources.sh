#!/bin/bash
# fetch-resources.sh — re-fetch/rebuild the minio binaries that were removed when
# this delivery repo was slimmed down. NEEDS NETWORK (and a Go toolchain for rv/loong).
#
# After this script finishes, run:   bash prep-minio-rootfs.sh <arch>
#
# WHAT THIS PROVIDES  (prep-minio-rootfs.sh consumes ./bins/<arch>/minio)
#   x86_64      -> OFFICIAL dl.min.io linux-amd64, pinned RELEASE.2025-09-07T16-13-09Z  (direct download)
#   aarch64     -> OFFICIAL dl.min.io linux-arm64, pinned RELEASE.2025-09-07T16-13-09Z  (direct download)
#   riscv64     -> SELF cross-compiled, RELEASE.2025-10-15T17-29-55Z  (SOURCE BUILD — no official release)
#   loongarch64 -> SELF cross-compiled, RELEASE.2025-10-15T17-29-55Z  (SOURCE BUILD — no official release)
#
# IMPORTANT (version pin): the bare ".../linux-<arch>/minio" URL serves "latest stable"
# which has rolled past the pinned date and would FAIL the per-arch toml version gate.
# We therefore use the dl.min.io *archive* URL that pins the exact RELEASE tag.
#
# sha256: SOURCES.md / the qemu tomls record only ABBREVIATED hashes (x86 7c5bd8..855f,
# aa 5c83cd..f03d, rv f846b3..f639, loong d1398d..98be) — not full 64-hex — so an
# automatic checksum is not possible here; verify manually against those prefixes.
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
  chmod 0755 "$dest" 2>/dev/null || true
  if [ "$want" = "-" ] || [ -z "$want" ]; then
    echo "  WARN: no full sha256 available — verify manually (see header for abbreviated hash)"
  else
    got="$(sha256sum "$dest" | awk '{print $1}')"
    if [ "$got" != "$want" ]; then
      echo "  sha256 MISMATCH for $(basename "$dest"): got $got want $want" >&2
      rm -f "$dest"; exit 1
    fi
    echo "  sha256 OK: $want"
  fi
}

# --- official binaries (removed during slimming) -----------------------------
OFF_VER="RELEASE.2025-09-07T16-13-09Z"
fetch "https://dl.min.io/server/minio/release/linux-amd64/archive/minio.$OFF_VER" \
      "$HERE/bins/x86_64/minio" "-"
fetch "https://dl.min.io/server/minio/release/linux-arm64/archive/minio.$OFF_VER" \
      "$HERE/bins/aarch64/minio" "-"

# --- riscv64 / loongarch64: SOURCE BUILD (no official release exists) ---------
# minio is pure Go (CGO disabled) so cross-compiling is straightforward, but there is
# NO setup-*.sh in this app dir. The steps below reproduce the self cross-compile
# documented in SOURCES.md, pinned at the RELEASE.2025-10-15T17-29-55Z git tag, with
# the version string embedded via upstream's Makefile gen-ldflags (as the rv/loong
# tomls state). Requires: git, go>=1.26 (SOURCES.md used go1.26.3), network.
SRC_VER="RELEASE.2025-10-15T17-29-55Z"
SRC="${MINIO_SRC:-$HERE/.minio-src}"

build_minio() { # build_minio <goarch> <starch>
  local goarch="$1" starch="$2" dest="$HERE/bins/$starch/minio"
  if [ -f "$dest" ]; then echo "  present (self-built): bins/$starch/minio"; return 0; fi
  if ! have go || ! have git; then
    echo "  NEED-MANUAL bins/$starch/minio: install git + go>=1.26, then:" >&2
    echo "    git clone --depth 1 --branch $SRC_VER https://github.com/minio/minio \"$SRC\"" >&2
    echo "    ( cd \"$SRC\" && CGO_ENABLED=0 GOOS=linux GOARCH=$goarch \\" >&2
    echo "        go build -trimpath -ldflags \"\$(go run buildscripts/gen-ldflags.go)\" -o \"$dest\" . )" >&2
    echo "    # (version embedding follows upstream Makefile gen-ldflags at tag $SRC_VER)" >&2
    return 0
  fi
  echo "  source-build minio $starch ($goarch) @ $SRC_VER"
  [ -d "$SRC/.git" ] || git clone --depth 1 --branch "$SRC_VER" https://github.com/minio/minio "$SRC"
  mkdir -p "$HERE/bins/$starch"
  ( cd "$SRC" && CGO_ENABLED=0 GOOS=linux GOARCH="$goarch" \
      go build -trimpath -ldflags "$(go run buildscripts/gen-ldflags.go)" -o "$dest" . )
  chmod 0755 "$dest"
  echo "  built: bins/$starch/minio"
}

build_minio riscv64 riscv64
build_minio loong64 loongarch64

echo "fetch-resources: minio OK"
