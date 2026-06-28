#!/bin/bash
# prep-quarkus-rootfs.sh — build rootfs-<arch>-quarkus.img for the `quarkus-0`
# StarryOS stress case (Quarkus CLI 3.35.4 — the supersonic-subatomic-Java CLI).
#
# BASE = rootfs-<arch>-java.img (already carries the 4-arch musl OpenJDK17 JRE).
# The Quarkus case adds the official CLI distribution tarball, extracted under
# /opt/quarkus-cli-3.35.4. The CLI is a plain `java -jar` wrapper (bin/quarkus is
# `#!/usr/bin/env sh`, runs io.quarkus.cli.Main from lib/quarkus-cli-3.35.4-runner.jar),
# so it runs under busybox ash with only the JDK17 the base image already provides.
#
# WHY CLI-version + offline ops (not `quarkus create`/`dev`): project scaffolding +
# dev mode pull the Quarkus platform BOM + extension graph from Maven Central /
# the Quarkus registry over the network, which the offline starry guest cannot
# reach (documented honestly in CASE-NOTES). The DoD here is the CLI's OWN
# self-contained, fully-offline surface:
#   * `quarkus --version`  -> prints exactly "3.35.4"   (in-gate version assert)
#   * `quarkus --help`     -> prints the picocli usage banner + command list
#                             ("Usage: quarkus" + create/build/dev) — proves the
#                             whole CLI runtime (picocli + Quarkus bootstrap) ran.
# Both were captured on the host (java 17): `--version` -> "3.35.4", `--help` ->
# "Usage: quarkus ..." with the command table. These are deterministic and need
# no network, so they are a faithful "does the CLI actually run on starry" probe.
#
# Usage:  bash prep-quarkus-rootfs.sh <arch>   # arch in x86_64|aarch64|riscv64|loongarch64
#
# WSL2 NOTE: uses `debugfs -w` on the UNMOUNTED image (no mount, no sync) -> no
# D-state deadlock; only touches the quarkus image it creates.
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable: ROOT from env (your tgoskits checkout); CLI tarball co-located in package/.
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
TARBALL="$HERE/package/quarkus-cli-3.35.4.tar.gz"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-java.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-quarkus.img
STAGE=/tmp/quarkus-stage-$ARCH

[ -f "$BASE" ]    || { echo "missing base $BASE"; exit 2; }
[ -f "$TARBALL" ] || { echo "missing quarkus tarball $TARBALL"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }

# --- 1. copy base -> quarkus image --------------------------------------------
echo "=== [$ARCH] copy java base -> $IMG ==="
cp -f "$BASE" "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1

# --- 2. stage: extract the CLI tarball (top dir = quarkus-cli-3.35.4/) ---------
echo "=== [$ARCH] stage quarkus CLI -> $STAGE ==="
rm -rf "$STAGE"; mkdir -p "$STAGE"
tar xzf "$TARBALL" -C "$STAGE"
[ -x "$STAGE/quarkus-cli-3.35.4/bin/quarkus" ] || chmod +x "$STAGE/quarkus-cli-3.35.4/bin/quarkus" 2>/dev/null

# --- 3. inject the staged tree into /opt via debugfs -w (no mount, no sync) ----
echo "=== [$ARCH] inject /opt/quarkus-cli-3.35.4 into $IMG via debugfs -w ==="
DBG=/tmp/quarkus-debugfs-$ARCH.cmds
: > "$DBG"
echo "mkdir /opt" >> "$DBG"
( cd "$STAGE" && find . -type d | sort ) | while read -r d; do
  rel="${d#./}"; [ "$rel" = "." ] && continue
  echo "mkdir /opt/$rel" >> "$DBG"
done
( cd "$STAGE" && find . -type f | sort ) | while read -r f; do
  rel="${f#./}"
  echo "rm /opt/$rel" >> "$DBG"
  echo "write $STAGE/$rel /opt/$rel" >> "$DBG"
done
debugfs -w -f "$DBG" "$IMG" >/tmp/quarkus-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/quarkus-debugfs-$ARCH.log | grep -viE 'File exists.*mkdir|File not found.*rm|rm:.*not found' | head

# --- 4. verify the launcher + runner jar landed -------------------------------
echo "=== [$ARCH] verify quarkus CLI in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
debugfs -R "stat /opt/quarkus-cli-3.35.4/bin/quarkus" "$IMG" 2>/dev/null | grep -iE 'Inode|Size' | head -2
debugfs -R "stat /opt/quarkus-cli-3.35.4/lib/quarkus-cli-3.35.4-runner.jar" "$IMG" 2>/dev/null | grep -iE 'Inode|Size' | head -2
echo "[$ARCH] DONE -> $IMG"
