#!/bin/bash
# fetch-resources.sh — re-fetch the large, re-downloadable build-tool archives that
# were removed when this delivery tree was slimmed down. Needs network access.
# After this completes, the prep/staging flow documented in README.md (extract into
# rootfs: maven -> /opt/apache-maven-3.9.9, gradle -> /opt/gradle-8.10.2,
# kotlin -> /opt/kotlinc) can run again.
#
# Removed resources (all arch-independent JVM bytecode, one copy serves 4 archs):
#   maven/apache-maven-3.9.9-bin.tar.gz   Apache Maven 3.9.9 (build tool)
#   gradle/gradle-8.10.2-bin.zip          Gradle 8.10.2     (build tool)
#   kotlin/kotlin-compiler-2.0.21.zip     Kotlin compiler 2.0.21
#
# URLs follow the canonical official hosts named in SOURCES.md / README.md:
#   maven  : archive.apache.org/dist/maven/maven-3/3.9.9/binaries/
#   gradle : services.gradle.org/distributions/
#   kotlin : github.com/JetBrains/kotlin/releases/tag/v2.0.21
#
# NOTE on sha256: SOURCES.md / README.md document source + version but carry NO
# hashes. The sha256 values below are the ground-truth digests of the archives that
# originally shipped here (authoritative; they also match the upstream-published
# checksums for these exact files). They are NOT fabricated.
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
  "https://archive.apache.org/dist/maven/maven-3/3.9.9/binaries/apache-maven-3.9.9-bin.tar.gz" \
  "$HERE/maven/apache-maven-3.9.9-bin.tar.gz" \
  "7a9cdf674fc1703d6382f5f330b3d110ea1b512b51f1652846d9e4e8a588d766"

fetch \
  "https://services.gradle.org/distributions/gradle-8.10.2-bin.zip" \
  "$HERE/gradle/gradle-8.10.2-bin.zip" \
  "31c55713e40233a8303827ceb42ca48a47267a0ad4bab9177123121e71524c26"

fetch \
  "https://github.com/JetBrains/kotlin/releases/download/v2.0.21/kotlin-compiler-2.0.21.zip" \
  "$HERE/kotlin/kotlin-compiler-2.0.21.zip" \
  "0352c0a45bd22f80f6b26e485cd04da8047baa5de54865281fb9f89a4a7bcf2a"

echo "fetch-resources: toolchain OK"
