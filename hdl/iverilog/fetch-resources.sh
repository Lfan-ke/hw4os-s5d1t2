#!/bin/bash
# fetch-resources.sh (hdl/iverilog)
# -----------------------------------------------------------------------------
# Purpose: re-acquire any re-downloadable resources removed during repo slimming
# so that prep-iverilog-rootfs.sh can build the rootfs. Run BEFORE prep-*.sh.
# Network is needed only for the OPTIONAL source rebuild (FETCH_SOURCES=1).
#
# STATUS for this app: NOTHING is required for prep. Every resource consumed by
# prep-iverilog-rootfs.sh is a self-built, hard-to-reproduce artifact kept in slim:
#   - testbin/vvp-<arch>  (STATIC Icarus vvp v12_0, system VPI embedded; the
#                          source-build lives in vvp-static-build/, kept)
#   - dut.vvp             (host iverilog -g2012 bytecode, :vpi_module rewritten; kept)
#   - golden.txt          (host iverilog/verilator golden; kept)
# The base rootfs-<arch>-alpine.img comes from the maintainer's TGOSKITS_ROOT
# checkout, NOT from this repo.
#
# OPTIONAL (FETCH_SOURCES=1): rebuild the static vvp from upstream source. This is
# the "source-build" class — the vvp-static-build/ dir ships xbuild-vvp.sh +
# vpi_modules.static-system.patch + BUILD.md. Only the upstream source tarball is
# fetched here (Icarus Verilog v12_0, URL from SOURCES.md). SOURCES.md provides NO
# sha256, so the download is left for manual verification; then follow BUILD.md.
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

echo "fetch-resources: iverilog — verifying retained self-built artifacts"
miss=0
for a in x86_64 aarch64 riscv64 loongarch64; do
    f="$HERE/testbin/vvp-$a"
    if [ -f "$f" ]; then echo "  [have] testbin/vvp-$a"; else echo "  [MISS] testbin/vvp-$a (source-build: vvp-static-build/, FETCH_SOURCES=1)"; miss=1; fi
done
[ -f "$HERE/dut.vvp" ]   && echo "  [have] dut.vvp"   || { echo "  [MISS] dut.vvp"; miss=1; }
[ -f "$HERE/golden.txt" ] && echo "  [have] golden.txt" || { echo "  [MISS] golden.txt"; miss=1; }

if [ "${FETCH_SOURCES:-0}" = "1" ]; then
    echo "fetch-resources: iverilog — OPTIONAL source-build: fetching Icarus Verilog v12_0 source"
    # URL from SOURCES.md / vvp-static-build/BUILD.md. No sha256 in SOURCES.md -> manual verify.
    fetch "https://codeload.github.com/steveicarus/iverilog/tar.gz/refs/tags/v12_0" \
          "$HERE/.srcbuild/iverilog-v12_0.tar.gz" "-"
    echo "  next: see vvp-static-build/BUILD.md (autoconf -> configure -> apply patch -> xbuild-vvp.sh <arch>)"
fi

[ "$miss" -eq 0 ] || echo "fetch-resources: iverilog — some artifacts absent; rebuild with FETCH_SOURCES=1 + vvp-static-build/BUILD.md" >&2
echo "fetch-resources: iverilog OK"
