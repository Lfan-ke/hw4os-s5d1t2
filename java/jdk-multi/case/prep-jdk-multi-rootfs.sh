#!/bin/bash
# prep-jdk-multi-rootfs.sh — build rootfs-<arch>-jdk-multi.img for the
# `openjdk-multi-0` StarryOS stress case (#764 jdk17+ "openjdk 17 21 23 25 +
# update-alternatives" parent item).
#
# Stages FOUR JDKs side-by-side under /opt/jdk{17,21,23,25} (the
# update-alternatives candidate roots) so the case can:
#   1. run a version-specific language/stdlib/syntax feature test on EACH JDK,
#      asserting `java -version` reports the right major (17/21/23/25), and
#   2. exercise update-alternatives-style version switching (JAVA_HOME + a
#      retargeted /opt/jdk-current symlink) + an sdkman-style candidate switch.
#
# Per-arch JDK source mapping (all musl where available; glibc noted):
#                JDK17                 JDK21              JDK23              JDK25
#   x86_64       apk (openjdk17)       BellSoft musl tar  BellSoft musl tar  BellSoft musl tar
#   aarch64      apk (openjdk17)       BellSoft musl tar  BellSoft musl tar  BellSoft musl tar
#   riscv64      native-musl cross tar BellSoft glibc tar BellSoft glibc tar BellSoft glibc tar   (21/23/25 glibc -> need gcompat)
#   loongarch64  apk (openjdk17-loong) Loongson glibc tar Loongson glibc tar Alpine musl apks     (21/23 glibc -> gcompat; 25 native musl)
#
# WSL2 NOTE: a bare global `sync` D-state-deadlocks this host.
# This script uses `debugfs -w` (write into the UNMOUNTED ext4 image directly),
# so it NEVER mounts and NEVER calls sync -> no deadlock.
#
# Usage:   bash prep-jdk-multi-rootfs.sh <arch>     # x86_64|aarch64|riscv64|loongarch64
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable: ROOT from env (your tgoskits checkout); materials co-located in-repo.
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
# JDK17 apks/cross-tar are shared with the openjdk17 app (referenced, not duplicated);
# JDK21/23/25 packages + the version-feature .java sources live in this app dir.
OJ17="$HERE/../../openjdk17/packages"   # shared JDK17 apk/cross-tar dir (dedup)
JM="$HERE/.."                           # jdk-multi app dir: packages/ + programs/
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-jdk-multi.img
STAGE=/tmp/jdkmulti-stage-$ARCH
DBG=/tmp/jdkmulti-debugfs-$ARCH.cmds

[ -f "$BASE" ] || { echo "missing base $BASE"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }

rm -rf "$STAGE"; mkdir -p "$STAGE"
shopt -s nullglob

# untar_into <archive> <dest_under_stage> [strip_top_component(1|0)]
#   extracts a .tar.gz, optionally stripping the single top-level dir, into STAGE/<dest>
untar_into() {
  local arc="$1" dest="$2" strip="${3:-1}"
  [ -f "$arc" ] || { echo "  !! missing archive $arc"; return 1; }
  mkdir -p "$STAGE/$dest"
  if [ "$strip" = 1 ]; then
    tar xzf "$arc" -C "$STAGE/$dest" --strip-components=1
  else
    tar xzf "$arc" -C "$STAGE/$dest"
  fi
}

# apk_into <apk> <dest_under_stage>
#   extracts an Alpine apk (gzip tar) into STAGE/<dest>, dropping apk metadata.
apk_into() {
  local apk="$1" dest="$2"
  [ -f "$apk" ] || { echo "  !! missing apk $apk"; return 1; }
  mkdir -p "$STAGE/$dest"
  tar xzf "$apk" -C "$STAGE/$dest" \
    --exclude='.PKGINFO' --exclude='.SIGN.*' --exclude='.pre-install' \
    --exclude='.post-install' --exclude='.pre-upgrade' --exclude='.post-upgrade' \
    --exclude='.trigger' 2>/dev/null
}

echo "=== [$ARCH] stage 4 JDKs into $STAGE/opt/jdk{17,21,23,25} ==="

# ------------------------------------------------------------------ JDK17 ----
case "$ARCH" in
  x86_64|aarch64)
    # openjdk17 apk closure lands under usr/lib/jvm/java-17-openjdk; gather jdk+jmods+jre-headless+jre.
    T=/tmp/jdk17-apk-$ARCH; rm -rf "$T"; mkdir -p "$T"
    for a in openjdk17-jdk openjdk17-jmods openjdk17-jre-headless openjdk17-jre; do
      apk="$(ls $OJ17/$ARCH/${a}-*.apk 2>/dev/null | head -1)"
      [ -n "$apk" ] && tar xzf "$apk" -C "$T" 2>/dev/null
    done
    mkdir -p "$STAGE/opt/jdk17"
    cp -a "$T/usr/lib/jvm/java-17-openjdk/." "$STAGE/opt/jdk17/" ;;
  loongarch64)
    T=/tmp/jdk17-apk-$ARCH; rm -rf "$T"; mkdir -p "$T"
    for a in openjdk17-loongarch-jdk openjdk17-loongarch-jmods openjdk17-loongarch-jre-headless openjdk17-loongarch-jre; do
      apk="$(ls $OJ17/$ARCH/${a}-*.apk 2>/dev/null | head -1)"
      [ -n "$apk" ] && tar xzf "$apk" -C "$T" 2>/dev/null
    done
    mkdir -p "$STAGE/opt/jdk17"
    cp -a "$T/usr/lib/jvm/"*"/." "$STAGE/opt/jdk17/" ;;
  riscv64)
    # native-musl cross build, top dir = rvjdk-stage
    untar_into "$OJ17/riscv64/openjdk17-riscv64-musl-NATIVE-cross.tar.gz" "opt/jdk17" 1 ;;
