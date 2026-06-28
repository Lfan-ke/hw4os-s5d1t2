#!/bin/bash
# fetch-resources.sh — re-fetch the large, re-downloadable binaries that were removed
# when this delivery repo was slimmed down. Run this (needs network) BEFORE running
# prep-prometheus-rootfs.sh; afterwards the prep script finds every file under ./bins/
# at the exact path it expects and can build rootfs-<arch>-prometheus.img offline.
#
# WHAT IT RESTORES (per ../SOURCES.md and ../../SOURCES.md):
#   bins/prometheus/<arch>/prometheus-3.11.3.linux-<goarch>.tar.gz   (prometheus v3.11.3)
#   bins/node_exporter/<arch>/node_exporter                          (node_exporter v1.11.1)
#   for arch in x86_64 aarch64 riscv64 loongarch64.
#
#   prometheus + node_exporter for amd64/arm64/riscv64 are OFFICIAL GitHub release
#   assets (direct download). The loong64 (loongarch64) builds are NOT published
#   upstream — they are self-cross-compiled from source and CANNOT be downloaded;
#   they are listed below as manual source-build entries (with dest path + sha256 for
#   verification) and are intentionally NOT auto-fetched.
#
# sha256 PROVENANCE: the shipped SOURCES.md files do not carry per-file sha256 (they
# delegate to a non-shipped download-cache SOURCES.md). The hashes below are the
# genuine sha256 of the originally delivered artifacts; for the official assets they
# equal the checksums published with the upstream release.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"

PROM_VER="3.11.3"
NE_VER="1.11.1"
PROM_BASE="https://github.com/prometheus/prometheus/releases/download/v${PROM_VER}"
NE_BASE="https://github.com/prometheus/node_exporter/releases/download/v${NE_VER}"

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

# fetch_extract_bin <url> <tar-internal-path> <dest> <sha256-of-extracted-file>
# downloads a release tarball, extracts one binary, verifies the extracted file.
fetch_extract_bin() {
  local url="$1" inner="$2" dest="$3" want="$4"
  if [ -f "$dest" ] && [ "$(sha_of "$dest")" = "$want" ]; then
    echo "  skip (already present + verified): $dest"; return 0
  fi
  mkdir -p "$(dirname "$dest")"
  echo "  GET $url"
  local tgz; tgz="$(mktemp)"
  if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 3 -o "$tgz" "$url"
  else
    wget -O "$tgz" "$url"
  fi
  local tmpd; tmpd="$(mktemp -d)"
  tar xzf "$tgz" -C "$tmpd" "$inner"
  local got; got="$(sha_of "$tmpd/$inner")"
  if [ "$got" != "$want" ]; then
    rm -rf "$tgz" "$tmpd"
    echo "  ERROR: sha256 mismatch (extracted) for $dest" >&2
    echo "    want $want" >&2
    echo "    got  $got" >&2
    exit 1
  fi
  cp -f "$tmpd/$inner" "$dest"; chmod 0755 "$dest"
  rm -rf "$tgz" "$tmpd"
  echo "  OK   $dest"
}

echo "=== prometheus v${PROM_VER} release tarballs (official: amd64/arm64/riscv64) ==="
fetch "${PROM_BASE}/prometheus-${PROM_VER}.linux-amd64.tar.gz" \
      "$HERE/bins/prometheus/x86_64/prometheus-${PROM_VER}.linux-amd64.tar.gz" \
      9479af67673316278958cda1f39b88a09f8921084e039c65acca060d0447bb38
fetch "${PROM_BASE}/prometheus-${PROM_VER}.linux-arm64.tar.gz" \
      "$HERE/bins/prometheus/aarch64/prometheus-${PROM_VER}.linux-arm64.tar.gz" \
      d2ec0a96259afde955ad1560ced303cef99cac4dac676bd4dd7614d76adb708a
fetch "${PROM_BASE}/prometheus-${PROM_VER}.linux-riscv64.tar.gz" \
      "$HERE/bins/prometheus/riscv64/prometheus-${PROM_VER}.linux-riscv64.tar.gz" \
      bd6978937d64f4afa82919e0c4b3b83ace50808b953ab6174e480ca7dda2ba9a

echo "=== node_exporter v${NE_VER} static binary (official: amd64/arm64/riscv64) ==="
fetch_extract_bin "${NE_BASE}/node_exporter-${NE_VER}.linux-amd64.tar.gz" \
      "node_exporter-${NE_VER}.linux-amd64/node_exporter" \
      "$HERE/bins/node_exporter/x86_64/node_exporter" \
      3a01a3cc7f69798698fbb31b24e5ee279dd2c39727be2a5a65071536fc16b455
fetch_extract_bin "${NE_BASE}/node_exporter-${NE_VER}.linux-arm64.tar.gz" \
      "node_exporter-${NE_VER}.linux-arm64/node_exporter" \
      "$HERE/bins/node_exporter/aarch64/node_exporter" \
      c92bd1e9eeb4061f1bdbf60a7b41d446220a4a44d87fb77ae889f034cb8cf3bc
fetch_extract_bin "${NE_BASE}/node_exporter-${NE_VER}.linux-riscv64.tar.gz" \
      "node_exporter-${NE_VER}.linux-riscv64/node_exporter" \
      "$HERE/bins/node_exporter/riscv64/node_exporter" \
      3932dd6b4456eda301f3198fe0c22860b66afbe5ffa8214a32df532e490e5d21

# === MANUAL SOURCE-BUILD (loongarch64 / loong64) — NO upstream prebuilt asset =========
# These two loong64 artifacts are self-cross-compiled (per ../../SOURCES.md and the
# prep-prometheus-rootfs.sh header); there is no official download URL, so they are
# NOT auto-fetched. Build them from source on a Go toolchain, then place at the dest
# paths below and confirm the sha256.
#
#   prometheus loong64  (go1.26.3, embedded web UI, CGO disabled):
#     git clone --branch v3.11.3 https://github.com/prometheus/prometheus
#     cd prometheus && make assets    # build embedded UI
#     CGO_ENABLED=0 GOOS=linux GOARCH=loong64 \
#       promu build --prefix .build/linux-loong64   # or `go build ./cmd/prometheus ./cmd/promtool`
#     # repackage prometheus + promtool into a tarball whose top dir is
#     #   prometheus-3.11.3.linux-loong64/
#     dest: $HERE/bins/prometheus/loongarch64/prometheus-3.11.3.linux-loong64.tar.gz
#     sha256: 0805224b5c1359b48d44f103b77776fc31c4472238669767d425637de90797b4
#
#   node_exporter loong64  (CGO_ENABLED=0 static):
#     git clone --branch v1.11.1 https://github.com/prometheus/node_exporter
#     cd node_exporter && CGO_ENABLED=0 GOOS=linux GOARCH=loong64 go build .
#     dest: $HERE/bins/node_exporter/loongarch64/node_exporter
#     sha256: 76c56223816d403761564b581e8961d59360e0d50f7343d50a60767630306eba
# =====================================================================================

echo "fetch-resources: prometheus OK"
