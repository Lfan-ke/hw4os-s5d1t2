#!/bin/bash
# prep-trino-rootfs.sh — build rootfs-<arch>-trino.img for the `trino-0` StarryOS
# stress case (Trino 435 distributed SQL engine, via io.trino.testing.LocalQueryRunner).
#
# BASE = rootfs-<arch>-java.img (4-arch musl OpenJDK17). Trino 435 is the LAST release that
# runs on Java 17 (436+ require Java 22+), so no newer JDK is needed.
#
# WHY THE jna VERSION + MUSL libjnidispatch SWAP:
#   trino-435 bundles jna-5.13.0.jar whose com/sun/jna/linux-x86-64/libjnidispatch.so is
#   GLIBC-linked; on a musl libc (StarryOS/Alpine) musl's dynamic linker cannot relocate it
#   and OSHI's MachineInfo call aborts the JVM (SIGSEGV in Java_com_sun_jna_Native_sizeof).
#   The faithful Alpine practice is to install java-jna-native (musl-built libjnidispatch.so)
#   and a matching JNA jar. Alpine ships java-jna-native 5.15.0-r0 for x86_64/aarch64/
#   loongarch64; we ship the matching jna-5.15.0.jar (host-validated: ok=3 fail=0, drop-in
#   compatible with OSHI 6.4.8 + trino 435) AND tell JNA to load the staged musl native via
#   -Djna.boot.library.path + -Djna.nounpack=true (set in the toml).
#
#   riscv64: Alpine community does NOT package java-jna-native for riscv64 (any branch).
#   The script will warn and skip the native if it's absent; the toml uses the documented
#   MachineInfo shim as the riscv64-only fallback (engine unmodified).
#
# Stage layout in the image:
#   /root/trino/trino-demo.jar       (org.starry.dod.TrinoDemo, our test driver)
#   /root/trino/trino-libs/*.jar     (193 trino-435 runtime deps + jna-5.15.0.jar [swap])
#   /root/trino/jna/libjnidispatch.so (Alpine's musl build for this arch; absent on riscv64)
#   /root/trino/trino-machineinfo-shim.jar (used by riscv64 toml only)
#
# The maven artifacts (target/trino-demo.jar + target/trino-libs/) are co-located in
# package/. To regenerate them (on host, needs network), run from this app dir:
#   mvn -f package/pom.xml package -DskipTests
#
# WSL2: uses `debugfs -w` on the UNMOUNTED image (no mount, no sync). Only touches the
# trino image it creates. Adds ~134M; 3G java base has ample free space.
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable: ROOT from env (your tgoskits checkout); maven build output (demo + 193
# runtime libs) co-located under package/target/, jna jar+natives under jna/.
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
MVN="$HERE/package"
DEMO="$MVN/target/trino-demo.jar"
LIBS="$MVN/target/trino-libs"
JNA_NEW="$HERE/jna/jna-5.15.0.jar"
JNA_NATIVE="$HERE/jna/libjnidispatch-$ARCH-musl.so"
SHIM_JAR="$HERE/trino-machineinfo-shim.jar"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-java.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-trino.img
STAGE=/tmp/trino-stage-$ARCH

[ -f "$BASE" ]    || { echo "missing base $BASE"; exit 2; }
[ -f "$DEMO" ]    || { echo "missing $DEMO — run mvn package first"; exit 2; }
[ -d "$LIBS" ]    || { echo "missing $LIBS — run mvn package first"; exit 2; }
[ -f "$JNA_NEW" ] || { echo "missing $JNA_NEW (Alpine-compatible jna 5.15.0)"; exit 2; }
[ -f "$SHIM_JAR" ]|| { echo "missing $SHIM_JAR (MachineInfo shim)"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }

echo "=== [$ARCH] copy java base -> $IMG ==="
cp -f "$BASE" "$IMG"

echo "=== [$ARCH] stage trino -> $STAGE ==="
rm -rf "$STAGE"; mkdir -p "$STAGE/root/trino/trino-libs" "$STAGE/root/trino/jna"
cp -f "$DEMO" "$STAGE/root/trino/trino-demo.jar"
cp -f "$SHIM_JAR" "$STAGE/root/trino/trino-machineinfo-shim.jar"
# copy all maven-produced libs but EXCLUDE the buggy glibc-bundling jna-5.13.0.jar
for j in "$LIBS"/*.jar; do
  case "$(basename "$j")" in
    jna-5.13.0.jar) echo "  skipping $(basename "$j") (replaced by jna-5.15.0)" ;;
    *) cp -f "$j" "$STAGE/root/trino/trino-libs/" ;;
  esac
done
cp -f "$JNA_NEW" "$STAGE/root/trino/trino-libs/jna-5.15.0.jar"
if [ -f "$JNA_NATIVE" ]; then
  cp -f "$JNA_NATIVE" "$STAGE/root/trino/jna/libjnidispatch.so"
  echo "  staged Alpine musl libjnidispatch.so ($(stat -c%s "$JNA_NATIVE") bytes) for $ARCH"
else
  echo "  WARN: no musl libjnidispatch.so for $ARCH (Alpine doesn't ship it) — toml must use shim fallback"
fi
echo "  staged $(ls "$STAGE/root/trino/trino-libs" | wc -l) libs + demo + shim + jna-native"

echo "=== [$ARCH] inject /root/trino into $IMG via debugfs -w (no mount/sync) ==="
DBG=/tmp/trino-debugfs-$ARCH.cmds
: > "$DBG"
echo "mkdir /root/trino" >> "$DBG"
echo "mkdir /root/trino/trino-libs" >> "$DBG"
echo "mkdir /root/trino/jna" >> "$DBG"
( cd "$STAGE" && find root/trino -type f | sort ) | while read -r f; do
  echo "rm /$f" >> "$DBG"
  echo "write $STAGE/$f /$f" >> "$DBG"
done
debugfs -w -f "$DBG" "$IMG" >/tmp/trino-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/trino-debugfs-$ARCH.log | grep -viE 'File exists.*mkdir|File not found.*rm|rm:.*not found' | head

echo "=== [$ARCH] verify trino in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
debugfs -R "stat /root/trino/trino-demo.jar" "$IMG" 2>/dev/null | grep -iE 'Inode|Size' | head -2
debugfs -R "stat /root/trino/trino-libs/jna-5.15.0.jar" "$IMG" 2>/dev/null | grep -iE 'Inode|Size' | head -2
debugfs -R "stat /root/trino/jna/libjnidispatch.so" "$IMG" 2>/dev/null | grep -iE 'Inode|Size' | head -2 || echo "  (no native libjnidispatch — riscv64 path)"
echo "  trino-libs jar count in image:"
debugfs -R "ls -l /root/trino/trino-libs" "$IMG" 2>/dev/null | grep -c '\.jar' | sed 's/^/    /'
echo "[$ARCH] DONE -> $IMG"
