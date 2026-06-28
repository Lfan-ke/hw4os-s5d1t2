#!/bin/bash
# prep-iceberg-rootfs.sh — build rootfs-<arch>-iceberg.img for the `iceberg-0` StarryOS
# stress case (#764 大应用 iceberg — Apache Iceberg table-format library on jdk17).
#
# WHAT IT DOES
#   Apache Iceberg ships ONLY as a LIBRARY jar (no standalone daemon). The smoke loads the
#   shaded Spark runtime jar (iceberg-spark-runtime-3.5_2.12-1.11.0.jar — 16844 org.apache.
#   iceberg classes) on the JVM classpath and builds an in-memory Schema. So this script
#   starts from the musl OpenJDK17 base image (rootfs-<arch>-java.img — JVM already present)
#   and injects just TWO files into /root/iceberg:
#     /root/iceberg/iceberg-spark-runtime-3.5_2.12-1.11.0.jar  (the library under test)
#     /root/iceberg/IcebergSmoke.java                          (single-file source driver)
#   The qemu toml launches it with Java 17 single-file source mode
#   (`java -cp <jar> /root/iceberg/IcebergSmoke.java`) — no precompiled .class, no bytecode
#   skew, the guest JVM compiles the driver to its own level.
#
#   The jar is pure Java bytecode (arch-independent), so the SAME jar serves all 4 archs;
#   the only per-arch wiring is the /etc/ld-musl-<arch>.path written by the toml at runtime.
#
# WSL2 RULE:
#   A bare global `sync` D-state-deadlocks the build host. This script uses `debugfs -w` to write
#   into the UNMOUNTED ext4 image directly — it NEVER mounts and NEVER calls sync, so there
#   is no deadlock. (Canonical recipe; mount+umount prep scripts are deprecated.)
#
# Usage:   bash prep-iceberg-rootfs.sh <arch>   # arch in x86_64|aarch64|riscv64|loongarch64
#
# Idempotent: re-runs cleanly (debugfs rm+write replaces files). Only touches the
# rootfs-<arch>-iceberg.img it creates; downloads nothing (jar already on disk).
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable: ROOT from env (your tgoskits checkout); jar+driver co-located in this app dir.
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
DL="$HERE"
JAR=$DL/iceberg-spark-runtime-3.5_2.12-1.11.0.jar
DRIVER=$DL/IcebergSmoke.java
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-java.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-iceberg.img
STAGE=/tmp/iceberg-stage-$ARCH

# Iceberg jar is arch-independent Java bytecode; arch only selects the java base image.
case "$ARCH" in
  x86_64|aarch64|riscv64|loongarch64) ;;
  *) echo "unknown arch $ARCH"; exit 2 ;;
esac

[ -f "$BASE" ]   || { echo "missing base $BASE"; exit 2; }
[ -f "$JAR" ]    || { echo "missing iceberg jar $JAR"; exit 2; }
[ -f "$DRIVER" ] || { echo "missing driver $DRIVER"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }

# --- 1. stage the jar + driver -------------------------------------------------
echo "=== [$ARCH] stage iceberg jar + driver -> $STAGE ==="
rm -rf "$STAGE"; mkdir -p "$STAGE/root/iceberg"
cp -f "$JAR"    "$STAGE/root/iceberg/iceberg-spark-runtime-3.5_2.12-1.11.0.jar"
cp -f "$DRIVER" "$STAGE/root/iceberg/IcebergSmoke.java"
echo "  staged:"; ls -la "$STAGE/root/iceberg/"

# --- 2. copy java base image (3G already, ample room for a 46M jar) -----------
echo "=== [$ARCH] copy java base -> $IMG ==="
cp -f "$BASE" "$IMG"

# --- 3. write the staged tree into the UNMOUNTED ext4 via debugfs -------------
# debugfs `rm`+`write` is idempotent for files; the dir is created with `mkdir`
# (errors for an already-existing dir are harmless and filtered from the log scan).
echo "=== [$ARCH] inject /root/iceberg into $IMG via debugfs -w (no mount, no sync) ==="
DBG=/tmp/iceberg-debugfs-$ARCH.cmds
: > "$DBG"
echo "mkdir /root/iceberg" >> "$DBG"
( cd "$STAGE" && find root/iceberg -type f | sort ) | while read -r f; do
  echo "rm /$f"              >> "$DBG"
  echo "write $STAGE/$f /$f" >> "$DBG"
done

debugfs -w -f "$DBG" "$IMG" >/tmp/iceberg-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/iceberg-debugfs-$ARCH.log \
  | grep -viE 'File exists.*mkdir|File not found.*rm|rm:.*not found' | head

# --- 4. verify the jar + driver landed ----------------------------------------
echo "=== [$ARCH] verify iceberg in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
debugfs -R "stat /root/iceberg/iceberg-spark-runtime-3.5_2.12-1.11.0.jar" "$IMG" 2>/dev/null | grep -iE 'Inode|Size' | head -2
debugfs -R "stat /root/iceberg/IcebergSmoke.java" "$IMG" 2>/dev/null | grep -iE 'Inode|Size' | head -2
debugfs -R "ls /root/iceberg" "$IMG" 2>/dev/null | tr ' ' '\n' | grep -E 'iceberg-spark-runtime.*\.jar|IcebergSmoke\.java'
echo "[$ARCH] DONE -> $IMG"
