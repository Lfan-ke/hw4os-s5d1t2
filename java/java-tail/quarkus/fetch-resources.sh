#!/bin/bash
# fetch-resources.sh — re-fetch the re-downloadable resource that was removed from
# this slimmed delivery tree for the `quarkus-0` StarryOS case.
#
# WHAT THIS RESTORES (exact path/name that prep-quarkus-rootfs.sh expects):
#   package/quarkus-cli-3.35.4.tar.gz        (official Quarkus CLI 3.35.4 dist, ~20 MB)
#
# sha256 is taken verbatim from ../../bigapps/SOURCES.md (authoritative;
# = official checksums_sha256.txt).
# REQUIRES NETWORK. After running this, run prep-quarkus-rootfs.sh <arch>.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"

# fetch <url> <dest> <sha256>: skip if dest already matches; else download + verify.
fetch() {
  local url="$1" dest="$2" sha="$3" got
  if [ -f "$dest" ] && [ "$(sha256sum "$dest" | awk '{print $1}')" = "$sha" ]; then
    echo "  ok (cached): ${dest#$HERE/}"
    return 0
  fi
  mkdir -p "$(dirname "$dest")"
  echo "  fetching: $url"
  if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 3 -o "$dest" "$url"
  else
    wget -O "$dest" "$url"
  fi
  got="$(sha256sum "$dest" | awk '{print $1}')"
  if [ "$got" != "$sha" ]; then
    echo "  SHA256 MISMATCH for ${dest#$HERE/}" >&2
    echo "    expected $sha" >&2
    echo "    got      $got" >&2
    rm -f "$dest"
    exit 1
  fi
  echo "  verified: ${dest#$HERE/}"
}

echo "=== quarkus: official CLI distribution (GitHub release asset) ==="
fetch "https://github.com/quarkusio/quarkus/releases/download/3.35.4/quarkus-cli-3.35.4.tar.gz" \
      "$HERE/package/quarkus-cli-3.35.4.tar.gz" \
      "4ce2ed5937b77c4515439344f38e2b25f85aee1c66f266ce147c93d85aa8a92a"

echo "fetch-resources: quarkus OK"
