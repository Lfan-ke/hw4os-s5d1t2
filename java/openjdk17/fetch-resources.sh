#!/bin/bash
# fetch-resources.sh -- java/openjdk17
#
# Re-acquires the OpenJDK 17 package store (Alpine musl apks + riscv64 glibc
# tarballs/.debs) that was stripped from this slimmed delivery repo. This
# directory is a SHARED package store: it is consumed by jdk-multi's
# prep (case/prep-jdk-multi-rootfs.sh, via ../openjdk17/packages) and by the
# other java apps' prebuild scripts. Run this before any of those preps.
#
# Requires network access (and curl or wget + sha256sum). Every file is placed
# back under packages/<arch>/ at the EXACT name the prep scripts expect and is
# sha256-verified against the value recorded by the repo's git-LFS index /
# SOURCES.md (authoritative; nothing here is invented).
#
# Resource classes:
#   * direct download : Alpine apks (edge/main, edge/community, v3.22/community)
#                       for x86_64/aarch64/loongarch64/riscv64 + the loongarch
#                       native variant; riscv64 glibc JDKs (Adoptium Temurin,
#                       BellSoft Liberica) + Debian .debs.
#   * manual          : openjdk17-riscv64-musl-NATIVE-cross.tar.gz -- a self
#                       built native musl JDK17 cross (no vendor ships musl+
#                       riscv64 JDK17); reproduce per SOURCES.md III riscv64.
#   * meta-apkindex   : the rolling Alpine APKINDEX files (reference only, not
#                       consumed by prep) -- optional, see bottom of script.
#
# NOTE on Alpine `edge` URLs: edge is a rolling repo, so some pinned versions
# recorded here are historical snapshots that edge may have superseded (a fetch
# would then 404 or sha-mismatch). If that happens use a mirror or the Alpine
# archive (https://archive.alpinelinux.org/alpine/). The sha256 check guarantees
# you never install a silently-different version.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
FAILED=()
MANUAL=()

have() { command -v "$1" >/dev/null 2>&1; }

# sha256 verify: returns 0 if file exists and matches
sha_ok() {
  local f="$1" sha="$2"
  [ -f "$f" ] || return 1
  printf '%s  %s\n' "$sha" "$f" | sha256sum -c --status 2>/dev/null
}

# fetch <url> <dest_abs> <sha256>
#   skip if dest already present and sha matches; else download + verify; on
#   mismatch delete the partial and report failure (never leaves a bad file).
fetch() {
  local url="$1" dest="$2" sha="$3" rel="${2#$HERE/}"
  if sha_ok "$dest" "$sha"; then echo "  ok (cached)  $rel"; return 0; fi
  mkdir -p "$(dirname "$dest")"
  echo "  GET          $rel"
  if have curl; then
    curl -fL --retry 3 --retry-delay 2 -o "$dest.part" "$url" || { echo "  !! download failed: $url" >&2; rm -f "$dest.part"; return 1; }
  elif have wget; then
    wget -O "$dest.part" "$url" || { echo "  !! download failed: $url" >&2; rm -f "$dest.part"; return 1; }
  else
    echo "  !! need curl or wget" >&2; return 1
  fi
  if ! sha_ok "$dest.part" "$sha"; then
    echo "  !! sha256 MISMATCH for $rel (expected $sha) -- removing" >&2
    rm -f "$dest.part"; return 1
  fi
  mv -f "$dest.part" "$dest"
  echo "  done         $rel"
}

# run the embedded TAB-separated direct table on stdin: <rel>\t<url>\t<sha>
run_direct() {
  local rel url sha
  while IFS=$'\t' read -r rel url sha; do
    [ -z "$rel" ] && continue
    case "$rel" in \#*) continue;; esac
    fetch "$url" "$HERE/packages/$rel" "$sha" || FAILED+=("$rel")
  done
}

# note a manual (no reliable URL) item: verify if present, else record for the
# human to obtain per SOURCES.md. <rel>\t<sha>\t<note>
run_manual() {
  local rel sha note
  while IFS=$'\t' read -r rel sha note; do
    [ -z "$rel" ] && continue
    case "$rel" in \#*) continue;; esac
    if sha_ok "$HERE/packages/$rel" "$sha"; then
      echo "  ok (present) $rel"
    else
      echo "  MANUAL       $rel"
      echo "                 sha256: $sha"
      echo "                 note  : $note"
      MANUAL+=("$rel")
    fi
  done
}

