#!/bin/bash
# prep-ktor-rootfs.sh — build rootfs-<arch>-ktor.img for the `ktor-0` StarryOS
# stress case (Kotlin/JVM async web framework, Ktor 2.3.x on the Netty engine).
#
# BASE = rootfs-<arch>-java.img (already carries the 4-arch musl OpenJDK17 JRE +
# the java-apps dod payload). The Ktor case adds exactly ONE artifact: the
# host-compiled fat jar /root/ktor/ktor-demo.jar. Kotlin is compiled on the HOST
# (not on starry) to dodge the on-starry kotlinc crash #237, same strategy as
# exposed / springkt. The jar bundles Ktor + Netty + kotlinx.serialization +
# the Kotlin stdlib/coroutines runtime, so the guest needs no extra libs beyond
# the JRE that the java base image already provides.
#
# WHY a server case (not in-process self-test): the jar binds 127.0.0.1:18082,
# prints KTOR_READY, then BLOCKS. The qemu-<arch>.toml harness drives every route
# with busybox nc from the guest shell and asserts exact status line + body +
# the negotiated application/json content-type (genuine-stack proof), then
# `kill`s the JVM. This exercises the Kotlin coroutines runtime + Netty NIO event
# loop + Ktor routing/plugin pipeline end-to-end over the starry net stack
# (#223/#225) on IPv4 loopback.
#
# Usage:  bash prep-ktor-rootfs.sh <arch>     # arch in x86_64|aarch64|riscv64|loongarch64
#
# WSL2 NOTE: a bare global `sync` D-state-deadlocks this host.
# This script uses `debugfs -w` to write into the UNMOUNTED ext4 image directly,
# so it never mounts and never calls sync -> no deadlock. It only ever touches
# the ktor image it creates.
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable: ROOT from env (your tgoskits checkout); ktor-demo.jar shared with the
# dod-frameworks jars set (referenced, not duplicated).
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
JAR="$HERE/../../dod-frameworks/jars/ktor-demo.jar"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-java.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-ktor.img

[ -f "$BASE" ] || { echo "missing base $BASE"; exit 2; }
[ -f "$JAR" ]  || { echo "missing ktor jar $JAR"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }

# --- 1. copy base -> ktor image (java base is already ~3G, ample for one jar) -
echo "=== [$ARCH] copy java base -> $IMG ==="
cp -f "$BASE" "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1

# --- 2. inject /root/ktor/ktor-demo.jar via debugfs -w (no mount, no sync) -----
echo "=== [$ARCH] inject ktor-demo.jar into $IMG via debugfs -w ==="
DBG=/tmp/ktor-debugfs-$ARCH.cmds
{
  echo "mkdir /root/ktor"
  echo "rm /root/ktor/ktor-demo.jar"
  echo "write $JAR /root/ktor/ktor-demo.jar"
} > "$DBG"
debugfs -w -f "$DBG" "$IMG" >/tmp/ktor-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/ktor-debugfs-$ARCH.log | grep -viE 'File exists.*mkdir|File not found.*rm|rm:.*not found' | head

# --- 3. verify the jar landed --------------------------------------------------
echo "=== [$ARCH] verify ktor-demo.jar in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
debugfs -R "stat /root/ktor/ktor-demo.jar" "$IMG" 2>/dev/null | grep -iE 'Inode|Size' | head -2
echo "[$ARCH] DONE -> $IMG"
