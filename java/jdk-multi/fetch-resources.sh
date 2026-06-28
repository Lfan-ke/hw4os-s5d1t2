#!/bin/bash
# fetch-resources.sh -- java/jdk-multi
#
# Re-acquires the large, re-downloadable JDK packages that were stripped from
# this slimmed delivery repo. After it succeeds, run the rootfs builder:
#     bash case/prep-jdk-multi-rootfs.sh <x86_64|aarch64|riscv64|loongarch64>
#
# Requires network access (and curl or wget + sha256sum). All artifacts are
# placed back under packages/ at the EXACT path + filename prep expects, and
# every download is sha256-verified against the value recorded by the repo's
# git-LFS index / SOURCES.md (authoritative; nothing here is invented).
#
# Resource classes:
#   * direct download : BellSoft Liberica JDK/JRE tarballs (musl x64/aarch64,
#                       glibc riscv64) + Alpine edge/community loongarch64 musl
#                       OpenJDK25 apks.
#   * manual          : Loongson "FX" LoongArch JDK21/JDK23 glibc tarballs --
#                       no stable canonical download URL; obtain per SOURCES.md.
#   * source build    : the *-srcbuild.tar.gz musl-native ports (loong JDK23,
#                       rv JDK25), reproduced by the in-tree setup-*.sh + .patch.
#                       NOTE: these are NOT consumed by the current prep script
#                       (it uses the Loongson/BellSoft-glibc fallbacks above);
#                       they are the optional native-musl variants. Build only
#                       on demand (needs a cross toolchain).
#
# NOTE: JDK17 is shared with the openjdk17 app and is fetched by
#       ../openjdk17/fetch-resources.sh -- run that one too.
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

