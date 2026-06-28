#!/bin/bash
# fetch-resources.sh — re-fetch the large, re-downloadable resources that were
# removed when this delivery tree was slimmed down. Needs network access.
# After this completes, run the prep scripts to build the rootfs images:
#   bash prep-nodejs-rootfs.sh       <arch>   (base node runtime: extracts ALL apks/<arch>/*.apk)
#   bash prep-nodejs-lite-rootfs.sh  <arch>   (builds on the node base; uses host $NODEJS_FW)
#   bash prep-nodejs-pm-rootfs.sh    <arch>   (builds on the node base; uses yarn + icu apks)
#   bash prep-nodejs-tools-rootfs.sh <arch>   (builds on the node base; uses icu-data-full apk)
#
# Removed resources (consumed by the prep scripts as $HERE/apks/<arch>/*.apk):
#   The Node.js v22.22.2 musl-native runtime + its 20-apk dependency closure, the
#   SAME 20 package versions for each of the 4 architectures (x86_64, aarch64,
#   riscv64, loongarch64) = 80 .apk files total. prep-nodejs-rootfs.sh extracts the
#   WHOLE closure into /usr; prep-nodejs-pm/-tools additionally pick out yarn-* and
#   icu-data-full-* by name. All 20 per arch are required.
#
# NOT fetched here (NOT part of this slim app dir — supplied separately by the host):
#   The per-case test material (pp-standalone / cjs-esm / kotlin-js / pm-sample /
#   tools-suite node_modules + REF outputs) is provided through the $NODEJS_FW host
#   directory referenced by the lite/pm/tools prep scripts; it is host-vendored with
#   npm/yarn and never lived in this tree.
#
# SOURCE / provenance (see SOURCES.md "Node runtime closure"):
#   https://dl-cdn.alpinelinux.org/alpine/edge/{main,community}/<arch>/<pkg>-<ver>.apk
#   SOURCES.md does NOT record a per-file sha256 and does NOT say which of the two
#   repos (main vs community) each package lives in. This script therefore:
#     * tries the documented branch's main AND community repo for each package, and
#     * verifies every download against the sha256 of the exact artifact that was
#       shipped in the full delivery tree (computed from those artifacts; this is the
#       authoritative integrity anchor, NOT a fabricated value).
#
# IMPORTANT CAVEAT — Alpine "edge" is a rolling branch. The pinned versions below may
# have been superseded/pruned from the live CDN by the time you run this; then both
# main and community will 404 (or a newer version will fail the sha256 check, which is
# correct — it is NOT the shipped artifact). If that happens, recover the exact pinned
# versions via Alpine's signed package index instead, e.g.:
#     apk fetch --no-cache --arch <arch> -R \
#         --repository https://dl-cdn.alpinelinux.org/alpine/edge/main \
#         --repository https://dl-cdn.alpinelinux.org/alpine/edge/community \
#         nodejs yarn
#   (apk verifies the Alpine signatures cryptographically), or pull the pinned files
#   from an Alpine edge archive mirror, dropping them into apks/<arch>/ by the exact
#   filenames listed below, then re-run this script to confirm the sha256.
# You may override the branch (e.g. a release branch carrying these versions) with
#   APK_BRANCH=v3.22 bash fetch-resources.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"

# Branch + repos to search (SOURCES.md: edge/{main,community}). Override branch via env.
APK_BRANCH="${APK_BRANCH:-edge}"
APK_MIRROR="${APK_MIRROR:-https://dl-cdn.alpinelinux.org/alpine}"
REPOS=(main community)

# The 20-package Node v22.22.2 musl runtime closure (same filenames every arch).
PKGS=(
  ada-libs-2.9.2-r4.apk
  brotli-libs-1.1.0-r2.apk
  c-ares-1.34.6-r0.apk
  ca-certificates-20260413-r0.apk
  ca-certificates-bundle-20260413-r0.apk
  icu-data-full-76.1-r1.apk
  icu-libs-76.1-r1.apk
  libcrypto3-3.5.6-r0.apk
  libgcc-14.2.0-r6.apk
  libssl3-3.5.6-r0.apk
  libstdc++-14.2.0-r6.apk
  musl-1.2.5-r12.apk
  nghttp2-libs-1.69.0-r0.apk
  nodejs-22.22.2-r0.apk
  simdjson-3.12.0-r0.apk
  simdutf-7.2.1-r0.apk
  sqlite-libs-3.49.2-r1.apk
  yarn-1.22.22-r1.apk
  zlib-1.3.2-r0.apk
  zstd-libs-1.5.7-r0.apk
)

