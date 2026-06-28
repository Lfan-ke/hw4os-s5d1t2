#!/bin/bash
# fetch-resources.sh — re-fetch the re-downloadable resources removed from this
# slimmed delivery tree for the `sdkman-0` StarryOS case.
#
# WHAT THIS RESTORES (exact paths/names that prep-sdkman-rootfs.sh expects):
#   package/apks/<arch>/*.apk    the 22-apk bash closure per arch (bash/curl/zip/
#                                unzip/readline/ncurses/musl/... ) — 4 arches × 22.
#
# What prep ACTUALLY consumes: the per-arch apk closures (above), plus the EXTRACTED
# SDKMAN framework package/zip-inspect/sdkman-5.23.0/ and package/candidates-all.csv
# (both KEPT in this tree). The two framework zips (sdkman-5.23.0-*.zip) were also
# removed but are NOT consumed by prep (only their extracted form is) — see the
# OPTIONAL block at the bottom for how to restore them by hand if desired.
#
# No SOURCES.md ships for this app; the apk sha256 values below are the content
# fingerprints of the originally-delivered artifacts (authoritative for this set).
#
# IMPORTANT — Alpine `edge` rotates: these apks come from Alpine edge/main, whose
# packages are re-spun frequently (the pinned -rN versions WILL 404 once Alpine
# bumps them). If a download 404s, fetch the CURRENT version of that package from
#   https://dl-cdn.alpinelinux.org/alpine/edge/main/<arch>/
# (the filename's -rN / version will differ). The exact -rN here is for byte-for-
# byte reproducibility; the case only needs a working bash+curl+zip+unzip closure.
# The embedded sha256 lets verification pass whenever the pinned version is still live.
#
# REQUIRES NETWORK. After running this, run prep-sdkman-rootfs.sh <arch>.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"

ALPINE="https://dl-cdn.alpinelinux.org/alpine"
APK_BRANCH="edge"
APK_REPO="main"

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

# apk_manifest <arch>: prints "<sha256>  <filename>" lines for the 22-apk closure.
apk_manifest() {
  case "$1" in
    x86_64) cat <<'EOF'
c52a2b6997567830d573160c2c0cad665473320f08862133aaf4c1e96864fc48  bash-5.3.3-r1.apk
09fbf1ddf0c71c6c4ab7bc12aef3452d8ace5adeab10c4c30214fc70f5e0e087  brotli-libs-1.2.0-r0.apk
2fea8c73710994099eaec6832b69a0f70163e0d6ee13848af3a5270a16d15c5c  busybox-1.37.0-r30.apk
94f10cf3bb79f033d5ac86675312229e28a8a858fa7bfb5268675951b99466e7  busybox-binsh-1.37.0-r30.apk
3e026e30e3d210d1a49c4d518f6c202dda9b652c484e53b49e4ebf8836ff3244  c-ares-1.34.6-r0.apk
75ba02b05c9487306a5c02735437382d82547aa949389e354323211c56c4d240  ca-certificates-bundle-20260413-r0.apk
52133a080359cae7d66678c61616193307e90a2c04ea3e220e2859fdbffd1caf  curl-8.19.0-r0.apk
9bf1548a563134be46e44048db645117b820b2dbc150ac406fbb627072f35046  libcrypto3-3.5.6-r0.apk
d1da25389adb248a4c2195f9989aadf3763c8fab7d1c8d15a7c69b6dab908c9d  libcurl-8.19.0-r0.apk
526275e11e449b99e5264e0ba32625fa12a78c426de61378f8b5cc0af5fc7014  libidn2-2.3.8-r0.apk
233f20a6d64d87afbc4b03ad3e7508f5d56af747508c395ba7a6c5f1033b15f0  libncursesw-6.5_p20251123-r0.apk
86ada309d460ea8768b340cbf83c7234a1564f8fea9c370fb19f028a2a165891  libpsl-0.21.5-r3.apk
88f8491b5fe8d7cdd724f02ca5f98db2ed07839bdb96ca4c1c8799eb7ccb84c8  libssl3-3.5.6-r0.apk
eca5eb66c8ac64cca7160fb766b15e885f75d18c15d0919d0adee987a53b9473  libunistring-1.4.1-r0.apk
4f3c4a7bf9f51d2c91007e333b17459362ffd881b4a343e8da07b6c50c4f4a0d  musl-1.2.5-r23.apk
9392f4d87fd9446c7c35dd1a29abe8e3566174e03bd75c848028e46829403f1f  ncurses-terminfo-base-6.5_p20251123-r0.apk
e52a5325b8720449ed4f123a33202794361b6b164c127abdb4b3c057467e7a41  nghttp2-libs-1.69.0-r0.apk
6d29b98eafff4aebaed4e61fca8329fbf7b26489a29fefc1d4462610d687303b  readline-8.3.1-r0.apk
f90e80016bfb7d17113d07a697cadf422f94b892123154e3479457685fc27388  unzip-6.0-r16.apk
21865fd8ef5549f14556604ccb6345b31aa2c7f34b12a2dd19293e8bf7bfb297  zip-3.0-r13.apk
f56dea63692059bd65854bb179b7551971a441000b0d98c5b031291ca0450b56  zlib-1.3.2-r0.apk
d5fcc58f7f071a5658d97b09838668e4443203217631992915b8e60656904104  zstd-libs-1.5.7-r2.apk
EOF
    ;;
    aarch64) cat <<'EOF'
