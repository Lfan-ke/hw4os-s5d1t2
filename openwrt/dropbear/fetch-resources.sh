#!/bin/bash
# fetch-resources.sh — re-fetch the redistributable apk packages that were removed
# when this delivery repo was slimmed down. Requires NETWORK ACCESS. After it
# finishes, run the prep script as usual:
#   bash prep-openwrt-rootfs.sh <arch>     # arch in x86_64|aarch64|riscv64|loongarch64
#
# All resources are Alpine Linux v3.23/main musl-native apks (the dropbear + dnsmasq
# dependency closure), fetched for all four arches from the official Alpine CDN:
#   https://dl-cdn.alpinelinux.org/alpine/v3.23/main/<arch>/<file>.apk
# Provenance/version matrix: openwrt-apks/SOURCES.md (download-side record). The apk
# files carry no published sha256 sidecar there, so the sha256 values below are the
# exact hashes of the delivered artifacts (the bytes that were tested/shipped), used
# strictly as integrity guards for the re-download.
#
# NOTE: Alpine's dl-cdn only serves the CURRENT package revision for a release
# branch. If a pinned -rN apk has been superseded the URL may 404 / mismatch; in
# that case pull the exact revision from an Alpine archive mirror. The sha256
# check below guards against any wrong/rotated file.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
APKS="$HERE/apks"
ALPINE="https://dl-cdn.alpinelinux.org/alpine/v3.23/main"

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

# ---- x86_64 ----
fetch "$ALPINE/x86_64/busybox-1.37.0-r30.apk"        "$APKS/x86_64/busybox-1.37.0-r30.apk"        2fea8c73710994099eaec6832b69a0f70163e0d6ee13848af3a5270a16d15c5c
fetch "$ALPINE/x86_64/busybox-binsh-1.37.0-r30.apk"  "$APKS/x86_64/busybox-binsh-1.37.0-r30.apk"  94f10cf3bb79f033d5ac86675312229e28a8a858fa7bfb5268675951b99466e7
fetch "$ALPINE/x86_64/dnsmasq-2.91-r1.apk"           "$APKS/x86_64/dnsmasq-2.91-r1.apk"           905ba0cdf2d6c30beed3c3e597765206f52f11a70d2dcae843d2572ec6e89768
fetch "$ALPINE/x86_64/dnsmasq-common-2.91-r1.apk"    "$APKS/x86_64/dnsmasq-common-2.91-r1.apk"    766427b6d987fdd87757e1d3f34f9d190ed8e9d20df8b0a28c8d2481d7b97c8f
fetch "$ALPINE/x86_64/dropbear-2025.88-r1.apk"       "$APKS/x86_64/dropbear-2025.88-r1.apk"       e08540b3a075fe430d686f018a2d5c6dc4619f5ad521220d44c2463ded1e6120
fetch "$ALPINE/x86_64/musl-1.2.5-r23.apk"            "$APKS/x86_64/musl-1.2.5-r23.apk"            4f3c4a7bf9f51d2c91007e333b17459362ffd881b4a343e8da07b6c50c4f4a0d
fetch "$ALPINE/x86_64/skalibs-libs-2.14.4.0-r0.apk"  "$APKS/x86_64/skalibs-libs-2.14.4.0-r0.apk"  1f502dd6a2a69609fbecde4bbf1684cfc8aac0130275a9746fcd907f209ec4d7
fetch "$ALPINE/x86_64/utmps-libs-0.1.3.1-r0.apk"     "$APKS/x86_64/utmps-libs-0.1.3.1-r0.apk"     8fd3aaf2c4cafd744c844eeabaea52d6dbd2b801d568eaadf4392611f48aa65e
fetch "$ALPINE/x86_64/zlib-1.3.2-r0.apk"             "$APKS/x86_64/zlib-1.3.2-r0.apk"             f56dea63692059bd65854bb179b7551971a441000b0d98c5b031291ca0450b56

# ---- aarch64 ----
fetch "$ALPINE/aarch64/busybox-1.37.0-r30.apk"       "$APKS/aarch64/busybox-1.37.0-r30.apk"       d3c919f1f142e41ba83c7dbcc392388f23f840dda6867719e660ebb9cc681254
fetch "$ALPINE/aarch64/busybox-binsh-1.37.0-r30.apk" "$APKS/aarch64/busybox-binsh-1.37.0-r30.apk" 311c832f7b05a545b8d18dbddff23b1d6d0c6085970735398064f91ed55cd575
fetch "$ALPINE/aarch64/dnsmasq-2.91-r1.apk"          "$APKS/aarch64/dnsmasq-2.91-r1.apk"          ee0643977bc4db3ca618d3bca6ec31d1cecd28b3fc911f5cc9f2f434c299d83a
fetch "$ALPINE/aarch64/dnsmasq-common-2.91-r1.apk"   "$APKS/aarch64/dnsmasq-common-2.91-r1.apk"   2a0241f086d6778a757992009c1dcf45fae9c53e53674f4e975dcc87f92174d5
fetch "$ALPINE/aarch64/dropbear-2025.88-r1.apk"      "$APKS/aarch64/dropbear-2025.88-r1.apk"      bbf6110ac6fb8498da5a42e12ed13c7e21d22a93d1be0a706d7e1dc6ac489230
fetch "$ALPINE/aarch64/musl-1.2.5-r23.apk"           "$APKS/aarch64/musl-1.2.5-r23.apk"           6a3edd924ead1fad88a69e28c5775809af3026b322f58428001cd02fedc5299e
fetch "$ALPINE/aarch64/skalibs-libs-2.14.4.0-r0.apk" "$APKS/aarch64/skalibs-libs-2.14.4.0-r0.apk" f1c33c73d842c98d99033833eaf681819b0f5b51310a6e286c2eb88b9a88ed97
fetch "$ALPINE/aarch64/utmps-libs-0.1.3.1-r0.apk"    "$APKS/aarch64/utmps-libs-0.1.3.1-r0.apk"    dbaa260e4328738d51b2ec77fdda9037cdbd67047465de5b2a30dce9a66f8fff
fetch "$ALPINE/aarch64/zlib-1.3.2-r0.apk"            "$APKS/aarch64/zlib-1.3.2-r0.apk"            ecda4cc94fd18f90182f1d3a615889df5e0db9cf78926d11627dd23e06d2e6e8

