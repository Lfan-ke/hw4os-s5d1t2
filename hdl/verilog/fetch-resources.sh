#!/bin/bash
# fetch-resources.sh (hdl/verilog)
# -----------------------------------------------------------------------------
# Purpose: re-acquire any re-downloadable resources removed during repo slimming
# so that prep-verilog-rootfs.sh can build the rootfs. Needs network only if the
# OPTIONAL source rebuild below is requested. Run this BEFORE prep-*.sh.
#
# STATUS for this app: NOTHING needs downloading. Every resource consumed by
# prep-verilog-rootfs.sh is a self-built, hard-to-reproduce artifact retained in
# the slim repo:
#   - testbin/vsim-<arch>   (Verilator-verilated top.sv -> C++ -> musl-cross
#                            static sim; built host-side, kept)
#   - golden.txt            (host Verilator golden, kept)
# The base rootfs-<arch>-alpine.img comes from the maintainer's tgoskits checkout
# (TGOSKITS_ROOT), it is NOT a resource of this repo.
#
# OPTIONAL rebuild-from-source: the vsim-<arch> binaries are produced host-side by
# Verilator 5.008 (host `apt`) verilating top.sv + sim_main.cpp, then cross-compiled
# with /opt/<arch>-linux-musl-cross g++ (static). Verilator is a host-exclusive EDA
# tool (no fetchable repo artifact, no sha256 in SOURCES.md); install it from your
# distro / verilator.org and re-run the host verilate+cross-compile if you want to
# regenerate the kept binaries. No download is performed here.
# -----------------------------------------------------------------------------
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"

echo "fetch-resources: verilog — verifying retained self-built artifacts"
miss=0
for a in x86_64 aarch64 riscv64 loongarch64; do
    f="$HERE/testbin/vsim-$a"
    if [ -f "$f" ]; then echo "  [have] testbin/vsim-$a"; else echo "  [MISS] testbin/vsim-$a (self-built; rebuild via host Verilator 5.008 + musl-cross)"; miss=1; fi
done
[ -f "$HERE/golden.txt" ] && echo "  [have] golden.txt" || { echo "  [MISS] golden.txt"; miss=1; }

if [ "$miss" -ne 0 ]; then
    echo "fetch-resources: verilog — some self-built artifacts are absent." >&2
    echo "  These are NOT downloadable; regenerate them host-side (see SOURCES.md / the comment header)." >&2
fi

echo "fetch-resources: verilog OK"
