#!/bin/bash
# fetch-resources.sh — re-fetch the large, re-downloadable resource(s) that were
# removed when this delivery tree was slimmed down. Needs network access.
# After this completes, run `bash prep-paimon-rootfs.sh <arch>` to build the rootfs.
#
# Removed resource (consumed by prep-paimon-rootfs.sh as $HERE/<file>):
#   paimon-flink-1.20-1.4.1.jar  (Apache Paimon 1.4.1 flink-1.20 bundle LIBRARY jar,
#   ~55 MB, architecture-independent Java bytecode — one jar for all 4 archs)
#
# NOT fetched (retained in the slim tree, self-built/ours):
#   paimon-smoke.jar  (our ~2 KB host-compiled PaimonSmoke driver)
#
# Source: Maven Central, groupId org.apache.paimon / artifactId paimon-flink-1.20 /
#   version 1.4.1 (see ../SOURCES.md §1.3).
# sha256 is the authoritative content fingerprint from ../SOURCES.md §2.
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
  "https://repo1.maven.org/maven2/org/apache/paimon/paimon-flink-1.20/1.4.1/paimon-flink-1.20-1.4.1.jar" \
  "$HERE/paimon-flink-1.20-1.4.1.jar" \
  "c70a60aef9d86d73220c7417ebe77d47b06bed5071aab65c75422a3becb56ea1"

echo "fetch-resources: paimon OK"
