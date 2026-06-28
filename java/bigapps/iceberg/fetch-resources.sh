#!/bin/bash
# fetch-resources.sh — re-fetch the large, re-downloadable resource(s) that were
# removed when this delivery tree was slimmed down. Needs network access.
# After this completes, run `bash prep-iceberg-rootfs.sh <arch>` to build the rootfs.
#
# Removed resource (consumed by prep-iceberg-rootfs.sh as $HERE/<file>):
#   iceberg-spark-runtime-3.5_2.12-1.11.0.jar  (Apache Iceberg shaded Spark runtime
#   library jar, ~48 MB, architecture-independent Java bytecode — one jar for all 4 archs)
#
# Source: Maven Central, groupId org.apache.iceberg / artifactId
#   iceberg-spark-runtime-3.5_2.12 / version 1.11.0 (see ../SOURCES.md §1.2).
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
  "https://repo1.maven.org/maven2/org/apache/iceberg/iceberg-spark-runtime-3.5_2.12/1.11.0/iceberg-spark-runtime-3.5_2.12-1.11.0.jar" \
  "$HERE/iceberg-spark-runtime-3.5_2.12-1.11.0.jar" \
  "94b8e36fc329f0293d44ba9e01b784a56e9501affec1842d898144c51f6e486a"

echo "fetch-resources: iceberg OK"