echo "=== jdk-multi: direct downloads (BellSoft tars + Alpine loongarch64 apks) ==="
run_direct <<'JM_DIRECT'
jdk21/bellsoft-jdk21.0.11+11-linux-aarch64-musl.tar.gz	https://download.bell-sw.com/java/21.0.11+11/bellsoft-jdk21.0.11+11-linux-aarch64-musl.tar.gz	6118fce93eb0f595b3ed48252a43e6610fd550b42b7740701212369cd934ce5a
jdk21/bellsoft-jdk21.0.11+11-linux-riscv64.tar.gz	https://download.bell-sw.com/java/21.0.11+11/bellsoft-jdk21.0.11+11-linux-riscv64.tar.gz	30bc4fefa5ab806dc10ba5dce6d87caac8de959ddf385802f8fdec4491ffd48d
jdk21/bellsoft-jdk21.0.11+11-linux-x64-musl.tar.gz	https://download.bell-sw.com/java/21.0.11+11/bellsoft-jdk21.0.11+11-linux-x64-musl.tar.gz	5326af096fb1b943b4819ae2a51cbe3bfd4de45e4d1803d3fe3db2a6c8c8b125
jdk21/bellsoft-jre21.0.11+11-linux-aarch64-musl.tar.gz	https://download.bell-sw.com/java/21.0.11+11/bellsoft-jre21.0.11+11-linux-aarch64-musl.tar.gz	524f037778aac5e746648f31cf235eb2cf17c48bb63f4275b39e8565cb21f5c5
jdk21/bellsoft-jre21.0.11+11-linux-riscv64.tar.gz	https://download.bell-sw.com/java/21.0.11+11/bellsoft-jre21.0.11+11-linux-riscv64.tar.gz	e390494c5685adbd80daf3a301a678b7a62ce80077ba8c9d1c46e4f64fa9970b
jdk21/bellsoft-jre21.0.11+11-linux-x64-musl.tar.gz	https://download.bell-sw.com/java/21.0.11+11/bellsoft-jre21.0.11+11-linux-x64-musl.tar.gz	96acec6919281e9fab92217f439ab3be812414424ffebf6f9edf7a7424afff06
jdk23/bellsoft-jdk23.0.2+9-linux-aarch64-musl.tar.gz	https://download.bell-sw.com/java/23.0.2+9/bellsoft-jdk23.0.2+9-linux-aarch64-musl.tar.gz	40b39d58eb66598f46245e85a34bd1271cd1ddf077fa2d4357a5377cda7c8b59
jdk23/bellsoft-jdk23.0.2+9-linux-riscv64.tar.gz	https://download.bell-sw.com/java/23.0.2+9/bellsoft-jdk23.0.2+9-linux-riscv64.tar.gz	912dbba0e3dca9b0981891dda8746d221bd78f0346a46cb5027c70503952add4
jdk23/bellsoft-jdk23.0.2+9-linux-x64-musl.tar.gz	https://download.bell-sw.com/java/23.0.2+9/bellsoft-jdk23.0.2+9-linux-x64-musl.tar.gz	16f67aed6d6564f3bec7b5904ccc35f48b1b9dddf32540b47975eb1c155603ce
jdk23/bellsoft-jre23.0.2+9-linux-aarch64-musl.tar.gz	https://download.bell-sw.com/java/23.0.2+9/bellsoft-jre23.0.2+9-linux-aarch64-musl.tar.gz	bd2af6be5a68aef52d5bb805f3c04d04fa9c4e623f738d05e79fdadc90e0c7af
jdk23/bellsoft-jre23.0.2+9-linux-riscv64.tar.gz	https://download.bell-sw.com/java/23.0.2+9/bellsoft-jre23.0.2+9-linux-riscv64.tar.gz	b07ccb5af9e5a238216c8a730f3a6afc0453b420b7d5a4b39c8994853ce5a671
jdk23/bellsoft-jre23.0.2+9-linux-x64-musl.tar.gz	https://download.bell-sw.com/java/23.0.2+9/bellsoft-jre23.0.2+9-linux-x64-musl.tar.gz	2d7404dccbaa0f1704c413b445aac9825880d457486ffe472fa7b5252440a20d
jdk25/bellsoft-jdk25+37-linux-aarch64-musl.tar.gz	https://download.bell-sw.com/java/25+37/bellsoft-jdk25+37-linux-aarch64-musl.tar.gz	0d0aae364e44768059358434fe8bfe2aaa209866586c9f38842fec6f03f363c3
jdk25/bellsoft-jdk25+37-linux-riscv64.tar.gz	https://download.bell-sw.com/java/25+37/bellsoft-jdk25+37-linux-riscv64.tar.gz	281b66572b2a9dc8a07cf0bbc3653a2fbef78de35a7cf78d9f48517da328d242
jdk25/bellsoft-jdk25+37-linux-x64-musl.tar.gz	https://download.bell-sw.com/java/25+37/bellsoft-jdk25+37-linux-x64-musl.tar.gz	c39d961788095b7facc97d88cbf5c26e2b88b2df30438c0b0e5e637a44e708ef
jdk25/bellsoft-jre25+37-linux-aarch64-musl.tar.gz	https://download.bell-sw.com/java/25+37/bellsoft-jre25+37-linux-aarch64-musl.tar.gz	32092d0209475f1269b62329edbb168325dbad9f09e069d9d89f403dc245ecd4
jdk25/bellsoft-jre25+37-linux-riscv64.tar.gz	https://download.bell-sw.com/java/25+37/bellsoft-jre25+37-linux-riscv64.tar.gz	2a36d4fa3da2b59c85722cd8288a937589ef10e3015692dc2d1a1b5b5fc63bb5
jdk25/bellsoft-jre25+37-linux-x64-musl.tar.gz	https://download.bell-sw.com/java/25+37/bellsoft-jre25+37-linux-x64-musl.tar.gz	49141bc5e8d675328f818e6765ba51a8f8b3430cb8f50741ccf6abdba9e7bb63
jdk25/loongarch64-alpine-musl/openjdk25-25.0.3_p9-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/openjdk25-25.0.3_p9-r1.apk	d80c211912e7319b51753e3abc4ed84995aaee5c74244b70affcb393a1edccbe
jdk25/loongarch64-alpine-musl/openjdk25-jdk-25.0.3_p9-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/openjdk25-jdk-25.0.3_p9-r1.apk	38bf9508ba53fa604f7169510fd9c8b72672871ff76415634c80eaee902f3ea3
jdk25/loongarch64-alpine-musl/openjdk25-jmods-25.0.3_p9-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/openjdk25-jmods-25.0.3_p9-r1.apk	81b2786e32e9ee889c9d28efecc522c330b97c9a8f6922fab077566d25c57b9c
jdk25/loongarch64-alpine-musl/openjdk25-jre-25.0.3_p9-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/openjdk25-jre-25.0.3_p9-r1.apk	1aba2d825748b5913d56d8856f31830481fc786107f04dd3a8c7f61ec8bd518f
jdk25/loongarch64-alpine-musl/openjdk25-jre-headless-25.0.3_p9-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/openjdk25-jre-headless-25.0.3_p9-r1.apk	41b736535311500e42a2c2f949c38e5f0bb65c20d02158dc4fa382690f9bb914
jdk25/loongarch64-alpine-musl/openjdk25-loongarch-25.0.1_p8-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/openjdk25-loongarch-25.0.1_p8-r1.apk	42a6d9a8dafa885c37a763a5b70814915bb73879bc8249222566f175d6cd2772
jdk25/loongarch64-alpine-musl/openjdk25-loongarch-jdk-25.0.1_p8-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/openjdk25-loongarch-jdk-25.0.1_p8-r1.apk	4e1c4d1aada0a4f524ec5200ec705cb0c7769fe77e0f5d7ccf998becafce61df
jdk25/loongarch64-alpine-musl/openjdk25-loongarch-jmods-25.0.1_p8-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/openjdk25-loongarch-jmods-25.0.1_p8-r1.apk	cb6886b8f3f5306f2f509c542d94a1adba523e08881a1fc73a762da7ce3b5fc0
jdk25/loongarch64-alpine-musl/openjdk25-loongarch-jre-25.0.1_p8-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/openjdk25-loongarch-jre-25.0.1_p8-r1.apk	a8ab4d738501c5ff3a8257d2c9e85684200dd5275b097efe0cadaa80693dddb6
jdk25/loongarch64-alpine-musl/openjdk25-loongarch-jre-headless-25.0.1_p8-r1.apk	https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/openjdk25-loongarch-jre-headless-25.0.1_p8-r1.apk	28e19f2c14d8137d9e767347ef61953af99fec263a1374e6221d4d22b8ef3796
JM_DIRECT