ARCHES=(x86_64 aarch64 riscv64 loongarch64)

# Authoritative sha256, keyed "<arch>:<file>" — the checksum of each artifact as it was
# shipped in the full delivery tree. (SOURCES.md records no per-file sha256.)
declare -A SHA
# --- x86_64 ---
SHA[x86_64:ada-libs-2.9.2-r4.apk]=183fd6d3164c8236b578b410d5ad9b19204d7d06f3dd60075c6a5466289fd483
SHA[x86_64:brotli-libs-1.1.0-r2.apk]=a693524543421b3a90f163ccb48d5ad0f5fd773b5c3b640acc461eace2cb01b6
SHA[x86_64:c-ares-1.34.6-r0.apk]=102006a36777a55d805339eb8799f5ff0819548c309710ade9eb01e3247306e2
SHA[x86_64:ca-certificates-20260413-r0.apk]=64d363241ef63d20b12fc2d540958d46af9c5bd879a1fe744698b5aa2e0b24d0
SHA[x86_64:ca-certificates-bundle-20260413-r0.apk]=b227b13e7151a8506be85809188d83cfe5a825796f4ea27e6e1e10f27b4a7d62
SHA[x86_64:icu-data-full-76.1-r1.apk]=478c438c41359350c824b5276e52cba60da36484eff68f4e6d43e9cbdd462415
SHA[x86_64:icu-libs-76.1-r1.apk]=0d6d3b9173f388f7450b168f8a98647f21356d1d4e5ae414e160db2843c6a2c8
SHA[x86_64:libcrypto3-3.5.6-r0.apk]=ae361b948361ca4ab9236500d90397797fd93ec7aab70929cc47a5f1e698ad01
SHA[x86_64:libgcc-14.2.0-r6.apk]=04f3467bc967e705221a843fe4d3de5850db826e571686e0c0ed453d38cb5c59
SHA[x86_64:libssl3-3.5.6-r0.apk]=424fce8597ac1d81fe1176283b8568c90713b6de85f80d467e4d415a532b017c
SHA[x86_64:libstdc++-14.2.0-r6.apk]=939f7c99898f3e8154207a17f4acbe8bc40437e1bb1b43f5525620ca9e452a2e
SHA[x86_64:musl-1.2.5-r12.apk]=4990a5e0ba312e478f94cfe431a70efef1538004eb361c8ae424516848be45bb
SHA[x86_64:nghttp2-libs-1.69.0-r0.apk]=d6ee515d30d703e94b55ef9c0a02aea053313f654acbd2c655d18e3907ecb66a
SHA[x86_64:nodejs-22.22.2-r0.apk]=a53bcd8008811eb674545aa7a65c1fd4528bc00e14b499019805658c4bb3c80c
SHA[x86_64:simdjson-3.12.0-r0.apk]=e051fefd290e6b3658e5510a09785df6db2a2b2291f69f6ddfceefc03d7a811d
SHA[x86_64:simdutf-7.2.1-r0.apk]=a29bb33167c1e2d478dab9e83172a8a455db41d62dcdfe34f680c4f18572bea0
SHA[x86_64:sqlite-libs-3.49.2-r1.apk]=7af9a58595642de91ea35d3d070cd38d969680a720ce76b14966859e6fb4af99
SHA[x86_64:yarn-1.22.22-r1.apk]=55aa3775e939b6da92de128f7aec7ef5a1aea561d66c71b753ccd73972cbba03
SHA[x86_64:zlib-1.3.2-r0.apk]=1f3d5f463f490dad3a68097376711bfe5e8156e9e8daff3070513aa4378cdeca
SHA[x86_64:zstd-libs-1.5.7-r0.apk]=1bdd6e57cfbfbfd6e8481cad37ddd5d199950715bec1879b3afb600272dbb09e
# --- aarch64 ---
SHA[aarch64:ada-libs-2.9.2-r4.apk]=58147891c4ae32752fd81792dfec19c71b8d88661c4aa30db3f26600df33bb28
SHA[aarch64:brotli-libs-1.1.0-r2.apk]=b05f9d2839bb89f28325890ffbd7d94025af6b301344ae0255f08617a6036c65
SHA[aarch64:c-ares-1.34.6-r0.apk]=364ef8cdf856f127e5de14ce11df33e72418620e7a41be6746a7804a96d2c1e2
SHA[aarch64:ca-certificates-20260413-r0.apk]=5647df38d9458e1fcc520733114fbfe3260c6d12b338c5d8bf534004bd6edd0f
SHA[aarch64:ca-certificates-bundle-20260413-r0.apk]=319e8842d01f1633115201f4ece26f8c672fddbc43242f3260ececea87ba8f52
SHA[aarch64:icu-data-full-76.1-r1.apk]=478c438c41359350c824b5276e52cba60da36484eff68f4e6d43e9cbdd462415
SHA[aarch64:icu-libs-76.1-r1.apk]=6c9dd2e6b0ddc6e7d5fd2a21b427799d7ca4f7e8b5aad72d17e84520db3cd249
SHA[aarch64:libcrypto3-3.5.6-r0.apk]=b68704979608806579131804808fd8facd07166c69080816000810ba57d7db3b
SHA[aarch64:libgcc-14.2.0-r6.apk]=ba1835eec3ad8a120efd3d5020e561d53553a0513763a08f509e3ce6d4baa9ca
SHA[aarch64:libssl3-3.5.6-r0.apk]=15346fbe85ee862a37cd07f515d128b9ceb5f1228cc652fa254d80830652237a
SHA[aarch64:libstdc++-14.2.0-r6.apk]=0d2f054057a4f932e985a129eccb79908b40964185139a0a609aed3032aba064
SHA[aarch64:musl-1.2.5-r12.apk]=ac281d1e7f9e9c447c51e309317b975f48be6edaf3ab91ae73b959cf86703782
SHA[aarch64:nghttp2-libs-1.69.0-r0.apk]=19db967a36f1e041e96240484d94063354f5c837e36d50daa017c2e96393a5e7
SHA[aarch64:nodejs-22.22.2-r0.apk]=279805e0d711a3adf1459ee53ae086e914a3da51f7d4c65cf5731d5105460567
SHA[aarch64:simdjson-3.12.0-r0.apk]=5605c691ab62e5a0071d065b5afdd5c3740d763821d689a6bf54e46c95916974
SHA[aarch64:simdutf-7.2.1-r0.apk]=b20688ad72d096ba903bc77ea92165153427dcf5d25b5b9dcf27e7b4ca7046b9
SHA[aarch64:sqlite-libs-3.49.2-r1.apk]=204910bcbb13df4d517cb01acb178ebe14f12ff0e55a04b38d1565941780ee29
SHA[aarch64:yarn-1.22.22-r1.apk]=4cd934937550eb528f524f41303e25a2c8216966feb5bec2b6e1d394330d4f10
SHA[aarch64:zlib-1.3.2-r0.apk]=7a39a917e4dab3c7a45537210ee5b5f17bf75f5e7777809a20cddd0afe074187
SHA[aarch64:zstd-libs-1.5.7-r0.apk]=a0e92d2225941a514eb0b2325b137fe6444ef9171627aae8129b74a6ad934ac4
# --- riscv64 ---
SHA[riscv64:ada-libs-2.9.2-r4.apk]=e3a2359d3ccf07d25f2e4a7aa2efa9061ca028ca610821943999298057eaff48
SHA[riscv64:brotli-libs-1.1.0-r2.apk]=692bbfa741115c9f3bebfb6779b837aad2c9bf7b5eb15de2a65499b25c1622fe
SHA[riscv64:c-ares-1.34.6-r0.apk]=bc2e42e60ba19d9c94f1918abe50bd67afa1a340a380b2e8e78585fd83fbeae6
SHA[riscv64:ca-certificates-20260413-r0.apk]=cc6da281a722ffe59b234def95db891822684352c804388ba3251c3751d5ba0d
SHA[riscv64:ca-certificates-bundle-20260413-r0.apk]=8ffb430bc7a92b5c06c7d132e3a6e01737e097e8d4f7bada34dcf071e423eaac
SHA[riscv64:icu-data-full-76.1-r1.apk]=478c438c41359350c824b5276e52cba60da36484eff68f4e6d43e9cbdd462415
SHA[riscv64:icu-libs-76.1-r1.apk]=b74313308881ac038f6669c76b6b1fffa6636a8512ae82d3584daac0f526a1af
SHA[riscv64:libcrypto3-3.5.6-r0.apk]=05be654995f0b55300aef8d5001a82637a4f5e9cc51914f6dfbbc6fe35bbc6b8
SHA[riscv64:libgcc-14.2.0-r6.apk]=3962255abb95c10d1400883daf823cab3d04779ea8886a8e44d61c98caa8ee55
SHA[riscv64:libssl3-3.5.6-r0.apk]=035aff6c6bc86e99a05874246167ac307e2477169785172d930cb8857c7c2c1a
SHA[riscv64:libstdc++-14.2.0-r6.apk]=01f39aabb27a201645ac274c3e54019c6a5f1f8658d814edbaca79c6c96a595f
SHA[riscv64:musl-1.2.5-r12.apk]=6814d9cbaad929d14181ef4fbd1d65c7749df43746269b9bdb75551ba32a79db
SHA[riscv64:nghttp2-libs-1.69.0-r0.apk]=3d554fbada551f44c8e1dda4581cb531789e7ebd80a6f3aaf97e0e95280d993d
SHA[riscv64:nodejs-22.22.2-r0.apk]=e7c42581eb772394980b16a2b1cc6439b27715f90c64b11f88d92917ef7b2d55
SHA[riscv64:simdjson-3.12.0-r0.apk]=33bad0b8ee5c525eec0b3ecba063de4222ab6d1cfe8191aa345b08ee35350d5b
SHA[riscv64:simdutf-7.2.1-r0.apk]=5c463ccc76b336223dd42ce57d3f52815cbab980fa17080264984611ab5b3294
SHA[riscv64:sqlite-libs-3.49.2-r1.apk]=fe76441a09fee8f2926db452762b0186ffb30fd7f083d445f4da4c7a86da9560
SHA[riscv64:yarn-1.22.22-r1.apk]=ba35e325123606247687b88bbb57be6a217382523fafcb63e8264a9152c47600
SHA[riscv64:zlib-1.3.2-r0.apk]=9a2761a457312f4aa1312c94d3ca8789c2f1dd51d34d992e400851c8181a6887
SHA[riscv64:zstd-libs-1.5.7-r0.apk]=28bb837f617870a996009130c938ad12075f942738cfcd1390251720c78f0b8d
# --- loongarch64 ---
SHA[loongarch64:ada-libs-2.9.2-r4.apk]=7c1a61d9f84d11f4dfddbba92a6eabd7969a9791be7607badde40a831493d9c8
SHA[loongarch64:brotli-libs-1.1.0-r2.apk]=ad97f63766c68cfe14a773cabb9b8ef71d93348654800e2532bded2e154249e9
SHA[loongarch64:c-ares-1.34.6-r0.apk]=356aaceab411a594c8a6679f3d301dee89b75065dc57fc8a64e0a7a1a689aef2
SHA[loongarch64:ca-certificates-20260413-r0.apk]=10610fe1281072b4abe3a7be54affde1b9e59b7206a2b119fba28e2e711ca34e
SHA[loongarch64:ca-certificates-bundle-20260413-r0.apk]=96fa9a6e477720c47404de579e7aa7db7e2ba5c345961d8e4957c4d112607bb7
SHA[loongarch64:icu-data-full-76.1-r1.apk]=478c438c41359350c824b5276e52cba60da36484eff68f4e6d43e9cbdd462415
SHA[loongarch64:icu-libs-76.1-r1.apk]=8a93269e6699a102048609ce426989044685c7d6ec5f661aaf24072e96704167
SHA[loongarch64:libcrypto3-3.5.6-r0.apk]=af2bb6e33c686e4c1774a4d46a1410a0b6cc681fff57043d708422822bb92acf
SHA[loongarch64:libgcc-14.2.0-r6.apk]=2fe75802b0aab66e979821d35181142dfd3e1f3d6a052ad4edec4eff93172946
SHA[loongarch64:libssl3-3.5.6-r0.apk]=4b67ef49ded1f189890aff14ca3375d5ac4884db6e473df1861b4c46fb122ea2
SHA[loongarch64:libstdc++-14.2.0-r6.apk]=922ba4b57dddb9f5fe4b3df9d1bcedbbe28c12c4c7cf18f86a5e20e6d9cb566c
SHA[loongarch64:musl-1.2.5-r12.apk]=669c4e918eca5ea292499e03e0d226e62cae4da68f5f3ac0e359077d7e6de658
SHA[loongarch64:nghttp2-libs-1.69.0-r0.apk]=1c03b00e12a189aa5b23dbdb7662ddf50976daa2101edd2043f43ee607df9449
SHA[loongarch64:nodejs-22.22.2-r0.apk]=52a68535b57dda75b44b269270ea21c916fd422d00d55ac6df8f20ee20d24f9a
SHA[loongarch64:simdjson-3.12.0-r0.apk]=142b5dc38d49d1b56a162ba41051d7e9e51634c1275f62f98f0132eecabd2b30
SHA[loongarch64:simdutf-7.2.1-r0.apk]=914d5e77324d5da018455710322cf5ca272d63ed95f87180f9eff49e0771fd7c
SHA[loongarch64:sqlite-libs-3.49.2-r1.apk]=7d9ab44f4b798b1628b3126f9bc52b24f7803ed1e3130df0be996e8ddcee9ed5
SHA[loongarch64:yarn-1.22.22-r1.apk]=e4e575056d14d5fc5779b0a88c5a01d0bfb6f06b50853798cdcd9c3918130270
SHA[loongarch64:zlib-1.3.2-r0.apk]=d1857035bff84fb067e26f92e43275b17e2774a7250cab4fa2627a058421f061
SHA[loongarch64:zstd-libs-1.5.7-r0.apk]=65822d3aeccf357935ffea583719b9b7bc6d67727aefa8e381533b9ec82b13c6