esac
echo "  jdk17 staged: $(ls $STAGE/opt/jdk17/bin/java 2>/dev/null || echo MISSING)"

# ------------------------------------------------------------------ JDK21 ----
case "$ARCH" in
  x86_64)      untar_into "$JM/packages/jdk21/bellsoft-jdk21.0.11+11-linux-x64-musl.tar.gz"     "opt/jdk21" 1 ;;
  aarch64)     untar_into "$JM/packages/jdk21/bellsoft-jdk21.0.11+11-linux-aarch64-musl.tar.gz" "opt/jdk21" 1 ;;
  riscv64)     untar_into "$JM/packages/jdk21/bellsoft-jdk21.0.11+11-linux-riscv64.tar.gz"      "opt/jdk21" 1 ;;  # glibc
  loongarch64) untar_into "$JM/packages/jdk21/loongson21.10.25-fx-jdk21.0.10_7-linux-loongarch64.tar.gz" "opt/jdk21" 1 ;;  # glibc
esac
echo "  jdk21 staged: $(ls $STAGE/opt/jdk21/bin/java 2>/dev/null || echo MISSING)"

# ------------------------------------------------------------------ JDK23 ----
case "$ARCH" in
  x86_64)      untar_into "$JM/packages/jdk23/bellsoft-jdk23.0.2+9-linux-x64-musl.tar.gz"     "opt/jdk23" 1 ;;
  aarch64)     untar_into "$JM/packages/jdk23/bellsoft-jdk23.0.2+9-linux-aarch64-musl.tar.gz" "opt/jdk23" 1 ;;
  riscv64)     untar_into "$JM/packages/jdk23/bellsoft-jdk23.0.2+9-linux-riscv64.tar.gz"      "opt/jdk23" 1 ;;  # glibc
  loongarch64) untar_into "$JM/packages/jdk23/loongson23.1.17-fx-jdk23_37-linux-loongarch64.tar.gz" "opt/jdk23" 1 ;;  # glibc
esac
echo "  jdk23 staged: $(ls $STAGE/opt/jdk23/bin/java 2>/dev/null || echo MISSING)"

# ------------------------------------------------------------------ JDK25 ----
case "$ARCH" in
  x86_64)      untar_into "$JM/packages/jdk25/bellsoft-jdk25+37-linux-x64-musl.tar.gz"     "opt/jdk25" 1 ;;
  aarch64)     untar_into "$JM/packages/jdk25/bellsoft-jdk25+37-linux-aarch64-musl.tar.gz" "opt/jdk25" 1 ;;
  riscv64)     untar_into "$JM/packages/jdk25/bellsoft-jdk25+37-linux-riscv64.tar.gz"      "opt/jdk25" 1 ;;  # glibc
  loongarch64)
    # Alpine edge native-musl openjdk25 (C2 JIT port). Merge jdk + jre-headless + jre + jmods;
    # all land under usr/lib/jvm/java-25-openjdk/ -> flatten into opt/jdk25.
    T=/tmp/jdk25-loong; rm -rf "$T"; mkdir -p "$T"
    for a in openjdk25-loongarch-jre-headless openjdk25-loongarch-jre openjdk25-loongarch-jdk openjdk25-loongarch-jmods; do
      apk="$(ls $JM/packages/jdk25/loongarch64-alpine-musl/${a}-*.apk 2>/dev/null | head -1)"
      [ -n "$apk" ] && tar xzf "$apk" -C "$T" 2>/dev/null
    done
    mkdir -p "$STAGE/opt/jdk25"
    cp -a "$T/usr/lib/jvm/java-25-openjdk/." "$STAGE/opt/jdk25/" ;;
esac
echo "  jdk25 staged: $(ls $STAGE/opt/jdk25/bin/java 2>/dev/null || echo MISSING)"

# riscv64 / loongarch64 21/23 (and loong nothing-extra) are glibc -> stage gcompat so musl loader
# can satisfy their libc.so.6 / ld-linux references via the gcompat shim (same approach as ros2).
case "$ARCH" in
  riscv64|loongarch64)
    GC="$(ls $OJ17/$ARCH/gcompat-*.apk 2>/dev/null | head -1)"
    [ -n "$GC" ] && { apk_into "$GC" ""; echo "  + gcompat shim staged (glibc JDKs)"; } ;;
