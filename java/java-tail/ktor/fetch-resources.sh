#!/bin/bash
# fetch-resources.sh — re-fetch the re-downloadable resources removed from this
# slimmed delivery tree for the `ktor-0` StarryOS case.
#
# NOTHING TO FETCH: the only payload prep-ktor-rootfs.sh consumes is the
# host-compiled fat jar ../../dod-frameworks/jars/ktor-demo.jar — a self-built
# (non-redownloadable) artifact that is KEPT in this tree. No upstream binaries
# were removed for this app, so there is nothing to re-download here.
#
# After confirming ktor-demo.jar is present, run prep-ktor-rootfs.sh <arch>.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"

JAR="$HERE/../../dod-frameworks/jars/ktor-demo.jar"
if [ -f "$JAR" ]; then
  echo "  present (kept self-built artifact): dod-frameworks/jars/ktor-demo.jar"
else
  echo "  ERROR: expected kept artifact missing: $JAR" >&2
  echo "         ktor-demo.jar is a host-compiled fat jar, not a re-downloadable upstream binary." >&2
  exit 1
fi

echo "fetch-resources: ktor OK"
