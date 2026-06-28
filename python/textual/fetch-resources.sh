#!/bin/bash
# fetch-resources.sh — re-fetch the slimmed-away, re-downloadable resources for the
# `textual-0` StarryOS case so that prep-textual-rootfs.sh can build the rootfs again.
#
# The delivery repo was slimmed: every artifact re-downloadable from an upstream
# registry was removed; only build/prep scripts + provenance were kept. This script
# restores them at the EXACT paths/filenames prep-textual-rootfs.sh expects
# (./wheels/<name>-<ver>-py3-none-any.whl) and verifies each by sha256.
#
# Requires: network access + a host python3 with pip. Run this FIRST, then run
#   bash prep-textual-rootfs.sh <arch>
#
# textual + rich + deps are all pure-python (py3-none-any, arch-independent) PyPI
# wheels. SOURCES.md documents acquisition as `pip download --only-binary=:all:`,
# so we fetch the pinned versions deterministically. The sha256 values are the digests
# of the artifacts that originally shipped here (authoritative ground truth; SOURCES.md
# carries no hashes).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WHEELS="$HERE/wheels"

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

echo "=== fetch textual + deps pure-python wheels (PyPI) -> $WHEELS ==="
pip_fetch "$WHEELS/textual-8.2.7-py3-none-any.whl" \
  4caaa13a90bc4cf9c6c862c067ccd34fe84e9c161710a2a907a8026313b6bd73 \
  --only-binary=:all: "textual==8.2.7"
pip_fetch "$WHEELS/rich-15.0.0-py3-none-any.whl" \
  33bd4ef74232fb73fe9279a257718407f169c09b78a87ad3d296f548e27de0bb \
  --only-binary=:all: "rich==15.0.0"
pip_fetch "$WHEELS/markdown_it_py-4.2.0-py3-none-any.whl" \
  9f7ebbcd14fe59494226453aed97c1070d83f8d24b6fc3a3bcf9a38092641c4a \
  --only-binary=:all: "markdown-it-py==4.2.0"
pip_fetch "$WHEELS/mdit_py_plugins-0.6.1-py3-none-any.whl" \
  214c82fb2ac524472ab6a5bcab1de80f73b50443e187f401bfd77efbc7c6481d \
  --only-binary=:all: "mdit-py-plugins==0.6.1"
pip_fetch "$WHEELS/mdurl-0.1.2-py3-none-any.whl" \
  84008a41e51615a49fc9966191ff91509e3c40b939176e643fd50a5c2196b8f8 \
  --only-binary=:all: "mdurl==0.1.2"
pip_fetch "$WHEELS/pygments-2.20.0-py3-none-any.whl" \
  81a9e26dd42fd28a23a2d169d86d7ac03b46e2f8b59ed4698fb4785f946d0176 \
  --only-binary=:all: "pygments==2.20.0"
pip_fetch "$WHEELS/typing_extensions-4.15.0-py3-none-any.whl" \
  f0fa19c6845758ab08074a0cfa8b7aecb71c999ca73d62883bc25cc018c4e548 \
  --only-binary=:all: "typing-extensions==4.15.0"
pip_fetch "$WHEELS/platformdirs-4.10.0-py3-none-any.whl" \
  fb516cdb12eb0d857d0cd85a7c57cea4d060bee4578d6cf5a14dfdf8cbf8784a \
  --only-binary=:all: "platformdirs==4.10.0"
pip_fetch "$WHEELS/linkify_it_py-2.1.0-py3-none-any.whl" \
  0d252c1594ecba2ecedc444053db5d3a9b7ec1b0dd929c8f1d74dce89f86c05e \
  --only-binary=:all: "linkify-it-py==2.1.0"
pip_fetch "$WHEELS/uc_micro_py-2.0.0-py3-none-any.whl" \
  3603a3859af53e5a39bc7677713c78ea6589ff188d70f4fee165db88e22b242c \
  --only-binary=:all: "uc-micro-py==2.0.0"

echo "fetch-resources: textual OK"