5557dc5d3dbf3bb3e64f7aad4cfe535a6bffe4c2c13b81d527ef25dd6c438652  bash-5.3.3-r1.apk
c8a9113b0007d40291e55cfc196744b64004253707e37b3c07e188166fb2d0a0  brotli-libs-1.2.0-r0.apk
d3c919f1f142e41ba83c7dbcc392388f23f840dda6867719e660ebb9cc681254  busybox-1.37.0-r30.apk
311c832f7b05a545b8d18dbddff23b1d6d0c6085970735398064f91ed55cd575  busybox-binsh-1.37.0-r30.apk
731ca70363b83cc185daf14d5ed0c18c8ffe3bba8b16cbd2f44537da2050adaf  c-ares-1.34.6-r0.apk
f65ee4b7b7313908ef4d5c3011d182dae4ab524be12d4a877ba6d72cd1b7d7fb  ca-certificates-bundle-20260413-r0.apk
7b884212c16bbd89d032f28d38f0341339d0582068002cf37239631c79eb7ba1  curl-8.19.0-r0.apk
855413e1b69813a1d04ea50465a289cd6efbba7b77a40da2789dcd5f0fadb03d  libcrypto3-3.5.6-r0.apk
2049c1945eb17a9296e01f50ff4c83296f834a5732f9b22b5f2ffc7ef568fe96  libcurl-8.19.0-r0.apk
71db3591a2cb251aae463b966f9731c9ecae3bff4dccea552e0033002b92fd75  libidn2-2.3.8-r0.apk
7fe332c8af8b97579e89df56466f83f4abda5040a18e1842c07d684320bbef4b  libncursesw-6.5_p20251123-r0.apk
3078398181f0955da453826965a9e184494e4d9dfd22391e28d72f7561a0ca2b  libpsl-0.21.5-r3.apk
135e6b17ce8429b423dc31e084865d5975383890bac68e621cb1b09edbf98d06  libssl3-3.5.6-r0.apk
03f26b1c897ad74af51b475d5f6cebad50efe90e4d42a0c38ffcb37859077481  libunistring-1.4.1-r0.apk
6a3edd924ead1fad88a69e28c5775809af3026b322f58428001cd02fedc5299e  musl-1.2.5-r23.apk
6952a6b39abaf7bbc498cb085f0f59bf23619b53b3f7328b08fe50c0198a2bd4  ncurses-terminfo-base-6.5_p20251123-r0.apk
2e17aa1d7d4a37b2d029210d239b8b2e4ac40748d9a8c8912a6b607011034fd1  nghttp2-libs-1.69.0-r0.apk
70d288a6c3d8daf19b10fb220120d2ebd6f154011c96ca5bc84923b658cb21f7  readline-8.3.1-r0.apk
74ad234c9438f631bb9cbcea5e1027ba4b1f67d0ed34db8edc77079abb3b94d7  unzip-6.0-r16.apk
7444a01c4db359f432e1a08e908ebb043a402aa9e461be832b254d408489927b  zip-3.0-r13.apk
ecda4cc94fd18f90182f1d3a615889df5e0db9cf78926d11627dd23e06d2e6e8  zlib-1.3.2-r0.apk
50a998e56e4bf31504996e49c1b4a23eed5e0cd4e24b8a1356a54daf1a6eabbe  zstd-libs-1.5.7-r2.apk
EOF
    ;;
    riscv64) cat <<'EOF'