echo "=== openjdk17: direct downloads (Alpine apks + riscv64 glibc JDKs/debs) ==="
run_direct <<'OJ_DIRECT'
aarch64/alsa-lib-1.2.14-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/alsa-lib-1.2.14-r2.apk	ebb1d37703901e8f9f0f57e14dfff82d31b084f7f01e15a95cb9825320912e92
aarch64/brotli-libs-1.2.0-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/brotli-libs-1.2.0-r0.apk	c8a9113b0007d40291e55cfc196744b64004253707e37b3c07e188166fb2d0a0
aarch64/ca-certificates-20260413-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/ca-certificates-20260413-r0.apk	e43ec774b9e42de01164eb2ab7b5adcaee82dca2f29c1088678350a3666617ae
aarch64/ca-certificates-bundle-20260413-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/ca-certificates-bundle-20260413-r0.apk	f65ee4b7b7313908ef4d5c3011d182dae4ab524be12d4a877ba6d72cd1b7d7fb
aarch64/freetype-2.14.3-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/freetype-2.14.3-r0.apk	9231502c35ad90c3e03ae4a6e6e3351d91df65b89205c452a12c62b3a1b2039a
aarch64/gcompat-1.1.0-r4.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/gcompat-1.1.0-r4.apk	1c85db437485f2efbbee3009b159e0cded7ec7c2b6ffd57086e85848efb2fedf
aarch64/giflib-5.2.2-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/giflib-5.2.2-r1.apk	22b95fc2ff28fecb3b75264c79bb8a86c718fdcb30704225aa3137f117a75fa7
aarch64/java-cacerts-1.1-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/aarch64/java-cacerts-1.1-r0.apk	1fe96dc01b84e8cd0204b91bba80a0512e418d1dd109a5349f374404637d9dcf
aarch64/java-common-1.0-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/aarch64/java-common-1.0-r1.apk	7dec8cd5da0095380387ff8284a42fd429a73ee72f5dec50504d58aef8bb8f01
aarch64/lcms2-2.19-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/lcms2-2.19-r0.apk	e7767a97578a698399ee89ec4814ba37a802e99fa467c88a3ee295c1a1831084
aarch64/libbsd-0.12.2-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/libbsd-0.12.2-r0.apk	2cf4afa2b9da7d16227ec1d2d70d73a5ba45a67e8fb4f5c6f3f61210e491ddb8
aarch64/libbz2-1.0.8-r6.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/libbz2-1.0.8-r6.apk	387613963132acce64d6210bf5a88b0c1522740d13e80060404f091260088038
aarch64/libcrypto3-3.5.6-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/libcrypto3-3.5.6-r0.apk	36d12d483a52311074a4ede96e94a018670df019b7208b611820215f2fb04623
aarch64/libffi-3.5.2-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/libffi-3.5.2-r1.apk	bff86f6c3fc29e87fb8741b6a05602e6268a7f8cbefa48ec53d6f7fcfd00ff02
aarch64/libjpeg-turbo-3.1.3-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/libjpeg-turbo-3.1.3-r0.apk	64dd0ab69fd3ae2c71bfa029841d9ecbbc841d5670d16e17ca59effb9bf45382
aarch64/libmd-1.2.0-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/libmd-1.2.0-r0.apk	ff730a502b0f3b0efa7ce5eed231096b7c23ae1679ac5b05ee295235ff36cba2
aarch64/libpng-1.6.58-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/libpng-1.6.58-r1.apk	db74101b8d18465b35abdad7be99df34b019051d67bc2488cc83be8affde870c
aarch64/libtasn1-4.21.0-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/libtasn1-4.21.0-r0.apk	0d26ec98db730cebc1ba291aaf213bcaa63e0b9f31b561490777896220b80b07
aarch64/libucontext-1.5.1-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/libucontext-1.5.1-r0.apk	850c169e421365c051f6631e8480991db13301f37598c27f4f53bf356a66c1d7
aarch64/libx11-1.8.13-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/libx11-1.8.13-r0.apk	84ce28da42a922e3c19fe3159a5d87e722803537e119acdf7ae42f63d5b62b39
aarch64/libxau-1.0.12-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/libxau-1.0.12-r0.apk	46f4de5b7f47eb2271b4b1b3e2a514dee81255dae470061bb5f2a2449cd29f72
aarch64/libxcb-1.17.0-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/libxcb-1.17.0-r2.apk	a7c15a9808e6158505ef3484b7371854b49ef9da6cb18d208e70f28d1261422b
aarch64/libxdmcp-1.1.5-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/libxdmcp-1.1.5-r1.apk	f9693ef39d6cb985cfb6bc1135173c61acc2bbc7cc444d24bcdaa5ddd9b8cf51
aarch64/libxext-1.3.6-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/libxext-1.3.6-r2.apk	bb7e84c1d38e5dcdfdb56e543394469f9c884cb13e09f5e07986863a9bea62e7
aarch64/libxi-1.8.2-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/libxi-1.8.2-r0.apk	532f9994ad778886bffa866071ca5a37b179285aef57519ec3da9cc9661b0611
aarch64/libxrender-0.9.12-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/libxrender-0.9.12-r0.apk	5b80c5722971072f51bc360cf70f11a07e206925f797957a009e0738662f87b3
aarch64/libxtst-1.2.5-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/libxtst-1.2.5-r0.apk	8807c47cd9f977c785b7fb80783738f0eae202207e3acf5f96cadb6624669349
aarch64/musl-1.2.6-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/musl-1.2.6-r2.apk	c6e74d765f7029fdc4389340181616eb834a6e692b16144c0ca8f8d0662578b3
aarch64/musl-obstack-1.2.3-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/musl-obstack-1.2.3-r2.apk	b7f8b8ec3ca26745aeb2894481a05b7621c49b3d7026f9c8368f1908c579e3bf
aarch64/openjdk17-jdk-17.0.18_p8-r0.apk	https://dl-cdn.alpinelinux.org/alpine/v3.22/community/aarch64/openjdk17-jdk-17.0.18_p8-r0.apk	65e138145b5e6f176302555af402c4b0e21fb9e07ffbddacd02c421687e48ed8
aarch64/openjdk17-jmods-17.0.18_p8-r0.apk	https://dl-cdn.alpinelinux.org/alpine/v3.22/community/aarch64/openjdk17-jmods-17.0.18_p8-r0.apk	ec3c4d179b34dd043b16c51ae08f797af10c6cc8a716a17480de69138a743f3e
aarch64/openjdk17-jre-17.0.18_p8-r0.apk	https://dl-cdn.alpinelinux.org/alpine/v3.22/community/aarch64/openjdk17-jre-17.0.18_p8-r0.apk	fa031809ee8e6c2a23dd923635c97168d78b17d74c6866d4155e290619c23627
aarch64/openjdk17-jre-headless-17.0.18_p8-r0.apk	https://dl-cdn.alpinelinux.org/alpine/v3.22/community/aarch64/openjdk17-jre-headless-17.0.18_p8-r0.apk	4fe83995a0e7b2120df4e81fee07c6846ac28c19f98ff749b6d70b403f2371a9
aarch64/p11-kit-0.25.5-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/p11-kit-0.25.5-r2.apk	0f6a19167431f350f9b8eb9df93e901af2838b48f226ad5388e731814bb9ac81
aarch64/p11-kit-trust-0.25.5-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/p11-kit-trust-0.25.5-r2.apk	d5e9537e71d932dc5d2099d2fa90230ce6d5a7b15f19b92da5e4bdb325dd7c42
aarch64/zlib-1.3.2-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/zlib-1.3.2-r0.apk	1d354ed1ef4e7bd9f6459b56a5e0d5c81ec78788d165b6a459f0b41ff3f4c037
loongarch64/alsa-lib-1.2.14-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/alsa-lib-1.2.14-r2.apk	d036a9dbd84a0831854f73f2158f4bfe606427361583417befd381b03aa39d39
loongarch64/brotli-libs-1.2.0-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/brotli-libs-1.2.0-r0.apk	b6ac6f2bbdc3fe35c2f997b4766132d7febf7df893ed0f8bb29bd6f9254024e3
loongarch64/ca-certificates-20260413-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/ca-certificates-20260413-r0.apk	abf23c3a5bf7d5cb43d83272c7c4543419e6c41b1f1121b83ea4e22e0366ec05
loongarch64/ca-certificates-bundle-20260413-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/ca-certificates-bundle-20260413-r0.apk	39c0524cb2e158124c484fca5f2bf9f71a8999dbf983dd2bcfb3fe18dc4454bb
loongarch64/freetype-2.14.3-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/freetype-2.14.3-r0.apk	09f56f11f5b6640e1df10a27a61c27b0de2d24fcffe3cd7e5755b0aa4cdb45e5
loongarch64/gcompat-1.1.0-r4.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/gcompat-1.1.0-r4.apk	76719a89dddba800681cd8f3192d3298f5af6873cd72506f14d22bcc34cc9d8c
loongarch64/giflib-5.2.2-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/giflib-5.2.2-r1.apk	88e38d6fc6e5c1511a9834c3f51d0d750d186a470058a08fd15d7e1b447bfd38
loongarch64/java-cacerts-1.1-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/java-cacerts-1.1-r0.apk	eb1ceb3569b12699f6e656e982ab2f1187c8bb2730d09bc3613a566d87147c1a
loongarch64/java-common-1.0-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/java-common-1.0-r1.apk	578534373ba778e7b65ed24787cf876e3bb44db648790a95a2cafcb5d9d175e2
loongarch64/lcms2-2.19-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/lcms2-2.19-r0.apk	3cdce282a148bdef6d66a7a4b8a651f9581355362df43d7baea662c7e4da1315
loongarch64/libbsd-0.12.2-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/libbsd-0.12.2-r0.apk	5154288d44378e6c6b670109fd79632e4fa3cf72e47b17f92dfd42837a7b7580
loongarch64/libbz2-1.0.8-r6.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/libbz2-1.0.8-r6.apk	5261aff752abd06f45dbb0108bd0bbd540ccad37f391bbf7cfdd0c6b9a1ade44
loongarch64/libcrypto3-3.5.6-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/libcrypto3-3.5.6-r0.apk	587376f363bb7a9612616315130bff98484d0f069c45e1d4af3d79c2fe3f3cc4
loongarch64/libffi-3.5.2-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/libffi-3.5.2-r1.apk	f81152fb8a31cc7e3a3e32602b4278d1c1d2dcfeecf4a58ef47afa5237db924f
loongarch64/libjpeg-turbo-3.1.3-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/libjpeg-turbo-3.1.3-r0.apk	f1f9b0f77194ce704e96bffc9e3b960b23a4bda0ef451c675c8d0751a27624e7
loongarch64/libmd-1.2.0-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/libmd-1.2.0-r0.apk	8d14ac4ed4a59017f1026263a69be48b64510ed1e75e5afe9cc754e61cf5f6cd
loongarch64/libpng-1.6.58-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/libpng-1.6.58-r1.apk	05bafe38567127513d0a4b0544d551a53d63787d7fac7d1f56fc53307193aa35
loongarch64/libtasn1-4.21.0-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/libtasn1-4.21.0-r0.apk	a56315d4be73dcdcdfbb4b9a81fd6dcc91251dcd46f156062d06ffc7623a67d3
loongarch64/libucontext-1.5.1-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/libucontext-1.5.1-r0.apk	593431039c826f706359071f44362dc8ca50fcbddcaab0171ade1aaacd90491e
loongarch64/libx11-1.8.13-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/libx11-1.8.13-r0.apk	6bb1dfb5bd53852adcb8b9097a27a99a9707a08510e705816a0805089887df1d
loongarch64/libxau-1.0.12-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/libxau-1.0.12-r0.apk	3709946915bb6864e49cc638789f7c42bd49cf972e7c1690632e4f0cd7a8ce21
loongarch64/libxcb-1.17.0-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/libxcb-1.17.0-r2.apk	dc5df38de1d3faf323483a05e4f8818ffcbee8ca9b6571443727d6649cbd9b57
loongarch64/libxdmcp-1.1.5-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/libxdmcp-1.1.5-r1.apk	e4d120b12f4ebb8ac604a8713d64d9002e7032e8efaac698595b5e54cc311357
loongarch64/libxext-1.3.6-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/libxext-1.3.6-r2.apk	116ef278d4c8813cb2b5f9d94237fe20bba2bc7ec19786b23218320306b16aa5
loongarch64/libxi-1.8.2-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/libxi-1.8.2-r0.apk	91ecf137ebcac5ae3d1eb6d54cf7d278c887f6a36403008adea6716c95dc5936
loongarch64/libxrender-0.9.12-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/libxrender-0.9.12-r0.apk	5c83360c15a27c30d729aa6aaf8dd0127ec59f2f16d9de132a857ac176a136c1
loongarch64/libxtst-1.2.5-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/libxtst-1.2.5-r0.apk	0aaf0c75d6ca8885cad93315d4ab823011c531200c788b90075cb4c2b6048d4a
loongarch64/musl-1.2.6-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/musl-1.2.6-r2.apk	9699d656699f1f60517cdf2190147ca22672c85f950d21a7fbf1fd4aa3cb019e
loongarch64/musl-obstack-1.2.3-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/musl-obstack-1.2.3-r2.apk	1db9aa304eebdce0d7d18c9e850e2dc4ecfb927c436ad6ea881ae5f4a35112ff
loongarch64/openjdk17-jdk-17.0.18_p8-r0.apk	https://dl-cdn.alpinelinux.org/alpine/v3.22/community/loongarch64/openjdk17-jdk-17.0.18_p8-r0.apk	dfe178b50090b16cd4ebb5e176b7c76de59ca8f1b18c864af77fa251d0532691
loongarch64/openjdk17-jmods-17.0.18_p8-r0.apk	https://dl-cdn.alpinelinux.org/alpine/v3.22/community/loongarch64/openjdk17-jmods-17.0.18_p8-r0.apk	d2577b843abd647e3694d23b8edd5d9745a94d74c85fda45171d4f6254f84081
loongarch64/openjdk17-jre-17.0.18_p8-r0.apk	https://dl-cdn.alpinelinux.org/alpine/v3.22/community/loongarch64/openjdk17-jre-17.0.18_p8-r0.apk	9a539f77ccffbe48453373693601a9b7c13862ff0dcd41bce62bf34bb52f9759
loongarch64/openjdk17-jre-headless-17.0.18_p8-r0.apk	https://dl-cdn.alpinelinux.org/alpine/v3.22/community/loongarch64/openjdk17-jre-headless-17.0.18_p8-r0.apk	d74fc8e9cd40835105c7bd53de8be7fd5175b2df7cdf6b07180b068c961a9c59
loongarch64/openjdk17-loongarch-17.0.17_p10-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/openjdk17-loongarch-17.0.17_p10-r0.apk	1f478b34badd9009786396bfcbd7e93f3019f0ef3b1d46b684f30cbf2910381e
loongarch64/openjdk17-loongarch-jdk-17.0.17_p10-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/openjdk17-loongarch-jdk-17.0.17_p10-r0.apk	e55611f2280854e9bc4e76785b51decf840015d26888f3c4eb15df9d603cc49c
loongarch64/openjdk17-loongarch-jmods-17.0.17_p10-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/openjdk17-loongarch-jmods-17.0.17_p10-r0.apk	d9ad8763f8d7a13b5ce2618444bc5fcc43081b9c20fed50ee50cedb9f1eedbc1
loongarch64/openjdk17-loongarch-jre-17.0.17_p10-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/openjdk17-loongarch-jre-17.0.17_p10-r0.apk	9f867f80ce79cbffe51623e38b3085cb62e6d0d98e459425d8452a24e275f26f
loongarch64/openjdk17-loongarch-jre-headless-17.0.17_p10-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/openjdk17-loongarch-jre-headless-17.0.17_p10-r0.apk	42ae887f2099d44bbaa7531dad11d29da47796ba06637e1259427d5e2a55d80d
loongarch64/p11-kit-0.25.5-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/p11-kit-0.25.5-r2.apk	67389c45c330e0ed3a1764f30ccec6d11b85cbdf19444c61a386439e28ba852a
loongarch64/p11-kit-trust-0.25.5-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/p11-kit-trust-0.25.5-r2.apk	ce45f3a9027e485239bbc6075616e29b498c90c74c9c1fb450c05a5d93285931
loongarch64/zlib-1.3.2-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/loongarch64/zlib-1.3.2-r0.apk	b5930064885bbc54edd9e99d923d783f0f18945e9c010d4aa87a27b808c75adb
riscv64/OpenJDK17U-jdk_riscv64_linux_hotspot_17.0.19_10.tar.gz	https://github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.19%2B10/OpenJDK17U-jdk_riscv64_linux_hotspot_17.0.19_10.tar.gz	191cdd904aef8b8a7a91c98d649c7e3dc75b7341f112061231c2094c418fd630
riscv64/alsa-lib-1.2.14-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/alsa-lib-1.2.14-r2.apk	8d7c7dae7928e73362d7c24cd9c55447bf272bd1733d5ae0135886c998f5f5ed
riscv64/bellsoft-jdk17.0.19+11-linux-riscv64.tar.gz	https://github.com/bell-sw/Liberica/releases/download/17.0.19+11/bellsoft-jdk17.0.19+11-linux-riscv64.tar.gz	b4c2af4b913ac8a9d6fbe99544e74ef42849fd3440b5d155ce214d0de78dfabd
riscv64/bellsoft-jre17.0.19+11-linux-riscv64.tar.gz	https://github.com/bell-sw/Liberica/releases/download/17.0.19+11/bellsoft-jre17.0.19+11-linux-riscv64.tar.gz	b016d27bce585d07d1e6c38a2f22e63e7ee69533872acc527c8b279d35f54b74
riscv64/brotli-libs-1.2.0-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/brotli-libs-1.2.0-r0.apk	f4f5e305b3912640274a1655427124ec5151a08bbf0cd60941b82b0c034a6d46
riscv64/ca-certificates-20260413-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/ca-certificates-20260413-r0.apk	f8937b699e44bfab6772c09d2741803c25d09a2321c4acfee82b19f550d85380
riscv64/ca-certificates-bundle-20260413-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/ca-certificates-bundle-20260413-r0.apk	5a6e8fe00d520eb55ea9063607385886862ad818864fb0a8637d35d79b28bb52
riscv64/debian-glibc/openjdk-17-jdk-headless_17.0.19~9ea-1_riscv64.deb	http://deb.debian.org/debian/pool/main/o/openjdk-17/openjdk-17-jdk-headless_17.0.19~9ea-1_riscv64.deb	830b94dc4a0874c9a557c983777a26ca1fccf5cece30138e05a5f7eadf8666fd
riscv64/debian-glibc/openjdk-17-jdk_17.0.19~9ea-1_riscv64.deb	http://deb.debian.org/debian/pool/main/o/openjdk-17/openjdk-17-jdk_17.0.19~9ea-1_riscv64.deb	03b4bef1cb89e731b73a23762f8a5636eb0a86378058ade95180f589a3b816f8
riscv64/debian-glibc/openjdk-17-jre-headless_17.0.19~9ea-1_riscv64.deb	http://deb.debian.org/debian/pool/main/o/openjdk-17/openjdk-17-jre-headless_17.0.19~9ea-1_riscv64.deb	a4718d36114dc52b08ee0d20bc71c3d268c83e43573793b7c1c068b1efc84fb0
riscv64/debian-glibc/openjdk-17-jre_17.0.19~9ea-1_riscv64.deb	http://deb.debian.org/debian/pool/main/o/openjdk-17/openjdk-17-jre_17.0.19~9ea-1_riscv64.deb	1efceb02a6763ca8c298c8961052d635b623738a0a60cb63b46e19aa0c54f2e0
riscv64/freetype-2.14.3-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/freetype-2.14.3-r0.apk	965312477890bff1d8b32ee4047627f27036d517016eac2cb4cb641505408dd4
riscv64/gcompat-1.1.0-r4.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/gcompat-1.1.0-r4.apk	caab16a14d67186db08a40970e3ff00925cc0267ea5546591f9298126b3637eb
riscv64/giflib-5.2.2-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/giflib-5.2.2-r1.apk	0ee4fb1be5d9f8037227974c5aec0d6de8e8a7d4e9f29c2ea2530e71f7a7fa96
riscv64/java-cacerts-1.1-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/riscv64/java-cacerts-1.1-r0.apk	bc0595b6c9e50a464184d97a2d0edac948a947de1f3c91e394e7270f478f6448
riscv64/java-common-1.0-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/riscv64/java-common-1.0-r1.apk	992976e79a5d64e4e09e29230359621a723382c32a96ff964b8d02d7ca14e280
riscv64/lcms2-2.19-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/lcms2-2.19-r0.apk	e84fb640e7a1b7450ac41569d1e1ab98284239d9d58beaf2a9643fddcfbc21ed
riscv64/libbsd-0.12.2-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/libbsd-0.12.2-r0.apk	515c8d9cb1952685269be3749c1fac709ad352010b23930738d99cbd6dfb11ee
riscv64/libbz2-1.0.8-r6.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/libbz2-1.0.8-r6.apk	2dc9fc0385ad0d592155b88b019418d5b03d651607ff3ea6115c8ae779dfc5ea
riscv64/libcrypto3-3.5.6-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/libcrypto3-3.5.6-r0.apk	0f7a307ed39dbfa1c34c96df8612dffa18d08e3b79622a794a5b14d0f9f37d16
riscv64/libffi-3.5.2-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/libffi-3.5.2-r1.apk	e072700a7bff84a148d8e217e904b398dd7bb6a6ca88821b39f24804dea8cbd7
riscv64/libjpeg-turbo-3.1.3-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/libjpeg-turbo-3.1.3-r0.apk	fbeabcd588795530e81b9ff5c780bfbda5b1c675fe58fbc54a101e92404ab8fc
riscv64/libmd-1.2.0-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/libmd-1.2.0-r0.apk	caa5b75656048113e3273505ecdd3d799d0f6dc95a677a124327055ea7316676
riscv64/libpng-1.6.58-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/libpng-1.6.58-r1.apk	0a8a455324e11e7e599ca8eabf7842e176d2a090e2bc574106f2ca390a15ce38
riscv64/libtasn1-4.21.0-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/libtasn1-4.21.0-r0.apk	2c3d405fc46884682d9f91a02e1652a0a09cc21b77a21d699c43410d396801fa
riscv64/libucontext-1.5.1-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/libucontext-1.5.1-r0.apk	5cea6f66a7e23ebdd0d2d31f7958fca4cd75812dd4f4c92e2c94bedd95e6aadc
riscv64/libx11-1.8.13-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/libx11-1.8.13-r0.apk	4d4c7a0a7e54d7390a8a8a820cf325b0a0b45b08ded270114ac7048fd6a67c8f
riscv64/libxau-1.0.12-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/libxau-1.0.12-r0.apk	6198639f546632d4cab4cbe808e7155b6f2995f01e8f5e8dbc278f2ac33e134a
riscv64/libxcb-1.17.0-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/libxcb-1.17.0-r2.apk	2db06994c13435082f0cf0bfb6fa69ab5109b0566f50419c4caf457ec7fb90bd
riscv64/libxdmcp-1.1.5-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/libxdmcp-1.1.5-r1.apk	872e16c4e21ae2f96bf7afe6db70cf41530e049f4c7fe988769d52bbafa2ef82
riscv64/libxext-1.3.7-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/libxext-1.3.7-r0.apk	49adc9b4a1b3b367664d092825f841ea1e499c4e174b61e5fb5a2d55036013ab
riscv64/libxi-1.8.2-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/libxi-1.8.2-r0.apk	db4956527e39929076a39efcc04c639c97d4a21c6fc3319f89f79dff4998a19f
riscv64/libxrender-0.9.12-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/libxrender-0.9.12-r0.apk	a4f50bf61b44b10a120bb4a8ec0705dfab7bb14c5af48b542896fc66793535e3
riscv64/libxtst-1.2.5-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/libxtst-1.2.5-r0.apk	b218478b370b67870e9df50a1d638b7e82838ceb8cce708c12f1e0b4bc214138
riscv64/musl-1.2.6-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/musl-1.2.6-r2.apk	fc046652abd316d87131486bd915e08380694af6a20dd0a7aa6a289316b7b7dc
riscv64/musl-obstack-1.2.3-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/musl-obstack-1.2.3-r2.apk	98081e0015a726db622fbc9da8bc8af744ab75eb31b6ba1590d4cdf36bdf5cff
riscv64/p11-kit-0.25.5-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/p11-kit-0.25.5-r2.apk	30f9fcc03c7bcc8d2ff9eeeebfd1db57a616793e71d65903659f0fb76636def3
riscv64/p11-kit-trust-0.25.5-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/p11-kit-trust-0.25.5-r2.apk	4af9da74a8fd27f5b8517d74c6e1025fc3ee0a567d5f85016157221548f7810a
riscv64/zlib-1.3.2-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/riscv64/zlib-1.3.2-r0.apk	9d2e3b8f1a33a60f843960074fba747df5a795943eb084c22b15fcf6d8500ab4
x86_64/alsa-lib-1.2.14-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/alsa-lib-1.2.14-r2.apk	f27ba453e88d62cb40240216490403ce7f6c00b705f006583d94fe691606ce21
x86_64/brotli-libs-1.2.0-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/brotli-libs-1.2.0-r0.apk	09fbf1ddf0c71c6c4ab7bc12aef3452d8ace5adeab10c4c30214fc70f5e0e087
x86_64/ca-certificates-20260413-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/ca-certificates-20260413-r0.apk	81feb605f4ce9549bc5de3ee646da08a7917e898c1b993b11cb91fc66ee44007
x86_64/ca-certificates-bundle-20260413-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/ca-certificates-bundle-20260413-r0.apk	75ba02b05c9487306a5c02735437382d82547aa949389e354323211c56c4d240
x86_64/freetype-2.14.3-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/freetype-2.14.3-r0.apk	09d76fde675a9ca3cfeb83d72fb09cb2e9ccf79b2937daa05a1af9a37a6b7815
x86_64/gcompat-1.1.0-r4.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/gcompat-1.1.0-r4.apk	186271e5f87274f5a3b145da0d4339d9f7921bb8ca2b26451e07095704bb680f
x86_64/giflib-5.2.2-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/giflib-5.2.2-r1.apk	3680599db9a9e7dd544d1099c57aaf1dc8f578dea4b595cee00cd6593b8223ed
x86_64/java-cacerts-1.1-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/x86_64/java-cacerts-1.1-r0.apk	0696270656159dc964ef022c1649bb87b23dd183d9a9d4c1619379b3545f9d79
x86_64/java-common-1.0-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/x86_64/java-common-1.0-r1.apk	99633901179b9e2e18abec99fecb8e76a371053baf42780cde700fae4a3e50c4
x86_64/lcms2-2.19-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/lcms2-2.19-r0.apk	b4cb038af3c2f02bedd8456ae876c97ac0a9f29789add77871bfd6ce084ff136
x86_64/libbsd-0.12.2-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/libbsd-0.12.2-r0.apk	1a5f292088dc2430ae12362333b1e4e1c87143897ba895638469ab67eee3e72a
x86_64/libbz2-1.0.8-r6.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/libbz2-1.0.8-r6.apk	77bf2e1327c0582fb59225c1e0c1bc38aac384473210550ce0f4e5b6d60364bd
x86_64/libcrypto3-3.5.6-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/libcrypto3-3.5.6-r0.apk	058b0fa34ad26be58b1b2a5b23a49ce006bc699bf7c3657fc632b8f643bb28cc
x86_64/libffi-3.5.2-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/libffi-3.5.2-r1.apk	0ab19290ba2a4aea64613c16b9744853363cbd7a61159860eaf4bd255d470f56
x86_64/libjpeg-turbo-3.1.2-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/libjpeg-turbo-3.1.2-r0.apk	7cd59be23cfb2400fd4d666cb4d3f70e3f7845caee3200565a403f9375ba6e88
x86_64/libmd-1.2.0-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/libmd-1.2.0-r0.apk	f6d68d072fcefc8edd4ffe3afc341ca391f8a9a209130628fb065d98b18a2792
x86_64/libpng-1.6.58-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/libpng-1.6.58-r1.apk	7c1988460b88cbf22f0b7a25eaa1c456b3c9def3523580779d53dd0dd72357d4
x86_64/libtasn1-4.21.0-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/libtasn1-4.21.0-r0.apk	4f7772e8eba07a621d59f63a8f4493b17d86e811bbc97be48f7d0a98c88ccbab
x86_64/libucontext-1.5.1-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/libucontext-1.5.1-r0.apk	fda972c0297b95efa2801de28d0a2995457cab85401c6d885a9c66641d79500c
x86_64/libx11-1.8.12-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/libx11-1.8.12-r1.apk	cbc478a642226812ab13688bfe9ec69c86cf0e8e037dba7cfd055e8d7741f30c
x86_64/libxau-1.0.12-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/libxau-1.0.12-r0.apk	535d2b50eac7ab5db5f6168382cdade054507feccc77088cc6747aabaa4a22b4
x86_64/libxcb-1.17.0-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/libxcb-1.17.0-r2.apk	7935d5ad9c7137b7b3f1292d62f55f7e0cc5c919155218c3060ea46503b3f211
x86_64/libxdmcp-1.1.5-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/libxdmcp-1.1.5-r1.apk	b0bf553ea30ed729e333edb655b4eb6260f11a0a228cb790074359c045bd6805
x86_64/libxext-1.3.6-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/libxext-1.3.6-r2.apk	404beb855daf09fdb8d15f177a3608da62825a44497a805675a186171c04eb6c
x86_64/libxi-1.8.2-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/libxi-1.8.2-r0.apk	9e9f1c6418837ba6aa137c3374a8d64eb9c2654cf6f6cf415afdd629cf3f78bb
x86_64/libxrender-0.9.12-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/libxrender-0.9.12-r0.apk	1db190111ef622222deec4d184a2607cefdd08f277960b6ff9f639534e671860
x86_64/libxtst-1.2.5-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/libxtst-1.2.5-r0.apk	aefecc111d66f1eeac7b5685260b22b339ff42b61965d0f9b98f1d23fd7ae491
x86_64/musl-1.2.6-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/musl-1.2.6-r2.apk	2aed6644a1332a63ee8873cc5b83e8c358bc6be45a7e16de9c9042e86cf30157
x86_64/musl-obstack-1.2.3-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/musl-obstack-1.2.3-r2.apk	c4b250d2a6088d7ebcf57b00b085b9c3ca29d8b24216b04a3ac224ae8d943014
x86_64/openjdk17-jdk-17.0.18_p8-r0.apk	https://dl-cdn.alpinelinux.org/alpine/v3.22/community/x86_64/openjdk17-jdk-17.0.18_p8-r0.apk	cc00d50df80c21f7e8891819b4adb5817dd46aeb3e381f7b6489de3feabaa37d
x86_64/openjdk17-jmods-17.0.18_p8-r0.apk	https://dl-cdn.alpinelinux.org/alpine/v3.22/community/x86_64/openjdk17-jmods-17.0.18_p8-r0.apk	788a66ec50d74784fa3ea545f7cd52fca51a855f99c930f8c508d683521e8b84
x86_64/openjdk17-jre-17.0.18_p8-r0.apk	https://dl-cdn.alpinelinux.org/alpine/v3.22/community/x86_64/openjdk17-jre-17.0.18_p8-r0.apk	9572f965396c63c24bd894c219ee78d70b182b043923b79190b0aff9b0a96547
x86_64/openjdk17-jre-headless-17.0.18_p8-r0.apk	https://dl-cdn.alpinelinux.org/alpine/v3.22/community/x86_64/openjdk17-jre-headless-17.0.18_p8-r0.apk	a188c449b4a92c6fb3d8bf404e8ca50a697560b7a1d9b67c8c34486f1e3c36bf
x86_64/p11-kit-0.25.5-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/p11-kit-0.25.5-r2.apk	d63c1264e58286dc8752860e9a2093b779639bc4276438fac3bdcf92ebf3d866
x86_64/p11-kit-trust-0.25.5-r2.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/p11-kit-trust-0.25.5-r2.apk	8e3a693c18313b23e24b040f433e878b545bf91df433793f2e872fbe4d3446e1
x86_64/zlib-1.3.2-r0.apk	https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/zlib-1.3.2-r0.apk	6e5dd88eb04341f673a40977a2d36bbe825ac772edddd2951192dcff76b9c49e
OJ_DIRECT