echo "=== jdk-multi: manual-retrieval items (Loongson FX glibc tars) ==="
run_manual <<'JM_MANUAL'
jdk21/loongson21.10.25-fx-jdk21.0.10_7-linux-loongarch64.tar.gz	ce90ceeba89ce365f1629ec1a1cefcd84826e89e9d45c1775c2d3c4745eaf3f2	Loongson FX JDK (loongnix.cn); no stable canonical URL in SOURCES.md
jdk23/loongson23.1.17-fx-jdk23_37-linux-loongarch64.tar.gz	55e4ab6a285962a24f2a916720899bcac0ce2838a63d44e75359aa49300fe8c9	Loongson FX JDK (loongnix.cn); no stable canonical URL in SOURCES.md
JM_MANUAL

# --- source-build variants (optional; not consumed by current prep) ----------
# Reproduce the native-musl ports from source via the kept setup scripts +
# .patch files. Disabled by default (need a cross GCC + a glibc x64 boot JDK +
# ~hours of build). Enable with BUILD_SRCBUILD=1 and the documented env:
#   export JAVA_DL_ROOT=<a scratch download/build dir>
#   export LOONG_MUSL_CROSS=<loongarch64-linux-musl cross GCC prefix>   # jdk23
#   export RV_MUSL_CROSS=<riscv64-linux-musl cross GCC prefix>          # jdk25
# Each setup script clones the upstream tag, applies its musl-port .patch and
# cross-compiles; this then copies + sha256-verifies the result into packages/.
build_srcbuild() {
  local setup="$1" dest_rel="$2" sha="$3" produced="$4"
  local dest="$HERE/packages/$dest_rel"
  if sha_ok "$dest" "$sha"; then echo "  ok (cached)  $dest_rel"; return 0; fi
  if [ "${BUILD_SRCBUILD:-0}" != "1" ]; then
    echo "  SKIP (set BUILD_SRCBUILD=1 to build): $dest_rel"
    echo "                 builder: $setup"
    echo "                 sha256 : $sha"
    MANUAL+=("$dest_rel (source build -- $setup)")
    return 0
  fi
  : "${JAVA_DL_ROOT:?set JAVA_DL_ROOT to a scratch download/build dir}"
  echo "  BUILD        $dest_rel  (via $setup)"
  bash "$HERE/packages/$setup"
  mkdir -p "$(dirname "$dest")"
  cp -f "$JAVA_DL_ROOT/jdk-multi/$produced" "$dest"
  if ! sha_ok "$dest" "$sha"; then
    echo "  !! built artifact sha256 mismatch: $dest_rel (expected $sha)" >&2
    FAILED+=("$dest_rel"); return 1
  fi
  echo "  done         $dest_rel"
}

echo "=== jdk-multi: source-build variants (optional) ==="
build_srcbuild "jdk23/setup-loong-jdk23.sh" "jdk23/openjdk23-loongarch64-musl-srcbuild.tar.gz" "74ef309f3a18a1954a4f0b36146b987e4924e1da5de0310fccad3fa38c0c5ef2" "jdk23/openjdk23-loongarch64-musl-srcbuild.tar.gz"
build_srcbuild "jdk25/setup-rv-jdk25.sh" "jdk25/openjdk25-riscv64-musl-srcbuild.tar.gz" "42bb25a018faf3bba253bffc9b0f18964bb231504e3fe61c77b32e4268b3e847" "jdk25/openjdk25-riscv64-musl-srcbuild.tar.gz"

echo
if [ "${#FAILED[@]}" -gt 0 ]; then
  echo "fetch-resources: jdk-multi -- the following download/verify failures:" >&2
  for x in "${FAILED[@]}"; do echo "   - $x" >&2; done
fi
if [ "${#MANUAL[@]}" -gt 0 ]; then
  echo "fetch-resources: jdk-multi -- items needing MANUAL retrieval (see notes above / SOURCES.md):" >&2
  for x in "${MANUAL[@]}"; do echo "   - $x" >&2; done
fi
if [ "${#FAILED[@]}" -gt 0 ]; then
  echo "fetch-resources: jdk-multi INCOMPLETE (see failures above)" >&2
  exit 1
fi
echo "fetch-resources: jdk-multi OK"
