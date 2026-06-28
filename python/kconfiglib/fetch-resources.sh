#!/bin/bash
# fetch-resources.sh — re-fetch the slimmed-away, re-downloadable resources for the
# `kconfiglib-0` StarryOS case so that prep-kconfiglib-rootfs.sh can build the rootfs again.
#
# Nothing to do: the kconfiglib case has a single resource — the pure-python, single-file
# parser ./kconfiglib.py (v14.1.0) — which is the test asset itself and is committed in this
# repo (it was NOT removed during slimming). prep-kconfiglib-rootfs.sh consumes only that
# file (debugfs-injected into the base rootfs-<arch>-python.img). No download is required.
#
# Run order: bash prep-kconfiglib-rootfs.sh <arch>   (this script is a no-op safeguard).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"

if [ -f "$HERE/kconfiglib.py" ]; then
  echo "  [ok] kconfiglib.py present (in-tree, no fetch needed)"
else
  echo "  [ERR] kconfiglib.py missing — it should be committed in this directory." >&2
  echo "        Restore it from upstream: https://github.com/ulfalizer/Kconfiglib (v14.1.0," >&2
  echo "        file kconfiglib.py) and place it beside this script." >&2
  exit 2
fi

echo "fetch-resources: kconfiglib OK"
