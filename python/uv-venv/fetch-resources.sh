#!/bin/bash
# fetch-resources.sh — re-acquire the slimmed-away, re-downloadable resources for
# python/uv-venv.
#
# python/uv-venv/prep-python-rootfs.sh consumes its apk closure from the SHARED
# location ../core/apks (DL="$HERE/../core/apks") — the same musl-native CPython +
# numpy/scikit-learn/opencv closure that python/core owns (uv binary included in
# that closure). Nothing is stored under python/uv-venv itself, so re-fetching is a
# delegation to python/core's fetch-resources.sh, which repopulates
# ../core/apks/<arch> via apk-closure.py.
#
# Needs network. After this completes, run:  bash prep-python-rootfs.sh <arch>
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
CORE_FETCH="$HERE/../core/fetch-resources.sh"
[ -f "$CORE_FETCH" ] || { echo "missing shared fetcher $CORE_FETCH" >&2; exit 1; }

echo "=== python/uv-venv resources are shared with python/core (../core/apks) — delegating ==="
bash "$CORE_FETCH"

echo "fetch-resources: python/uv-venv OK"
