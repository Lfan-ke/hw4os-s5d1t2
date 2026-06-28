#!/bin/bash
# prep-neo4j-rootfs.sh — build rootfs-<arch>-neo4j.img for the `neo4j-0` StarryOS
# stress case (#764 "neo4j" big-app item under the jdk17+/JVM sub-deps roadmap).
#
# WHAT IT DOES
#   Starts from the MULTI-JDK musl base image (rootfs-<arch>-jdk-multi.img — Alpine
#   musl + /opt/jdk17 /opt/jdk21 /opt/jdk23 /opt/jdk25 + ld-musl path already wired
#   for all four JDKs), then unpacks the OFFICIAL neo4j-community 2026.04.0 release
#   tarball and injects the 235 runtime jars (lib/*.jar, ~139 MB) into /opt/neo4j/lib
#   plus the host-precompiled embedded-smoke driver Neo4jEmbeddedSmoke.class into
#   /opt/neo4j.
#
#   WHY jdk-multi (NOT rootfs-<arch>-java.img): neo4j-community 2026.04.0 is a PURE
#   Java distribution (no native per-arch binaries; the tarball ships only
#   *.jar + shell scripts), but its core jars (neo4j-2026.04.0.jar, neo4j-kernel,
#   neo4j-dbms, neo4j-server, neo4j-bolt) are compiled to CLASS MAJOR VERSION 65 =
#   Java 21. The rootfs-java.img base ships OpenJDK 17 (class-major <=61), which
#   HARD-FAILS with java.lang.UnsupportedClassVersionError ("class file version 65.0
#   ... only recognizes up to 61.0"). neo4j's own NeoBoot launcher
#   also explicitly refuses to start: "Unsupported Java <ver> detected. Please use
#   Java(TM) 21 or Java(TM) 25 to run Neo4j Server." So the case targets /opt/jdk21
#   (BellSoft 21.0.11 on x86_64/aarch64, Alpine 21.0.11 on riscv64/loongarch64 —
#   all musl; image /opt/jdk21/release reports LIBC="musl").
#
#   WHY embedded smoke (NOT the full `neo4j console` HTTP server): the full server
#   (NeoBoot -> forked server JVM, Bolt :7687 + HTTP :7474 + system DB bootstrap) is
#   extremely heavy — it blocks for >5 min producing no output even on
#   native x86 with JIT, and starry forces -Xint (JIT unstable, #206). Instead we run
#   the REAL graph-DB kernel embedded in one JVM via DatabaseManagementServiceBuilder
#   (the same kernel the server wraps): page cache + record/token stores + the
#   transaction log + the Cypher parser/planner/runtime. End-to-end on
#   JDK21 -Xint the run reports NEO4J_RESULT pass=5 total=5 + NEO4J_SMOKE_DONE in ~5 min.
#
# WSL2 RULE:
#   A bare global `sync` D-state-deadlocks the build host. This script uses `debugfs -w`
#   to write into the UNMOUNTED ext4 image directly — it NEVER mounts and NEVER calls
#   sync, so there is no deadlock. (Canonical recipe; mount+umount prep is deprecated.)
#
# Usage:   bash prep-neo4j-rootfs.sh <arch>    # arch in x86_64|aarch64|riscv64|loongarch64
#
# Idempotent: re-runs cleanly (debugfs rm+write replaces files). Only touches the
# rootfs-<arch>-neo4j.img it creates; downloads nothing (tarball already on disk).
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable: ROOT from env (your tgoskits checkout); all materials co-located in this
# app dir ($HERE) — tarball under packages/, musl jnidispatch under jna/.
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-jdk-multi.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-neo4j.img
STAGE=/tmp/neo4j-stage-$ARCH

NEO4J_VER=2026.04.0
TARBALL=$HERE/packages/neo4j-community-$NEO4J_VER-unix.tar.gz
TOPDIR=neo4j-community-$NEO4J_VER        # directory name INSIDE the tarball
DRIVER_CLASS=$HERE/Neo4jEmbeddedSmoke.class   # host-precompiled (JDK21, class-major 65)

# --- musl jnidispatch (THE JNA-on-musl fix) -----------------------------------
# JNA bundles a glibc-built libjnidispatch.so in its jar; it SIGSEGVs on musl. We
# inject a musl-built one at /opt/jna and launch java with
# -Djna.boot.library.path=/opt/jna -Djna.nounpack=true (wired in the qemu tomls).
# riscv64/loongarch64 ship a per-arch musl .so under jna/; x86_64/aarch64 already
# have it inside their jdk-multi base (java-jna-native apk), so no inject needed.
case "$ARCH" in
  riscv64)      JNIDISPATCH="$HERE/jna/libjnidispatch-riscv64-musl.so" ;;
  loongarch64)  JNIDISPATCH="$HERE/jna/libjnidispatch-loongarch64-musl.so" ;;
  *)            JNIDISPATCH="" ;;
esac

# neo4j-community is arch-INDEPENDENT (pure jars), so there is no per-arch release
# token to map — every starry arch uses the SAME tarball. We only sanity-check that
# the matching jdk-multi base image exists for the requested arch.
case "$ARCH" in
  x86_64|aarch64|riscv64|loongarch64) ;;
  *) echo "unknown arch $ARCH"; exit 2 ;;
