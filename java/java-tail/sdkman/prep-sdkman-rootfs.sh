#!/bin/bash
# prep-sdkman-rootfs.sh — build rootfs-<arch>-sdkman.img for the `sdkman-0`
# StarryOS stress case (SDKMAN! 5.23.0 — the SDK manager for the JVM).
#
# BASE = rootfs-<arch>-java.img (4-arch musl OpenJDK17 JRE). SDKMAN is a pure
# bash CLI: `sdk` is a shell FUNCTION sourced from bin/sdkman-init.sh, which
# `source`s every src/sdkman-*.sh module. So this case adds TWO things to the
# base image:
#   (a) bash + its closure (curl/zip/unzip/readline/ncurses/...) from the 22-apk
#       sdkman closure in ../sdkman/apks/<arch>/ — the base image has only busybox
#       ash, and SDKMAN's modules use bash arrays / [[ ]] / functions, so a real
#       /bin/bash is REQUIRED (this is the distinguishing dependency of this case).
#   (b) the SDKMAN framework under /root/.sdkman/{bin,src,libexec,etc,var,...} plus
#       a pre-seeded etc/config + var/version (5.23.0) + var/platform so the
#       fully-OFFLINE surface works with no network:
#         * `sdk version`  -> reads var/version, prints "SDKMAN 5.23.0"
#         * `sdk help`     -> prints the "Usage: sdk <command>..." banner
#       sdkman-init.sh in 5.23.0 makes NO network call on source (auto-selfupdate
#       / version-check are gated behind explicit `sdk update` / `sdk selfupdate`),
#       and we additionally pin sdkman_auto_selfupdate=false + selfupdate_feature
#       =false + offline_mode=true in etc/config. So `sdk version`/`sdk help` are
#       deterministic offline. `sdk install <candidate>` DOES need the SDKMAN API
#       (api.sdkman.io) + the candidate download — documented honestly as net-only
#       in CASE-NOTES; it is NOT part of the offline gate.
#
# Usage:  bash prep-sdkman-rootfs.sh <arch>   # arch in x86_64|aarch64|riscv64|loongarch64
#
# WSL2 NOTE: uses `debugfs -w` on the UNMOUNTED image (no mount, no sync) -> no
# D-state deadlock; only touches the sdkman image it creates.
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable: ROOT from env (your tgoskits checkout); sdkman framework + apk closure
# + candidates CSV all co-located under this app's package/ dir.
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
SDKDL="$HERE/package"
FW="$SDKDL/zip-inspect/sdkman-5.23.0"        # extracted framework (src/bin/...)
APKDIR="$SDKDL/apks/$ARCH"                    # bash + curl closure (22 apks)
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-java.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-sdkman.img
STAGE=/tmp/sdkman-stage-$ARCH
SDKVER=5.23.0

[ -f "$BASE" ]        || { echo "missing base $BASE"; exit 2; }
[ -d "$FW/src" ]      || { echo "missing sdkman framework $FW"; exit 2; }
[ -d "$APKDIR" ]      || { echo "missing sdkman apk closure $APKDIR"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }
N=$(ls "$APKDIR"/*.apk 2>/dev/null | wc -l)
[ "$N" -ge 15 ] || { echo "apk closure too small ($N)"; exit 2; }

# platform token (cosmetic offline; only used for network calls we don't make)
case "$ARCH" in
  x86_64)      PLATFORM=LinuxX64 ;;
  aarch64)     PLATFORM=LinuxARM64 ;;
  riscv64)     PLATFORM=LinuxRISCV64 ;;
  loongarch64) PLATFORM=LinuxLOONGARCH64 ;;
  *)           PLATFORM=LinuxX64 ;;
esac

# --- 1. copy base -> sdkman image (resize 4G for bash closure + framework) -----
echo "=== [$ARCH] copy java base -> $IMG (resize 4G) ==="
cp -f "$BASE" "$IMG"
truncate -s 4G "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1
resize2fs "$IMG" >/dev/null 2>&1

# --- 2. stage: (a) bash apk closure overlay -----------------------------------
echo "=== [$ARCH] stage bash closure + sdkman framework -> $STAGE ==="
rm -rf "$STAGE"; mkdir -p "$STAGE"
shopt -s nullglob
for apk in "$APKDIR"/*.apk; do
  tar xzf "$apk" -C "$STAGE" \
    --exclude='.PKGINFO' --exclude='.SIGN.*' --exclude='.pre-install' \
    --exclude='.post-install' --exclude='.pre-upgrade' --exclude='.post-upgrade' \
    --exclude='.trigger' 2>/dev/null && echo "  + $(basename "$apk")" || echo "  ! $(basename "$apk") (partial)"
done

# --- 2b. stage the SDKMAN framework + offline config into .sdkman/ -------------
SDK=$STAGE/root/.sdkman
mkdir -p "$SDK/bin" "$SDK/src" "$SDK/libexec" "$SDK/etc" "$SDK/var" \
         "$SDK/tmp" "$SDK/ext" "$SDK/candidates"
cp "$FW/bin/sdkman-init.sh" "$SDK/bin/"
cp "$FW"/src/sdkman-*.sh    "$SDK/src/"
# pre-seed the offline state files. var/candidates is the CSV cache that
# sdkman-init.sh reads at source time (line 106); without it every `sdk` call
# prints a "var/candidates: No such file" warning. We seed it from the captured
# candidates-all.csv (84 JVM candidate names) so the framework loads cleanly.
echo "$SDKVER"   > "$SDK/var/version"
echo "$PLATFORM" > "$SDK/var/platform"
cp "$SDKDL/candidates-all.csv" "$SDK/var/candidates"
cat > "$SDK/etc/config" << 'CFG'
sdkman_auto_answer=false
sdkman_auto_complete=false
sdkman_auto_env=false
sdkman_auto_selfupdate=false
sdkman_selfupdate_feature=false
sdkman_colour_enable=false
sdkman_curl_connect_timeout=7
sdkman_curl_max_time=10
sdkman_debug_mode=false
sdkman_insecure_ssl=false
sdkman_offline_mode=true
CFG

# --- 3. inject the staged tree via debugfs -w (no mount, no sync) -------------
echo "=== [$ARCH] inject bash + .sdkman into $IMG via debugfs -w ==="
DBG=/tmp/sdkman-debugfs-$ARCH.cmds
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
debugfs -w -f "$DBG" "$IMG" >/tmp/sdkman-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/sdkman-debugfs-$ARCH.log | grep -viE 'File exists.*mkdir|File not found.*rm|rm:.*not found' | head

# --- 4. verify bash + sdkman framework landed ---------------------------------
echo "=== [$ARCH] verify bash + sdkman in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
debugfs -R "stat /bin/bash" "$IMG" 2>/dev/null | grep -iE 'Inode|Size|Type' | head -2
debugfs -R "stat /root/.sdkman/bin/sdkman-init.sh" "$IMG" 2>/dev/null | grep -iE 'Inode|Size' | head -2
debugfs -R "cat /root/.sdkman/var/version" "$IMG" 2>/dev/null
echo "[$ARCH] DONE -> $IMG"