# download <url> <dest>: curl (preferred) or wget; returns non-zero on HTTP/transfer error.
download() {
  local url="$1" dest="$2"
  if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 3 -o "$dest" "$url"
  else
    wget -O "$dest" "$url"
  fi
}

# fetch_apk <arch> <file> <sha256>: skip if already present + sha256 OK; else try the
# branch's main then community repo, downloading to the exact dest path/filename, and
# accept the first candidate whose bytes match the authoritative sha256.
fetch_apk() {
  local arch="$1" file="$2" sha="$3"
  local dest="$HERE/apks/$arch/$file"
  if [ -f "$dest" ] && echo "$sha  $dest" | sha256sum -c --status - 2>/dev/null; then
    echo "  [skip] $arch/$file already present (sha256 OK)"
    return 0
  fi
  [ -f "$dest" ] && { echo "  [warn] $arch/$file exists but sha256 mismatch -> re-downloading"; rm -f "$dest"; }
  mkdir -p "$(dirname "$dest")"
  local repo url
  for repo in "${REPOS[@]}"; do
    url="$APK_MIRROR/$APK_BRANCH/$repo/$arch/$file"
    echo "  [get] $url"
    if download "$url" "$dest" 2>/dev/null; then
      if echo "$sha  $dest" | sha256sum -c --status - 2>/dev/null; then
        echo "  [ok] $arch/$file sha256 verified ($repo)"
        return 0
      fi
      echo "  [warn] $arch/$file from $repo did not match shipped sha256 (wrong/rolled version?) -> trying next repo"
      rm -f "$dest"
    fi
  done
  echo "  [FAIL] could not obtain $arch/$file matching the shipped sha256 from $APK_BRANCH/{${REPOS[*]}}." >&2
  echo "         'edge' is rolling; recover the pinned version via 'apk fetch' or an edge archive mirror" >&2
  echo "         (see the CAVEAT at the top of this script), drop it into apks/$arch/$file, and re-run." >&2
  return 1
}

echo "=== fetch-resources: nodejs/lang (Node v22.22.2 musl runtime closure, ${#PKGS[@]} pkgs x ${#ARCHES[@]} arches) ==="
echo "    branch=$APK_BRANCH  mirror=$APK_MIRROR  repos=${REPOS[*]}"
rc=0
for arch in "${ARCHES[@]}"; do
  echo "--- [$arch] ---"
  for file in "${PKGS[@]}"; do
    sha="${SHA[$arch:$file]}"
    fetch_apk "$arch" "$file" "$sha" || rc=1
  done
done

[ "$rc" -eq 0 ] || { echo "fetch-resources: nodejs FAILED (some apks unresolved — see messages above)" >&2; exit 1; }
echo "fetch-resources: nodejs OK"