f650ed99533df97a3f641e3ef5683c5de1e641516814818f9b6822f266e92d1d  bash-5.3.3-r1.apk
f4f5e305b3912640274a1655427124ec5151a08bbf0cd60941b82b0c034a6d46  brotli-libs-1.2.0-r0.apk
76b26b7cd90f494c2e9520d0b553c928cc70375852ea396e9ba51c1ca984aefb  busybox-1.37.0-r30.apk
393187f7d98f9b761b20c3d452f635e9c19be6972b4626750ba7b657d3c20895  busybox-binsh-1.37.0-r30.apk
b45556a99db45b2599ddd0341d27677fa842814551140c7524032c89d5360a7a  c-ares-1.34.6-r0.apk
5a6e8fe00d520eb55ea9063607385886862ad818864fb0a8637d35d79b28bb52  ca-certificates-bundle-20260413-r0.apk
4308b1d2e09fd5ebc51b2c222f9178b7ca6ed15575dfcbf4015163fe1d0f01c9  curl-8.19.0-r0.apk
3eed2e314a33217db77c763ec41766954c67c77f6f7acb5130c521805a0769b6  libcrypto3-3.5.6-r0.apk
f4f9c03f18deaea9897b4e91e037c76023af8fd5189ca46dc134a58d911edb9a  libcurl-8.19.0-r0.apk
4e6747ee410fa08a8e35464e02cdf02bf2e1b1510fb773e5f97d1ba60284908d  libidn2-2.3.8-r0.apk
bfb0d92e6a28296db18289330f9e7aece182e57a9d02f95be5e1a67ab573b86d  libncursesw-6.5_p20251123-r0.apk
4e7a5675a0efeff4cb203f201d3726cf79835ddad699d14bbaaf75985255643b  libpsl-0.21.5-r3.apk
798126f13b5f4f24979db6144784388530b0e668e3871502d397dd3fdcfe7d91  libssl3-3.5.6-r0.apk
9f60b39bab32ebfa3456c0d6a354cf9a56be3aeb55dbdcfbd33157d3e223dfae  libunistring-1.4.1-r0.apk
149223dc8187f32427a5312eb07b64ecd23e011e24ce647632d00a0352d2d336  musl-1.2.5-r23.apk
2640c0e0fa262998402aae97d967d909275f943645e16c8d80d4b1e100b05626  ncurses-terminfo-base-6.5_p20251123-r0.apk
6edc919ab4e58dc8307090a33201294a074636e3582993a681c39eb08f5da0d5  nghttp2-libs-1.69.0-r0.apk
962e281e51ffefcf0f6a2fa856717ccc20772dbd54e41d8398d845ff3f08dafc  readline-8.3.1-r0.apk
97525a1fdafaaa6a869c6acd60c5738a484f3d4b3db44b094cf94952b2f2f2f4  unzip-6.0-r16.apk
ce2c837e346a11c4050d674b3d05eac5cc87f8e686a29136fb9c4f81c6055150  zip-3.0-r13.apk
be32b765504403fa5f702bb81cdb056e062d2b88f13ec88f6fba9acb7e7ce297  zlib-1.3.2-r0.apk
2d0163aa7864b99f29e36c4e167cef0a3bb4b9d25caeb0b56cc928b4c5879999  zstd-libs-1.5.7-r2.apk
EOF
    ;;
    loongarch64) cat <<'EOF'
