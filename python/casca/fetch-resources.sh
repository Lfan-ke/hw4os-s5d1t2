#!/bin/bash
# fetch-resources.sh — re-fetch the slimmed-away, re-downloadable resources for the
# `casca-0` StarryOS case so that prep-casca-rootfs.sh can build the rootfs again.
#
# The delivery repo was slimmed: every artifact that can be re-downloaded from an
# upstream registry was removed and only build/prep scripts + provenance were kept.
# This script restores them, at the EXACT paths/filenames prep-casca-rootfs.sh
# expects (./wheels/<name>-<ver>-py3-none-any.whl), then verifies each by sha256.
#
# Requires: network access + a host python3 with pip. Run this FIRST, then run
#   bash prep-casca-rootfs.sh <arch>
#
# casca is a single pure-python (py3-none-any, arch-independent) wheel from PyPI.
# Its SOURCES.md documents the acquisition method as `pip download --only-binary=:all:`,
# so we use pip to fetch the pinned version deterministically. The sha256 below is the
# digest of the artifact that originally shipped in this repo (authoritative ground truth;
# SOURCES.md carries no hash for this wheel).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WHEELS="$HERE/wheels"

# pip_fetch <dest-file> <sha256> <pip-download-args...>
#   skip if dest already present with matching sha256; otherwise `pip download` the
#   pinned spec into a temp dir, require the exact basename, verify sha256, then move.
pip_fetch() {
  local dest="$1" want="$2"; shift 2
  local base; base="$(basename "$dest")"
  if [ -f "$dest" ] && [ "$(sha256sum "$dest" | awk '{print $1}')" = "$want" ]; then
    echo "  [skip] $base (sha256 ok)"; return 0
  fi
  if [ -f "$dest" ]; then echo "  [stale] $base -> re-download"; rm -f "$dest"; fi
  mkdir -p "$(dirname "$dest")"
  local td; td="$(mktemp -d)"
  if ! python3 -m pip download --no-deps --disable-pip-version-check -d "$td" "$@"; then
    rm -rf "$td"; echo "  [ERR] pip download failed: $*" >&2; return 1
  fi
  if [ ! -f "$td/$base" ]; then
    echo "  [ERR] expected '$base' not produced by: pip download $*" >&2
    echo "        produced: $(cd "$td" && ls)" >&2
    rm -rf "$td"; return 1
  fi
  local got; got="$(sha256sum "$td/$base" | awk '{print $1}')"
  if [ "$got" != "$want" ]; then
    echo "  [ERR] sha256 mismatch for $base: got $got want $want" >&2
    rm -rf "$td"; return 1
  fi
  mv "$td/$base" "$dest"; rm -rf "$td"
  echo "  [ok] $base"
}

echo "=== fetch casca pure-python wheel (PyPI) -> $WHEELS ==="
pip_fetch "$WHEELS/casca-1.0.4-py3-none-any.whl" \
  90b4946493ba6052f42d0d7d5f24f271a1978da1bd8d6fe0d3cc63ee6bd19d72 \
  --only-binary=:all: "casca==1.0.4"

echo "fetch-resources: casca OK"
