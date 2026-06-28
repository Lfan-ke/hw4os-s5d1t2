#!/bin/bash
# fetch-resources.sh (hdl/gnumake)
# -----------------------------------------------------------------------------
# Purpose: re-acquire any re-downloadable resources removed during repo slimming
# so that prep-gnumake-rootfs.sh can build the rootfs. Run BEFORE prep-*.sh.
# Network is needed only for the OPTIONAL source rebuild (FETCH_SOURCES=1).
#
# STATUS for this app: NOTHING is required for prep. Every resource consumed by
# prep-gnumake-rootfs.sh is a self-built, hard-to-reproduce artifact kept in slim:
#   - testbin/make-<arch>   (GNU Make 4.4.1 cross-compiled musl-static
#                            ./configure --host=<arch>-linux-musl LDFLAGS=-static; kept)
#   - Makefile              (feature-exercising language test; kept)
#   - golden.txt            (host golden; kept)
# The base rootfs-<arch>-alpine.img comes from the maintainer's TGOSKITS_ROOT
# checkout, NOT from this repo.
#
# OPTIONAL (FETCH_SOURCES=1): rebuild make-<arch> from source. SOURCES.md names
# "GNU Make 4.4.1 (ftp.gnu.org)" but gives NO sha256; the canonical ftp.gnu.org URL
# below is the official pattern — VERIFY sha256 manually. Then cross-build:
#   ./configure --host=<arch>-linux-musl CC=<arch>-linux-musl-gcc LDFLAGS=-static && make
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

echo "fetch-resources: gnumake — verifying retained self-built artifacts"
miss=0
for a in x86_64 aarch64 riscv64 loongarch64; do
    [ -f "$HERE/testbin/make-$a" ] && echo "  [have] testbin/make-$a" || { echo "  [MISS] testbin/make-$a (rebuild from GNU Make 4.4.1 source, FETCH_SOURCES=1)"; miss=1; }
done
[ -f "$HERE/Makefile" ]   && echo "  [have] Makefile"   || { echo "  [MISS] Makefile"; miss=1; }
[ -f "$HERE/golden.txt" ] && echo "  [have] golden.txt" || { echo "  [MISS] golden.txt"; miss=1; }

if [ "${FETCH_SOURCES:-0}" = "1" ]; then
    echo "fetch-resources: gnumake — OPTIONAL source-build: fetching GNU Make 4.4.1 source"
    # ftp.gnu.org canonical URL (SOURCES.md names the host but not the exact file/sha256) — verify sha256 manually.
    fetch "https://ftp.gnu.org/gnu/make/make-4.4.1.tar.gz" \
          "$HERE/.srcbuild/make-4.4.1.tar.gz" "-"
    echo "  next: ./configure --host=<arch>-linux-musl LDFLAGS=-static && make  (per SOURCES.md)"
fi

[ "$miss" -eq 0 ] || echo "fetch-resources: gnumake — some artifacts absent; rebuild per SOURCES.md (GNU Make 4.4.1 musl-cross static)" >&2
echo "fetch-resources: gnumake OK"