9b5394fcc4c2275716d053bd13692517c480ace6ebf5641149146a4d6379917f  bash-5.3.3-r1.apk
b6ac6f2bbdc3fe35c2f997b4766132d7febf7df893ed0f8bb29bd6f9254024e3  brotli-libs-1.2.0-r0.apk
7806fe4ab1fcfc864d129a6055effdeef16b142c961493e00e50a430ab92b922  busybox-1.37.0-r30.apk
2cf9078f0b1120b34a4670a97905dd2f1c8a1719f7b0d18a6808b5be657ba72d  busybox-binsh-1.37.0-r30.apk
db2261a3f95a167f398058ab8b65abbcc9654c114b4444d76d7fee028e0a7f0c  c-ares-1.34.6-r0.apk
39c0524cb2e158124c484fca5f2bf9f71a8999dbf983dd2bcfb3fe18dc4454bb  ca-certificates-bundle-20260413-r0.apk
2a034a8997768c5a507f3f04978487734a952c11e7da54fe001012edff952ba8  curl-8.19.0-r0.apk
928690b2bce3e56eaf250bb3bbacafa5781004696228c2ed78fb9dbd79bff6b4  libcrypto3-3.5.6-r0.apk
d02b92a7251f30ae18aa568404fcf81ea790fd3cf8f86e7b4cd13c250bc09693  libcurl-8.19.0-r0.apk
b230ddccc5035e5bc88c08aba74c9d42214d8423271e6da12c3cea43e5f9daf2  libidn2-2.3.8-r0.apk
40e680201380bbdac47cef49cf2a0d0beb88dd952ff4213406cc98b40fd996b8  libncursesw-6.5_p20251123-r0.apk
50a271d827c2ca8ab7e8e78567ace81e5624477135b69dec0247a8fd9ce96287  libpsl-0.21.5-r3.apk
70c068a4a83240bdb54d03f7ed5d5056912c87aeece75bbf55309b93412e985d  libssl3-3.5.6-r0.apk
8533eee71cc9a9e0fce57bbafec99c6e44fff53169757297939ed3e841351946  libunistring-1.4.1-r0.apk
4b845a5d5b20428e1c9dd174d94593f5e14be073468d1b0adc2ff981bed7125a  musl-1.2.5-r23.apk
381cb22714898d7cdab07cd30227043c37697d49c1c8b7dcbdb83b662d9fc3f7  ncurses-terminfo-base-6.5_p20251123-r0.apk
f0bb8e5a5eb737d1b266c0d380ee100f32884af1cbab781bca6c222f2bf8c93c  nghttp2-libs-1.69.0-r0.apk
2891de8cfe1683d21907d467f4406f3c5592bb4b746a8fb94afc536ccadc7a82  readline-8.3.1-r0.apk
9c15e45c3f055d32d1b1c6f433bfef4ade7580732669cdd3d23ff13eb92c7b02  unzip-6.0-r16.apk
32c9d968f5a1696e0a4d4e2d79b3366d46b2215736b166a44767300320ef1611  zip-3.0-r13.apk
87f53f76fb1b91798984d9f05e43ff5ced7522206f264c779c3ca5c72d430453  zlib-1.3.2-r0.apk
cf4d180826effdd0420e00fae8e09ad2e123d58e01c60286f7a68ba1325c9402  zstd-libs-1.5.7-r2.apk
EOF
    ;;
    *) return 1 ;;
  esac
}

for arch in x86_64 aarch64 riscv64 loongarch64; do
  echo "=== sdkman: bash apk closure -> package/apks/$arch/ ==="
  while read -r sha file; do
    [ -z "${file:-}" ] && continue
    fetch "$ALPINE/$APK_BRANCH/$APK_REPO/$arch/$file" \
          "$HERE/package/apks/$arch/$file" "$sha"
  done < <(apk_manifest "$arch")
done

# --- OPTIONAL: the two SDKMAN framework zips (NOT consumed by prep) ------------
# prep-sdkman-rootfs.sh uses the EXTRACTED framework at
#   package/zip-inspect/sdkman-5.23.0/   (KEPT in this tree)
# so the zips below are NOT required to build the rootfs. They were the original
# download source. Both files are byte-identical (same sha256). SDKMAN serves the
# framework through its broker/API rather than a stable public file URL, so a
# reliable canonical download URL for these exact filenames CANNOT be determined
# here — restore them by hand if you want the originals back, then verify:
#   dest: package/sdkman-5.23.0-linuxx64.zip
#   dest: package/sdkman-5.23.0-exotic.zip
#   sha256 (both): 7ef83583a6986351ea8c86b8494a885fcae91a2fbfac91662bca7ea4f72bd230
# (Fetch the SDKMAN 5.23.0 framework zip via the SDKMAN install/self-update flow
#  documented at sdkman.io, then `sha256sum` against the value above.)

echo "fetch-resources: sdkman OK"
