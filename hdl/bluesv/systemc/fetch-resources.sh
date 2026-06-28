#!/bin/bash
# fetch-resources.sh (hdl/bluesv/systemc)
# -----------------------------------------------------------------------------
# Purpose: re-acquire any re-downloadable resources removed during repo slimming
# so that prep-systemc-rootfs.sh can build the rootfs. Run BEFORE prep-*.sh.
# Network is needed only for the OPTIONAL source rebuild (FETCH_SOURCES=1).
#
# STATUS for this app: NOTHING is required for prep. Every resource consumed by
# prep-systemc-rootfs.sh is a self-built, hard-to-reproduce artifact kept in slim:
#   - testbin/sc_sim-<arch>      (SystemC testbench statically linked against the
#                                 musl-cross libsystemc; kept)
#   - sc-golden.txt              (host golden; kept)
# Also retained (the source-build inputs / products): lib/libsystemc-<arch>.a,
# systemc-2.3.4-starry.patch, endian.hpp.patched, sc_nbdefs.h.patched, sc_main.cpp.
# The base rootfs-<arch>-alpine.img comes from the maintainer's TGOSKITS_ROOT
# checkout, NOT from this repo.
#
# OPTIONAL (FETCH_SOURCES=1): rebuild libsystemc-<arch>.a from source. This is the
# "source-build" class: Accellera SystemC 2.3.4 + the in-repo
# systemc-2.3.4-starry.patch (2 source patches: boost endian.hpp + sc_nbdefs.h
# int64) cross-compiled musl-static (-DSC_USE_PTHREADS, -std=c++14, -j1; riscv64/
# loongarch64 -no-pie). SOURCES.md names "Accellera SystemC 2.3.4 (github tag)" but
# gives NO exact URL and NO sha256 — the canonical Accellera release URL below is a
# best-effort; VERIFY URL + sha256 manually against SOURCES.md before trusting it.
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

echo "fetch-resources: bluesv/systemc — verifying retained self-built artifacts"
miss=0
for a in x86_64 aarch64 riscv64 loongarch64; do
    [ -f "$HERE/testbin/sc_sim-$a" ]  && echo "  [have] testbin/sc_sim-$a"  || { echo "  [MISS] testbin/sc_sim-$a (rebuild from SystemC 2.3.4 source, FETCH_SOURCES=1)"; miss=1; }
    [ -f "$HERE/lib/libsystemc-$a.a" ] && echo "  [have] lib/libsystemc-$a.a" || { echo "  [MISS] lib/libsystemc-$a.a"; miss=1; }
done
[ -f "$HERE/sc-golden.txt" ]              && echo "  [have] sc-golden.txt"              || { echo "  [MISS] sc-golden.txt"; miss=1; }
[ -f "$HERE/systemc-2.3.4-starry.patch" ] && echo "  [have] systemc-2.3.4-starry.patch" || { echo "  [MISS] systemc-2.3.4-starry.patch"; miss=1; }

if [ "${FETCH_SOURCES:-0}" = "1" ]; then
    echo "fetch-resources: bluesv/systemc — OPTIONAL source-build: fetching Accellera SystemC 2.3.4 source"
    # URL NOT in SOURCES.md (only "github tag"); canonical best-effort — verify URL + sha256 manually.
    fetch "https://github.com/accellera-official/systemc/archive/refs/tags/2.3.4.tar.gz" \
          "$HERE/.srcbuild/systemc-2.3.4.tar.gz" "-"
    echo "  next: apply systemc-2.3.4-starry.patch, then cross-build musl-static per SOURCES.md"
    echo "        (-DSC_USE_PTHREADS -std=c++14 make -j1; riscv64/loongarch64 add -no-pie -fno-pie)"
fi

[ "$miss" -eq 0 ] || echo "fetch-resources: bluesv/systemc — some artifacts absent; rebuild per SOURCES.md (SystemC 2.3.4 + in-repo patch)" >&2
echo "fetch-resources: bluesv/systemc OK"
