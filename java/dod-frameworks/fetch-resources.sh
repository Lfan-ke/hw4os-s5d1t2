#!/bin/bash
# fetch-resources.sh — re-fetch the re-downloadable resources that were removed when
# this delivery tree was slimmed down. Needs network access. After this completes,
# run `bash redis/case/prep-redis-rootfs.sh <arch>` to build rootfs-<arch>-redis.img.
#
# Removed resources (consumed by redis/case/prep-redis-rootfs.sh as
# $HERE/../apks/<arch>/redis.apk):
#   redis/apks/x86_64/redis.apk
#   redis/apks/aarch64/redis.apk
#   redis/apks/riscv64/redis.apk
#   redis/apks/loongarch64/redis.apk
#   = Alpine edge/community redis 8.8.0-r0 musl packages (one per arch).
#   Source + sha256: redis/README.md ("来源 & 版本" table).
#
# RETAINED in the slim tree (NOT fetched here): all demo fat-jars under jars/ (our
# own build artifacts: ktor/quarkus/wildfly/spring/netty/... + net-test.jar), the
# self-built SQLite musl JNI .so files (jars/sqlite-musl-jni/*.so), the jse-suite/
# *.java sources, and all src-modules/ pom.xml + sources. The framework demo jars
# are built on the host from src-modules/ via maven (see SOURCES.md); they are kept
# in-tree, so nothing under jars/ needs re-downloading.
#
# NOTE: Alpine *edge* apks are rebuilt over time; the pinned redis-8.8.0-r0.apk URL
# may 404 once edge advances. If so, fetch the current edge redis-<ver>.apk for each
# arch from the same mirror path (the on-disk filename stays redis.apk) and update the
# sha256 accordingly. The sha256 values below pin the exact 8.8.0-r0 artifacts that
# originally shipped here (match the prefixes documented in redis/README.md).
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

REDIS_BASE="https://dl-cdn.alpinelinux.org/alpine/edge/community"

fetch \
  "$REDIS_BASE/x86_64/redis-8.8.0-r0.apk" \
  "$HERE/redis/apks/x86_64/redis.apk" \
  "4836ec82a922047245ce9a4e8ba11c4181303f1557c5f2f7b2ef2140d4bda8fa"

fetch \
  "$REDIS_BASE/aarch64/redis-8.8.0-r0.apk" \
  "$HERE/redis/apks/aarch64/redis.apk" \
  "5e703268b666fb9fb06b3ec509fdfb5acf4236e3002459d2a6868331b1821223"

fetch \
  "$REDIS_BASE/riscv64/redis-8.8.0-r0.apk" \
  "$HERE/redis/apks/riscv64/redis.apk" \
  "c1db5ab3661f23436aa8af92cd2e0fa843b3f994e80953c8e6424a0bdc5ec098"

fetch \
  "$REDIS_BASE/loongarch64/redis-8.8.0-r0.apk" \
  "$HERE/redis/apks/loongarch64/redis.apk" \
  "1e4e889c9f14be21a68c74b644e503922f36fa8e938586754dd3b11793908034"

echo "fetch-resources: dod-frameworks OK"
