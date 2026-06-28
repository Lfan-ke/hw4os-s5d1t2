#!/bin/bash
# fetch-resources.sh — re-fetch the slimmed-away, re-downloadable resources for the
# `jupyter-lab-0` StarryOS case so that prep-jupyter-rootfs.sh can build the rootfs again.
#
# The delivery repo was slimmed: the per-arch Alpine apk closure and the front-end PyPI
# wheels were removed (only build/prep scripts + the resolver apk-closure.py were kept).
# This script restores them at the EXACT paths/filenames prep-jupyter-rootfs.sh expects:
#   ./apks/<arch>/*.apk            (Alpine edge native + noarch closure, 4 arches)
#   ./apks/wheels/<name>-<ver>-py3-none-any.whl   (arch-independent JupyterLab front-end)
#
# Requires: network access + a host python3 with pip. Run this FIRST, then run
#   bash prep-jupyter-rootfs.sh <arch>
#
# APK CLOSURE (resolver, not direct links): there is NO fixed URL list to pin — the closure
# is resolved transitively from the live Alpine *edge* APKINDEX by ./apks/apk-closure.py
# (the very same resolver prep-jupyter-rootfs.sh calls). We invoke it here for all 4 arches
# with the identical --repo roots prep uses, so apks/<arch>/ is fully pre-populated. Because
# Alpine edge floats, the resolved versions (e.g. llvm22-libs, py3-*) may differ from the
# set originally shipped here; this is intentional and matches prep's own self-download path,
# so apks are NOT sha256-pinned. Override the mirror via APK_MIRROR=<base> if dl-cdn is slow.
#
# FRONT-END WHEELS (pinned PyPI, py3-none-any, arch-independent): jupyterlab 4.5.7 +
# jupyterlab-server 2.28.0 + jupyter-lsp 2.3.1. These ARE sha256-verified; the digests are
# those of the artifacts that originally shipped here (authoritative ground truth).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
APKS="$HERE/apks"
CLOSURE="$APKS/apk-closure.py"
WHEELDIR="$APKS/wheels"
ARCHES="x86_64 aarch64 riscv64 loongarch64"

[ -f "$CLOSURE" ] || { echo "missing resolver $CLOSURE" >&2; exit 2; }

# pip_fetch <dest-file> <sha256> <pip-download-args...>
pip_fetch() {
  local dest="$1" want="$2"; shift 2
  local base; base="$(basename "$dest")"
  if [ -f "$dest" ] && [ "$(sha256sum "$dest" | awk '{print $1}')" = "$want" ]; then
    echo "  [skip] $base (sha256 ok)"; return 0
  fi
  if [ -f "$dest" ]; then echo "  [stale] $base -> re-download"; rm -f "$dest"; fi
  mkdir -p "$(dirname "$dest")"
  local td; td="$(mktemp -d)"
  if ! python3 -m pip download --no-deps --disable-pip-version-check -d "$td" "$@"; then
    rm -rf "$td"; echo "  [ERR] pip download failed: $*" >&2; return 1
  fi
  if [ ! -f "$td/$base" ]; then
    echo "  [ERR] expected '$base' not produced by: pip download $*" >&2
    echo "        produced: $(cd "$td" && ls)" >&2
    rm -rf "$td"; return 1
  fi
  local got; got="$(sha256sum "$td/$base" | awk '{print $1}')"
  if [ "$got" != "$want" ]; then
    echo "  [ERR] sha256 mismatch for $base: got $got want $want" >&2
    rm -rf "$td"; return 1
  fi
  mv "$td/$base" "$dest"; rm -rf "$td"
  echo "  [ok] $base"
}

# --- 1. resolve+download the per-arch Alpine edge apk closure (all 4 arches) -------
MIRROR_ARG=()
if [ -n "${APK_MIRROR:-}" ]; then MIRROR_ARG=(--mirror "$APK_MIRROR"); fi
for arch in $ARCHES; do
  out="$APKS/$arch"; mkdir -p "$out"
  echo "=== [$arch] resolve+download jupyter-server native closure -> $out ==="
  # apk-closure.py: 0 ok, 1 some downloads failed (soft), >=2 fatal (unsatisfiable root/dep).
  # Capture rc with `|| rc=$?` so `set -e` does not abort on a soft (1) result.
  rc=0
  python3 "$CLOSURE" --arch "$arch" --out "$out" "${MIRROR_ARG[@]}" \
    --repo edge/main python3 py3-jinja2 py3-requests py3-packaging py3-babel \
    --repo edge/community jupyter-server py3-ipykernel py3-async-lru \
           jupyter-notebook-shim py3-json5 py3-jsonschema py3-pyzmq py3-httpx || rc=$?
  if [ "$rc" -ge 2 ]; then echo "  RESOLVER FATAL ($rc) for $arch" >&2; exit 2; fi
  n=$(find "$out" -maxdepth 1 -name '*.apk' | wc -l)
  echo "  closure apks for $arch: $n"
  if [ "$n" -lt 120 ]; then echo "  closure too small ($n) for $arch — aborting" >&2; exit 2; fi
done

# --- 2. fetch the arch-independent JupyterLab front-end wheels (pinned PyPI) --------
echo "=== fetch JupyterLab front-end wheels (PyPI, py3-none-any) -> $WHEELDIR ==="
pip_fetch "$WHEELDIR/jupyterlab-4.5.7-py3-none-any.whl" \
  fba4cb0e2c44a52859669d8c98b45de029d5e515f8407bf8534d2a8fc5f0964d \
  --only-binary=:all: "jupyterlab==4.5.7"
pip_fetch "$WHEELDIR/jupyterlab_server-2.28.0-py3-none-any.whl" \
  e4355b148fdcf34d312bbbc80f22467d6d20460e8b8736bf235577dd18506968 \
  --only-binary=:all: "jupyterlab-server==2.28.0"
pip_fetch "$WHEELDIR/jupyter_lsp-2.3.1-py3-none-any.whl" \
  71b954d834e85ff3096400554f2eefaf7fe37053036f9a782b0f7c5e42dadb81 \
  --only-binary=:all: "jupyter-lsp==2.3.1"

echo "fetch-resources: jupyter OK"