esac

[ -f "$BASE" ]         || { echo "missing base $BASE (need rootfs-$ARCH-jdk-multi.img)"; exit 2; }
[ -f "$TARBALL" ]      || { echo "missing neo4j tarball $TARBALL"; exit 2; }
[ -f "$DRIVER_CLASS" ] || { echo "missing driver $DRIVER_CLASS"; exit 2; }
[ -z "$JNIDISPATCH" ] || [ -f "$JNIDISPATCH" ] || { echo "missing musl jnidispatch $JNIDISPATCH"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }

# --- 1. stage the neo4j runtime jars + embedded-smoke driver -------------------
echo "=== [$ARCH] extract neo4j $NEO4J_VER lib/*.jar from $(basename "$TARBALL") ==="
rm -rf "$STAGE"; mkdir -p "$STAGE/opt/neo4j/lib"
# only the runtime jars are needed for the embedded smoke (no conf/bin/data).
tar xzf "$TARBALL" -C "$STAGE/opt/neo4j/lib" --strip-components=2 "$TOPDIR/lib" 2>/dev/null \
  || { echo "  tar extract FAIL"; exit 2; }
cp "$DRIVER_CLASS" "$STAGE/opt/neo4j/Neo4jEmbeddedSmoke.class"
NJARS=$(ls "$STAGE/opt/neo4j/lib"/*.jar 2>/dev/null | wc -l)
echo "  staged $NJARS jars + Neo4jEmbeddedSmoke.class"
[ "$NJARS" -ge 200 ] || { echo "  expected >=200 jars, got $NJARS — abort"; exit 2; }

# --- 1b. stage the per-arch musl jnidispatch at /opt/jna (JNA-on-musl fix) -----
# Without this, JNA unpacks its glibc-built jnidispatch from the jar and SIGSEGVs on
# musl. riscv64/loongarch64 have no Alpine apk, so we ship a cross-built musl .so.
if [ -n "$JNIDISPATCH" ]; then
  mkdir -p "$STAGE/opt/jna"
  cp "$JNIDISPATCH" "$STAGE/opt/jna/libjnidispatch.so"
  echo "  staged /opt/jna/libjnidispatch.so <- $(basename "$JNIDISPATCH")"
else
  echo "  [$ARCH] no musl jnidispatch inject (provided by jdk-multi base)"
fi

# --- 2. copy base image, grow to 8G (6G jdk-multi base + 139M jars + headroom) -
# NB: the jdk-multi base is ~6G (4 JDKs 17/21/23/25). MUST grow to >6G, never
# shrink — `truncate -s 4G` on a 6G base cut off ext4 blocks and corrupted the
# filesystem ("Can't read block bitmap" -> debugfs "Filesystem not open" -> 0 jars).
echo "=== [$ARCH] copy base -> $IMG (resize 8G) ==="
cp -f "$BASE" "$IMG"
truncate -s 8G "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1
resize2fs "$IMG" >/dev/null 2>&1

# --- 3. write the staged tree into the UNMOUNTED ext4 via debugfs -------------
# debugfs `rm`+`write` is idempotent for files; dirs via `mkdir` (already-exists
# errors are harmless and filtered from the log scan).
echo "=== [$ARCH] inject tree into $IMG via debugfs -w (no mount, no sync) ==="
DBG=/tmp/neo4j-debugfs-$ARCH.cmds
: > "$DBG"
# directories first (shallow->deep)
( cd "$STAGE" && find . -type d | sort ) | while read -r d; do
  rel="${d#./}"; [ "$rel" = "." ] && continue
  echo "mkdir /$rel" >> "$DBG"
done
# files: rm-then-write so a re-run replaces cleanly
( cd "$STAGE" && find . -type f | sort ) | while read -r f; do
  rel="${f#./}"
  echo "rm /$rel"                 >> "$DBG"
  echo "write $STAGE/$rel /$rel"  >> "$DBG"
done

debugfs -w -f "$DBG" "$IMG" >/tmp/neo4j-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/neo4j-debugfs-$ARCH.log \
  | grep -viE 'File not found.*rm|rm:.*not found|File exists.*mkdir' | head

# --- 4. verify the jars + driver landed ---------------------------------------
echo "=== [$ARCH] verify neo4j payload in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
INIMG=$(debugfs -R "ls -l /opt/neo4j/lib" "$IMG" 2>/dev/null | grep -c '\.jar')
echo "  /opt/neo4j/lib jar count in image: $INIMG"
debugfs -R "stat /opt/neo4j/Neo4jEmbeddedSmoke.class" "$IMG" 2>/dev/null | grep -iE 'Inode|Mode' | head -1
debugfs -R "stat /opt/neo4j/lib/neo4j-kernel-$NEO4J_VER.jar" "$IMG" 2>/dev/null | grep -iE 'Inode' | head -1
if [ -n "$JNIDISPATCH" ]; then
  debugfs -R "stat /opt/jna/libjnidispatch.so" "$IMG" 2>/dev/null | grep -iE 'Inode|Size' | head -2
fi
echo "[$ARCH] DONE -> $IMG"
