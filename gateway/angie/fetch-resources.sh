#!/bin/bash
# fetch-resources.sh — re-fetch the redistributable resources that were removed
# when this delivery repo was slimmed down (the large, freely re-downloadable apk
# packages). Requires NETWORK ACCESS. After it finishes, run the prep script as
# usual:  bash case/prep-angie-rootfs.sh <arch>
#
# Scope: ONLY the x86_64 + aarch64 angie apk closures. The riscv64 / loongarch64
# angie binaries are SOURCE-CROSS-BUILT (no official apk for those arches) and are
# kept in-tree under case/apks/<arch>/payload/ — they are NOT fetched here.
#
# sha256 values are the authoritative hashes recorded in the download-side
# provenance (gateway-bins/SOURCES.md §2); URLs follow the canonical official
# mirrors. The angie body comes from angie's own Alpine repo; the dependency
# closure (pcre2/zlib/openssl/libssl3/libcrypto3/musl) comes from Alpine v3.23/main.
#
# NOTE: Alpine's dl-cdn only serves the CURRENT package revision for a release
# branch. If a pinned -rN apk has been superseded the URL may 404 / mismatch; in
# that case pull the exact revision from an Alpine archive mirror. The sha256
# check below guards against any wrong/rotated file.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
APKS="$HERE/case/apks"

# fetch <url> <dest> <sha256>
fetch() {
  local url="$1" dest="$2" want="$3" got
  mkdir -p "$(dirname "$dest")"
  if [ -f "$dest" ]; then
    got="$(sha256sum "$dest" | awk '{print $1}')"
    if [ "$got" = "$want" ]; then
      echo "  = $(basename "$dest") (cached, sha256 ok)"
      return 0
    fi
    echo "  ! $(basename "$dest") present but sha256 mismatch -> re-downloading"
    rm -f "$dest"
  fi
  echo "  + $(basename "$dest")"
  if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 3 -o "$dest" "$url"
  elif command -v wget >/dev/null 2>&1; then
    wget -O "$dest" "$url"
  else
    echo "need curl or wget" >&2; exit 2
  fi
  got="$(sha256sum "$dest" | awk '{print $1}')"
  if [ "$got" != "$want" ]; then
    echo "  ! sha256 MISMATCH for $dest" >&2
    echo "      want $want" >&2
    echo "      got  $got" >&2
    rm -f "$dest"
    exit 1
  fi
  echo "    sha256 ok"
}

ALPINE="https://dl-cdn.alpinelinux.org/alpine/v3.23/main"
ANGIE="https://download.angie.software/angie/alpine/v3.23/main"

# ---- x86_64 closure (angie body + 6 deps) ----
fetch "$ANGIE/x86_64/angie-1.11.5-r0.apk"       "$APKS/x86_64/angie-1.11.5-r0.apk"       4ad91b7932451695193783aae41ec5f61a0cbbf032bf4d588cfdc6769c7edc66
fetch "$ALPINE/x86_64/libcrypto3-3.5.6-r0.apk"  "$APKS/x86_64/libcrypto3-3.5.6-r0.apk"   9bf1548a563134be46e44048db645117b820b2dbc150ac406fbb627072f35046
fetch "$ALPINE/x86_64/libssl3-3.5.6-r0.apk"     "$APKS/x86_64/libssl3-3.5.6-r0.apk"      88f8491b5fe8d7cdd724f02ca5f98db2ed07839bdb96ca4c1c8799eb7ccb84c8
fetch "$ALPINE/x86_64/musl-1.2.5-r23.apk"       "$APKS/x86_64/musl-1.2.5-r23.apk"        4f3c4a7bf9f51d2c91007e333b17459362ffd881b4a343e8da07b6c50c4f4a0d
fetch "$ALPINE/x86_64/openssl-3.5.6-r0.apk"     "$APKS/x86_64/openssl-3.5.6-r0.apk"      d89963409d6e268ba0b84c8b13a5f2cf93a3fc0a893d4ed6af4280fef50e9736
fetch "$ALPINE/x86_64/pcre2-10.47-r0.apk"       "$APKS/x86_64/pcre2-10.47-r0.apk"        7880493ea1a743c97964e2444ef47649e43bc9fee25949c6712a8a03823985d3
fetch "$ALPINE/x86_64/zlib-1.3.2-r0.apk"        "$APKS/x86_64/zlib-1.3.2-r0.apk"         f56dea63692059bd65854bb179b7551971a441000b0d98c5b031291ca0450b56

# ---- aarch64 closure (angie body + 6 deps) ----
fetch "$ANGIE/aarch64/angie-1.11.5-r0.apk"      "$APKS/aarch64/angie-1.11.5-r0.apk"      2fb159974dd7da4618b10fb71cf682d8f8cbbe6af4197883538e9e3d61c9cee9
fetch "$ALPINE/aarch64/libcrypto3-3.5.6-r0.apk" "$APKS/aarch64/libcrypto3-3.5.6-r0.apk"  855413e1b69813a1d04ea50465a289cd6efbba7b77a40da2789dcd5f0fadb03d
fetch "$ALPINE/aarch64/libssl3-3.5.6-r0.apk"    "$APKS/aarch64/libssl3-3.5.6-r0.apk"     135e6b17ce8429b423dc31e084865d5975383890bac68e621cb1b09edbf98d06
fetch "$ALPINE/aarch64/musl-1.2.5-r23.apk"      "$APKS/aarch64/musl-1.2.5-r23.apk"       6a3edd924ead1fad88a69e28c5775809af3026b322f58428001cd02fedc5299e
fetch "$ALPINE/aarch64/openssl-3.5.6-r0.apk"    "$APKS/aarch64/openssl-3.5.6-r0.apk"     380e2775e8286c1ff66e8dccfa09370a01f5e981f620ab4d0a8e33fc2f6873b5
fetch "$ALPINE/aarch64/pcre2-10.47-r0.apk"      "$APKS/aarch64/pcre2-10.47-r0.apk"       4c324f11ee3a88c05df84c942eba633ac6e3a4b2d395a22f684e9085e2676463
fetch "$ALPINE/aarch64/zlib-1.3.2-r0.apk"       "$APKS/aarch64/zlib-1.3.2-r0.apk"        ecda4cc94fd18f90182f1d3a615889df5e0db9cf78926d11627dd23e06d2e6e8

echo "fetch-resources: gateway/angie OK"
