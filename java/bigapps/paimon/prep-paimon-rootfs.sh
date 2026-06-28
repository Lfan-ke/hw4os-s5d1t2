#!/bin/bash
# prep-paimon-rootfs.sh — build rootfs-<arch>-paimon.img for the `paimon-0` StarryOS
# stress case (Apache Paimon 1.4.1 lake-format LIBRARY, flink-1.20 bundle jar).
#
# BASE = rootfs-<arch>-java.img (4-arch musl OpenJDK17 JRE). Paimon 1.4.1's bundle jar
# is PURE Java bytecode (architecture-INDEPENDENT — one jar runs on all 4 archs' musl
# JRE), so no per-arch artifact and no newer JDK is needed: the smoke only touches the
# paimon-core type system + schema builder, which are JDK17-clean.
#
# WHY a LIBRARY smoke (not a server): paimon-flink-1.20-1.4.1.jar is a bundle LIBRARY
# (paimon-core + flink-1.20 connector) — there is no daemon to boot. The single-core
# smoke is a tiny host-precompiled driver (org PaimonSmoke, class file v61 == Java 17)
# that loads core Paimon classes and builds a trivial table schema:
#   1) DataTypes.INT()/STRING()              — paimon-core type module loads
#   2) RowType.of(id INT, name STRING)       — columnar RowType assembly (fieldCount==2)
#   3) Schema.newBuilder().column(..).primaryKey("id").build() — public table-schema API
# On OpenJDK17 (-Xint -Xmx512m) the run reports PAIMON_RESULT pass=3 total=3, and a
# -verbose:class trace shows NO jna/oshi/custom-native .so dlopen (only the JVM's own
# CDS bootstrap natives), so there is NONE of the musl-libjnidispatch hazard that the
# trino case had — this is a light, fast, deterministic class-load + API smoke (1022
# classes loaded total on host).
#
# Stage layout in the image (under /root/paimon):
#   /root/paimon/paimon-flink-1.20-1.4.1.jar  (the Apache Paimon bundle library, 55 MB)
#   /root/paimon/paimon-smoke.jar             (our host-compiled PaimonSmoke driver, ~2 KB)
# The qemu-<arch>.toml puts BOTH on the classpath and runs `java ... PaimonSmoke`.
#
# Usage:  bash prep-paimon-rootfs.sh <arch>   # arch in x86_64|aarch64|riscv64|loongarch64
#
# WSL2 NOTE: a bare global `sync` D-state-deadlocks the build host. This
# script uses `debugfs -w` to write into the UNMOUNTED ext4 image directly, so it never
# mounts and never calls sync -> no deadlock. It only ever touches the paimon image it
# creates. Idempotent: re-runs cleanly (debugfs rm+write replaces files).
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable: ROOT from env (your tgoskits checkout); bundle jar + smoke jar co-located here.
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
JAR="$HERE/paimon-flink-1.20-1.4.1.jar"   # arch-independent bundle lib
SMOKE="$HERE/paimon-smoke.jar"            # host-precompiled (class v61)
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-java.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-paimon.img

# Paimon's bundle jar + JRE are arch-independent (same file all 4 archs); the only thing
# that varies is which java base image we copy. Validate the arch token early all the same.
case "$ARCH" in
  x86_64|aarch64|riscv64|loongarch64) : ;;
  *) echo "unknown arch $ARCH"; exit 2 ;;
esac

[ -f "$BASE" ]  || { echo "missing base $BASE"; exit 2; }
[ -f "$JAR" ]   || { echo "missing paimon bundle jar $JAR"; exit 2; }
[ -f "$SMOKE" ] || { echo "missing smoke jar $SMOKE (host-compile PaimonSmoke.java first)"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }

# --- 1. copy java base -> paimon image (3G java base, ample for one 55M jar) --
echo "=== [$ARCH] copy java base -> $IMG ==="
cp -f "$BASE" "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1

# --- 2. inject /root/paimon/{bundle,smoke}.jar via debugfs -w (no mount/sync) --
echo "=== [$ARCH] inject paimon jars into $IMG via debugfs -w (no mount, no sync) ==="
DBG=/tmp/paimon-debugfs-$ARCH.cmds
{
  echo "mkdir /root/paimon"
  echo "rm /root/paimon/paimon-flink-1.20-1.4.1.jar"
  echo "write $JAR /root/paimon/paimon-flink-1.20-1.4.1.jar"
  echo "rm /root/paimon/paimon-smoke.jar"
  echo "write $SMOKE /root/paimon/paimon-smoke.jar"
} > "$DBG"
debugfs -w -f "$DBG" "$IMG" >/tmp/paimon-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/paimon-debugfs-$ARCH.log \
  | grep -viE 'File exists.*mkdir|File not found.*rm|rm:.*not found' | head

# --- 3. verify the jars landed -------------------------------------------------
echo "=== [$ARCH] verify paimon jars in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
debugfs -R "stat /root/paimon/paimon-flink-1.20-1.4.1.jar" "$IMG" 2>/dev/null | grep -iE 'Inode|Size' | head -2
debugfs -R "stat /root/paimon/paimon-smoke.jar"            "$IMG" 2>/dev/null | grep -iE 'Inode|Size' | head -2
echo "[$ARCH] DONE -> $IMG"
