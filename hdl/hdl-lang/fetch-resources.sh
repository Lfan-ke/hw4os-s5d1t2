#!/bin/bash
# fetch-resources.sh (hdl/hdl-lang)
# -----------------------------------------------------------------------------
# Purpose: re-acquire any re-downloadable resources removed during repo slimming
# so that prebuild.sh (the app runner's STARRY prebuild) can provision the carpet.
# Run BEFORE prebuild.sh / `cargo xtask starry app qemu -t hdl-lang`.
# Network is needed only for the OPTIONAL source rebuild (FETCH_SOURCES=1).
#
# STATUS for this app: NOTHING is required for prebuild from this repo. Everything
# prebuild.sh consumes is either retained in slim or a host-exclusive EDA tool:
#   RETAINED self-built artifacts (kept):
#     - vendor/vvp/vvp-<arch>    (STATIC Icarus runtime, VCD + system VPI embedded)
#     - vendor/make/make-<arch>  (GNU Make 4.4.1, musl-static)
#     - src/** , golden/** , host-carpets/**  (test sources + goldens, kept)
#   HOST-EXCLUSIVE tools on PATH (the maintainer installs these; not repo resources,
#   no fetchable artifact / no sha256 in SOURCES.md):
#     - verilator 5.008, iverilog/vvp 12, bsc 2026.01 (/usr/local/bsc), yosys 0.58
#     - /opt/<arch>-linux-musl-cross g++  (cross toolchain)
# The base alpine rootfs (if used downstream) comes from TGOSKITS_ROOT, NOT this repo.
#
# OPTIONAL (FETCH_SOURCES=1): rebuild the two retained static runtimes from source.
#   * vendor/vvp/vvp-<arch>  <- Icarus Verilog v12_0 source via ../iverilog/vvp-static-build
#   * vendor/make/make-<arch> <- GNU Make 4.4.1 source (ftp.gnu.org)
# SOURCES.md provides NO sha256 for either — downloads are left for manual verification.
# -----------------------------------------------------------------------------
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"

# fetch <url> <dest> <sha256|->   ('-' = no authoritative sha256 in SOURCES.md)
fetch() {
    local url="$1" dest="$2" want="$3"
    mkdir -p "$(dirname "$dest")"
    if [ -f "$dest" ]; then
        if [ "$want" != "-" ] && echo "$want  $dest" | sha256sum -c - >/dev/null 2>&1; then
            echo "  [skip] ${dest##*/} (sha256 ok)"; return 0
        elif [ "$want" = "-" ]; then
            echo "  [have] ${dest##*/} (no authoritative sha256 in SOURCES.md; verify manually)"; return 0
        fi
    fi
    echo "  [get ] $url"
    if command -v curl >/dev/null 2>&1; then curl -fL --retry 3 -o "$dest" "$url"
    else wget -O "$dest" "$url"; fi
    if [ "$want" != "-" ]; then
        echo "$want  $dest" | sha256sum -c - >/dev/null 2>&1 || { echo "  sha256 MISMATCH: $dest" >&2; rm -f "$dest"; exit 1; }
        echo "  [ok  ] ${dest##*/} (sha256 verified)"
    else
        echo "  [warn] ${dest##*/} fetched; SOURCES.md has no sha256 — verify manually" >&2
    fi
}

echo "fetch-resources: hdl-lang — verifying retained self-built artifacts"
miss=0
for a in x86_64 aarch64 riscv64 loongarch64; do
    [ -f "$HERE/vendor/vvp/vvp-$a" ]   && echo "  [have] vendor/vvp/vvp-$a"   || { echo "  [MISS] vendor/vvp/vvp-$a (rebuild via ../iverilog/vvp-static-build, FETCH_SOURCES=1)"; miss=1; }
    [ -f "$HERE/vendor/make/make-$a" ] && echo "  [have] vendor/make/make-$a" || { echo "  [MISS] vendor/make/make-$a (rebuild from GNU Make 4.4.1, FETCH_SOURCES=1)"; miss=1; }
done
[ -d "$HERE/src" ]    && echo "  [have] src/"    || { echo "  [MISS] src/"; miss=1; }
[ -d "$HERE/golden" ] && echo "  [have] golden/" || { echo "  [MISS] golden/"; miss=1; }

echo "fetch-resources: hdl-lang — host-exclusive tools required on PATH for prebuild.sh:"
echo "  verilator 5.008 / iverilog 12 / bsc 2026.01 (/usr/local/bsc) / yosys 0.58 / /opt/<arch>-linux-musl-cross"
echo "  (install from upstream per SOURCES.md; these are not downloadable repo resources)"

if [ "${FETCH_SOURCES:-0}" = "1" ]; then
    echo "fetch-resources: hdl-lang — OPTIONAL source-build: fetching upstream runtime sources"
    fetch "https://codeload.github.com/steveicarus/iverilog/tar.gz/refs/tags/v12_0" \
          "$HERE/.srcbuild/iverilog-v12_0.tar.gz" "-"
    fetch "https://ftp.gnu.org/gnu/make/make-4.4.1.tar.gz" \
          "$HERE/.srcbuild/make-4.4.1.tar.gz" "-"
    echo "  next: rebuild vvp via ../iverilog/vvp-static-build/BUILD.md; cross-build make per SOURCES.md"
fi

[ "$miss" -eq 0 ] || echo "fetch-resources: hdl-lang — some artifacts absent; rebuild per SOURCES.md" >&2
echo "fetch-resources: hdl-lang OK"