esac

# ------------------------------------------------------------- test sources --
# version feature tests (Jdk{17,21,23,25}Features.java + JavaGrammar.java full-JLS
# grammar carpet) + the CLI carpet shell driver (java-cli-core.sh) the qemu toml
# invokes at /root/jdkm/java-cli-core.sh. BOTH .java and .sh must land on-target.
mkdir -p "$STAGE/root/jdkm"
cp "$JM/programs/"*.java "$STAGE/root/jdkm/"
cp "$JM/programs/"*.sh   "$STAGE/root/jdkm/"
echo "  test sources: $(ls $STAGE/root/jdkm/*.java | wc -l) .java + $(ls $STAGE/root/jdkm/*.sh 2>/dev/null | wc -l) .sh"

# ------------------------------------------------------------- sdkman (opt) --
# Stage sdkman's offline zip + a pre-seeded candidates/java layout so the case
# can demonstrate `sdk use java <ver>` candidate-dir switching offline (needs
# bash, also staged). sdk INSTALL needs network; sdk USE on pre-seeded
# candidates is offline. If bash apk for this arch is present, stage it.
SDK="$HERE/../../java-tail/sdkman/package"   # shared with the sdkman app (dedup)
if [ -d "$SDK/apks/$ARCH" ]; then
  for apk in "$SDK/apks/$ARCH"/*.apk; do apk_into "$apk" ""; done
  echo "  + sdkman support apks (bash/curl/unzip/zip) staged"
fi
# Pre-seed an sdkman candidates layout pointing at the 4 staged JDKs (symlinks).
mkdir -p "$STAGE/root/.sdkman/candidates/java"
for V in 17 21 23 25; do
  ln -sfn /opt/jdk$V "$STAGE/root/.sdkman/candidates/java/$V-open"
done
ln -sfn /opt/jdk17 "$STAGE/root/.sdkman/candidates/java/current"
[ -f "$SDK/sdkman-5.23.0-linuxx64.zip" ] && cp "$SDK/sdkman-5.23.0-linuxx64.zip" "$STAGE/root/.sdkman/sdkman.zip"

# ------------------------------------------------------- update-alternatives -
# Seed the "current" symlink to JDK17; the case retargets it per switch step.
ln -sfn /opt/jdk17 "$STAGE/opt/jdk-current"

# musl loader search path: base + EVERY JDK's lib + lib/server (so any selected
# JDK resolves libjvm/libjava/etc.). Matches openjdk17-0's ld-musl.path pattern.
{
  printf '/lib\n/usr/lib\n'
  for V in 17 21 23 25; do printf '/opt/jdk%s/lib\n/opt/jdk%s/lib/server\n' "$V" "$V"; done
} > "$STAGE/etc-ld-musl-$ARCH.path"

# --- copy base, grow to 6G (4 full JDKs ~= 1.5-2G compressed-on-disk) ---------
echo "=== [$ARCH] copy base -> $IMG (resize 6G) ==="
cp -f "$BASE" "$IMG"
truncate -s 6G "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1
resize2fs "$IMG" >/dev/null 2>&1

# --- write the staged tree into the UNMOUNTED ext4 via debugfs (no mount/sync) -
echo "=== [$ARCH] inject tree into $IMG via debugfs -w (no mount, no sync) ==="
: > "$DBG"
( cd "$STAGE" && find . -type d | sort ) | while read -r d; do
  rel="${d#./}"; [ "$rel" = "." ] && continue
  echo "mkdir /$rel" >> "$DBG"
done
( cd "$STAGE" && find . -type f | sort ) | while read -r f; do
  rel="${f#./}"
  echo "rm /$rel" >> "$DBG"
  echo "write $STAGE/$rel /$rel" >> "$DBG"
done
( cd "$STAGE" && find . -type l | sort ) | while read -r l; do
  rel="${l#./}"; tgt=$(readlink "$STAGE/$rel")
  echo "rm /$rel" >> "$DBG"
  echo "symlink /$rel $tgt" >> "$DBG"
done
echo "rm /etc/ld-musl-$ARCH.path" >> "$DBG"
echo "write $STAGE/etc-ld-musl-$ARCH.path /etc/ld-musl-$ARCH.path" >> "$DBG"

debugfs -w -f "$DBG" "$IMG" >/tmp/jdkmulti-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/jdkmulti-debugfs-$ARCH.log \
  | grep -viE 'File not found.*rm|rm:.*not found|Allocating group tables' | head

# --- verify the 4 java binaries landed -----------------------------------------
echo "=== [$ARCH] verify /opt/jdk{17,21,23,25}/bin/java in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
for V in 17 21 23 25; do
  if debugfs -R "stat /opt/jdk$V/bin/java" "$IMG" 2>/dev/null | grep -q Inode; then
    echo "  jdk$V/bin/java OK"
  else
    echo "  jdk$V/bin/java MISSING !!"
  fi
done
debugfs -R "stat /opt/jdk-current" "$IMG" 2>/dev/null | head -1
echo "[$ARCH] DONE -> $IMG"
