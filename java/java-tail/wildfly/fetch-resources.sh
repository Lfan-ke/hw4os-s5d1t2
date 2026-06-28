#!/bin/bash
# fetch-resources.sh — re-fetch the re-downloadable resource that was removed from
# this slimmed delivery tree for the `wildfly-0` StarryOS case.
#
# WHAT THIS RESTORES (exact path/name that prep-wildfly-rootfs.sh expects):
#   package/wildfly-40.0.0.Final.tar.gz      (official WildFly 40 binary dist, ~262 MB)
#
# sha256 is taken verbatim from ../../bigapps/SOURCES.md (authoritative; that file
# also records the upstream sha1 0b5948eb... = official .tar.gz.sha1).
# REQUIRES NETWORK. After running this, run prep-wildfly-rootfs.sh <arch>.
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

echo "=== wildfly: official binary distribution (GitHub release) ==="
fetch "https://github.com/wildfly/wildfly/releases/download/40.0.0.Final/wildfly-40.0.0.Final.tar.gz" \
      "$HERE/package/wildfly-40.0.0.Final.tar.gz" \
      "6b75f6de39dcf7e94b96f82006b96ec257b6358fc769a29d9817284c31c1e793"

echo "fetch-resources: wildfly OK"
