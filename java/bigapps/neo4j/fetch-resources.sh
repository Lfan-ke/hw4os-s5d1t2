#!/bin/bash
# fetch-resources.sh — re-fetch the large, re-downloadable resource(s) that were
# removed when this delivery tree was slimmed down. Needs network access.
# After this completes, run `bash prep-neo4j-rootfs.sh <arch>` to build the rootfs.
#
# Removed resource (consumed by prep-neo4j-rootfs.sh as $HERE/packages/<file>):
#   packages/neo4j-community-2026.04.0-unix.tar.gz  (official Neo4j Community 2026.04.0
#   release tarball, ~235 MB, pure-Java distribution — same tarball for all 4 archs;
#   prep extracts lib/*.jar into /opt/neo4j/lib)
#
# NOT fetched (retained in the slim tree, self-built JNI cross-builds):
#   jna/libjnidispatch-riscv64-musl.so
#   jna/libjnidispatch-loongarch64-musl.so
#
# Source: dist.neo4j.org official release host (see SOURCES.md §1).
# sha256 is the authoritative official checksum from SOURCES.md §1/§2.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"

# fetch <url> <dest> <sha256>: skip if dest already matches; else download + verify.
fetch() {
  local url="$1" dest="$2" sha="$3"
  if [ -f "$dest" ] && echo "$sha  $dest" | sha256sum -c --status - 2>/dev/null; then
    echo "  [skip] $(basename "$dest") already present (sha256 OK)"
    return 0
  fi
  [ -f "$dest" ] && { echo "  [warn] $(basename "$dest") exists but sha256 mismatch -> re-downloading"; rm -f "$dest"; }
  mkdir -p "$(dirname "$dest")"
  echo "  [get] $url"
  if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 3 -o "$dest" "$url"
  else
    wget -O "$dest" "$url"
  fi
  if ! echo "$sha  $dest" | sha256sum -c --status - 2>/dev/null; then
    echo "  [FAIL] sha256 mismatch for $(basename "$dest") -> deleting" >&2
    rm -f "$dest"
    exit 1
  fi
  echo "  [ok] $(basename "$dest") sha256 verified"
}

fetch \
  "https://dist.neo4j.org/neo4j-community-2026.04.0-unix.tar.gz" \
  "$HERE/packages/neo4j-community-2026.04.0-unix.tar.gz" \
  "fd750466b1247c0d1ef09a84c614f7e045793b30dfa277148e8da71646598820"

echo "fetch-resources: neo4j OK"