echo "=== openjdk17: manual-retrieval items ==="
run_manual <<'OJ_MANUAL'
riscv64/openjdk17-riscv64-musl-NATIVE-cross.tar.gz	e321cfef413a133e4b11680f9166565f57a576e613803b23a79159b22703336b	self-built musl JDK17 cross (no upstream prebuilt); see SOURCES.md III riscv64 path 3
OJ_MANUAL

# --- meta-apkindex (optional, reference only) --------------------------------
# packages/meta-apkindex/<arch>/{APKINDEX-main,APKINDEX-community} are extracted
# from the rolling Alpine APKINDEX.tar.gz and are NOT consumed by any prep
# script. They cannot be pinned by sha256 (the live index changes on every
# package push). To refresh them on demand set FETCH_APKINDEX=1:
if [ "${FETCH_APKINDEX:-0}" = "1" ]; then
  echo "=== openjdk17: refreshing meta-apkindex (rolling; sha not pinned) ==="
  for arch in x86_64 aarch64 riscv64 loongarch64; do
    for repo in main community; do
      d="$HERE/packages/meta-apkindex/$arch"; mkdir -p "$d"
      url="https://dl-cdn.alpinelinux.org/alpine/edge/$repo/$arch/APKINDEX.tar.gz"
      if have curl; then curl -fL --retry 3 -o "$d/APKINDEX.tar.gz" "$url" || true
      else wget -O "$d/APKINDEX.tar.gz" "$url" || true; fi
      if [ -f "$d/APKINDEX.tar.gz" ]; then
        tar xzf "$d/APKINDEX.tar.gz" -C "$d" APKINDEX 2>/dev/null && mv -f "$d/APKINDEX" "$d/APKINDEX-$repo" || true
        rm -f "$d/APKINDEX.tar.gz"
        echo "  refreshed   meta-apkindex/$arch/APKINDEX-$repo"
      fi
    done
  done
fi

echo
if [ "${#FAILED[@]}" -gt 0 ]; then
  echo "fetch-resources: openjdk17 -- the following download/verify failures:" >&2
  for x in "${FAILED[@]}"; do echo "   - $x" >&2; done
fi
if [ "${#MANUAL[@]}" -gt 0 ]; then
  echo "fetch-resources: openjdk17 -- items needing MANUAL retrieval (see notes above / SOURCES.md):" >&2
  for x in "${MANUAL[@]}"; do echo "   - $x" >&2; done
fi
if [ "${#FAILED[@]}" -gt 0 ]; then
  echo "fetch-resources: openjdk17 INCOMPLETE (see failures above)" >&2
  exit 1
fi
echo "fetch-resources: openjdk17 OK"