# ---- riscv64 ----
fetch "$ALPINE/riscv64/busybox-1.37.0-r30.apk"       "$APKS/riscv64/busybox-1.37.0-r30.apk"       76b26b7cd90f494c2e9520d0b553c928cc70375852ea396e9ba51c1ca984aefb
fetch "$ALPINE/riscv64/busybox-binsh-1.37.0-r30.apk" "$APKS/riscv64/busybox-binsh-1.37.0-r30.apk" 393187f7d98f9b761b20c3d452f635e9c19be6972b4626750ba7b657d3c20895
fetch "$ALPINE/riscv64/dnsmasq-2.91-r1.apk"          "$APKS/riscv64/dnsmasq-2.91-r1.apk"          49a30c7354da18f80c2ae689027a6f535d6ee8e9540ea3c58966884fcad7ae9e
fetch "$ALPINE/riscv64/dnsmasq-common-2.91-r1.apk"   "$APKS/riscv64/dnsmasq-common-2.91-r1.apk"   d78cdd2114f95ab4c5ccb796bc3966b42fe8a88b45eb414e75c60699cce5c363
fetch "$ALPINE/riscv64/dropbear-2025.88-r1.apk"      "$APKS/riscv64/dropbear-2025.88-r1.apk"      95bd0afe2a4e2c76fbcbe4efc1a8cf308724fa82b15412ff697ecc729df74c89
fetch "$ALPINE/riscv64/musl-1.2.5-r23.apk"           "$APKS/riscv64/musl-1.2.5-r23.apk"           149223dc8187f32427a5312eb07b64ecd23e011e24ce647632d00a0352d2d336
fetch "$ALPINE/riscv64/skalibs-libs-2.14.4.0-r0.apk" "$APKS/riscv64/skalibs-libs-2.14.4.0-r0.apk" d79c3e528896f2bcf3131e9bcb24013f1d3ab7ba03f1274c36104573914022c8
fetch "$ALPINE/riscv64/utmps-libs-0.1.3.1-r0.apk"    "$APKS/riscv64/utmps-libs-0.1.3.1-r0.apk"    bd9db243b7bcd4272736516d9d6c08b3c823055947f943dd7cf2d16136fa350c
fetch "$ALPINE/riscv64/zlib-1.3.2-r0.apk"            "$APKS/riscv64/zlib-1.3.2-r0.apk"            be32b765504403fa5f702bb81cdb056e062d2b88f13ec88f6fba9acb7e7ce297

# ---- loongarch64 ----
fetch "$ALPINE/loongarch64/busybox-1.37.0-r30.apk"       "$APKS/loongarch64/busybox-1.37.0-r30.apk"       7806fe4ab1fcfc864d129a6055effdeef16b142c961493e00e50a430ab92b922
fetch "$ALPINE/loongarch64/busybox-binsh-1.37.0-r30.apk" "$APKS/loongarch64/busybox-binsh-1.37.0-r30.apk" 2cf9078f0b1120b34a4670a97905dd2f1c8a1719f7b0d18a6808b5be657ba72d
fetch "$ALPINE/loongarch64/dnsmasq-2.91-r1.apk"          "$APKS/loongarch64/dnsmasq-2.91-r1.apk"          365c73cc692f8b9deb9f07bf5cdaee9cf429083f97ed1fda31722cabe6b8acac
fetch "$ALPINE/loongarch64/dnsmasq-common-2.91-r1.apk"   "$APKS/loongarch64/dnsmasq-common-2.91-r1.apk"   068188f70e72d4be13c025defec3e03373a82a185a430ce66d84223dab042108
fetch "$ALPINE/loongarch64/dropbear-2025.88-r1.apk"      "$APKS/loongarch64/dropbear-2025.88-r1.apk"      f6066a8d35fa717d886f5be9d07779a2e4602e17f13cd69a1456da1714ba3684
fetch "$ALPINE/loongarch64/musl-1.2.5-r23.apk"           "$APKS/loongarch64/musl-1.2.5-r23.apk"           4b845a5d5b20428e1c9dd174d94593f5e14be073468d1b0adc2ff981bed7125a
fetch "$ALPINE/loongarch64/skalibs-libs-2.14.4.0-r0.apk" "$APKS/loongarch64/skalibs-libs-2.14.4.0-r0.apk" f8aa482cfa52f3c27c3126d80450c49249ec9247ef4a4a0183ae20a50eab4c98
fetch "$ALPINE/loongarch64/utmps-libs-0.1.3.1-r0.apk"    "$APKS/loongarch64/utmps-libs-0.1.3.1-r0.apk"    6a2acff904a7f498051988dada561456d06cb4da8c874941eb8dc2bf0898eb13
fetch "$ALPINE/loongarch64/zlib-1.3.2-r0.apk"            "$APKS/loongarch64/zlib-1.3.2-r0.apk"            87f53f76fb1b91798984d9f05e43ff5ced7522206f264c779c3ca5c72d430453

echo "fetch-resources: openwrt/dropbear OK"
