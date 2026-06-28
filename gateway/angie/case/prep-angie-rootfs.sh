#!/bin/bash
# prep-angie-rootfs.sh — build rootfs-<arch>-angie.img for the `angie-0` StarryOS
# stress case (#764 "gateway" sub-task: angie = an nginx-compatible HTTP server fork).
#
# Starts from the Alpine musl base image (rootfs-<arch>-alpine.img, Alpine v3.23.4 —
# the SAME branch the angie apk + its deps are fetched from, so the musl/openssl/pcre/zlib
# ABI matches byte-for-byte), then extracts the angie apk + its full dependency closure
# (pcre2 / zlib / libssl3 / libcrypto3 / openssl; musl + busybox already in the base) and
# writes a MINIMAL FOREGROUND /etc/angie/angie.conf that, exactly like the nginx case:
#   - daemon off;            -> angie stays in the foreground (no double-fork to background)
#   - master_process off;    -> SINGLE process, no worker fork (smallest syscall surface)
#   - error_log stderr info; -> diagnostics go to the serial console, not a file
#   - user root;             -> single foreground proc as root; getpwnam("root") always
#                               resolves (the apk angie's compile default --user=angie would
#                               otherwise [emerg] when no "angie" account exists in the rootfs)
#   - listen 127.0.0.1:8080 ; location / { return 200 "ANGIE_OK_BODY"; }  (in-config body,
#     no static index-file open, no sendfile path)
#
# angie 1.11.5-r0 (from download.angie.software/angie/alpine/v3.23/main) + deps from the
# Alpine v3.23/main CDN. Binary layout: /usr/sbin/angie -> symlink -> /usr/sbin/angie-nodebug
# (PIE musl ELF, NEEDED libpcre2-8 / libssl / libcrypto / libz / libc.musl). The symlink is
# recreated in the image. See <download-cache>/gateway-bins/SOURCES.md.
#
# ONLY x86_64 + aarch64: angie's official Alpine repo ships only those two arches; riscv64
# and loongarch64 have NO angie apk (APKINDEX 404) -> those arches are intentionally absent.
#
# Usage:   bash prep-angie-rootfs.sh <arch>     # arch in x86_64|aarch64
#
# WSL2 NOTE: under WSL2 a bare global `sync` can D-state-deadlock the host, and a
# loop-`mount` write can wedge too. This script uses `debugfs -w` (write to the UNMOUNTED
# ext4 image directly) so it NEVER mounts and NEVER calls sync -> no deadlock. It only
# touches the angie image it creates.
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs under tmp/axbuild/rootfs/); apks/payloads ship alongside this script under ./apks/.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE/apks"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-angie.img
APKDIR=$DL/$ARCH
STAGE=/tmp/angie-stage-$ARCH

# Source of the angie payload differs per arch:
#  - x86_64 / aarch64 : angie's official Alpine (musl) apk repo ships these -> we extract
#    the angie apk + its full dependency-closure apks (pcre2/zlib/openssl/...).  UNCHANGED.
#  - riscv64 / loongarch64 : angie's official Alpine repo has NO apk for these (APKINDEX 404),
#    so the binary is SOURCE-CROSS-BUILT with the musl cross toolchain (see SOURCES.md) and
#    delivered as a raw payload tree at $APKDIR/payload/  (usr/sbin/angie[-nodebug] + the
#    libpcre2-8 .so closure).  The source build drops the proxy/fastcgi/uwsgi/scgi/gzip
#    modules, so the foreground angie.conf for these arches must NOT reference their
#    *_temp_path directives (it would be an "unknown directive" and `angie -t` would fail).
SRCBUILD=no
case "$ARCH" in
  x86_64|aarch64) ;;
  riscv64|loongarch64) SRCBUILD=yes ;;
  *) echo "angie: unsupported arch '$ARCH' (have x86_64|aarch64 apk, riscv64|loongarch64 source-built)"; exit 3 ;;
esac

[ -f "$BASE" ] || { echo "missing base $BASE"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }
if [ "$SRCBUILD" = yes ]; then
  PAYDIR=$APKDIR/payload
  [ -d "$PAYDIR" ] || { echo "missing source-built payload $PAYDIR (cross-build angie first, see SOURCES.md)"; exit 2; }
  [ -x "$PAYDIR/usr/sbin/angie-nodebug" ] || { echo "missing $PAYDIR/usr/sbin/angie-nodebug"; exit 2; }
  echo "=== [$ARCH] using SOURCE-CROSS-BUILT payload from $PAYDIR ==="
else
  [ -d "$APKDIR" ] || { echo "missing apk dir $APKDIR"; exit 2; }
  N=$(ls "$APKDIR"/*.apk 2>/dev/null | wc -l)
  [ "$N" -ge 5 ] || { echo "closure too small ($N apks) in $APKDIR"; exit 2; }
  echo "=== [$ARCH] using $N apks from $APKDIR ==="
fi

# --- 1. copy base image, grow to 1280M (angie is tiny) ------------------------
echo "=== [$ARCH] copy base -> $IMG (resize 1280M) ==="
cp -f "$BASE" "$IMG"
truncate -s 1280M "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1
resize2fs "$IMG" >/dev/null 2>&1

# --- 2. stage: build one install tree -----------------------------------------
echo "=== [$ARCH] stage payload into $STAGE ==="
rm -rf "$STAGE"; mkdir -p "$STAGE"
shopt -s nullglob
if [ "$SRCBUILD" = yes ]; then
  # raw source-built payload tree (usr/sbin/angie[-nodebug] + libpcre2-8 .so closure);
  # musl libc is already in the Alpine base image. cp -a preserves the angie symlink.
  cp -a "$PAYDIR"/. "$STAGE"/
  echo "  + source-built payload ($(basename "$PAYDIR"))"
else
  for apk in "$APKDIR"/*.apk; do
    tar xzf "$apk" -C "$STAGE" \
      --exclude='.PKGINFO' --exclude='.SIGN.*' --exclude='.pre-install' \
      --exclude='.post-install' --exclude='.pre-upgrade' --exclude='.post-upgrade' \
      --exclude='.trigger' 2>/dev/null && echo "  + $(basename "$apk")" || echo "  ! $(basename "$apk") (partial)"
  done
fi

# --- 3. write the minimal FOREGROUND angie.conf into the stage tree -----------
# (overwrites the apk's default /etc/angie/angie.conf). Runtime paths under /tmp and /var
# so a fresh boot can create/use them; the DoD script mkdir's them before start.
mkdir -p "$STAGE/etc/angie"
cat > "$STAGE/etc/angie/angie.conf" << 'ANGIECONF'
# Minimal FOREGROUND angie config for the StarryOS angie-0 gateway stress test.
# The apk-packaged angie is compiled with a default --user=angie, so even with NO
# explicit `user` line angie resolves getpwnam("angie") at config-test/startup and
# [emerg] aborts when that account is absent from the rootfs /etc/passwd. Pin it to
# `root` (always present, uid 0) so the single foreground process needs no extra
# account — arch-independent (also overrides the source-built default for rv/loong).
user root;
daemon off;
master_process off;
worker_processes 1;
pid /run/angie.pid;
error_log stderr info;

events {
    worker_connections 64;
}

http {
    access_log off;
    default_type text/plain;

    # angie needs writable temp paths; keep them under /var/lib/angie/tmp.
    client_body_temp_path /var/lib/angie/tmp/client_body;
ANGIECONF
# The proxy/fastcgi/uwsgi/scgi modules are present in the official apk (x86_64/aarch64) but
# are dropped from the riscv64/loongarch64 SOURCE-CROSS-BUILD (minimal module set). Emitting
# their *_temp_path directives on a source build would be an "unknown directive" -> `angie -t`
# fails. So only the apk arches get those lines; the served path (return 200) is identical.
if [ "$SRCBUILD" != yes ]; then
  cat >> "$STAGE/etc/angie/angie.conf" << 'ANGIECONF_PROXY'
    proxy_temp_path       /var/lib/angie/tmp/proxy;
    fastcgi_temp_path     /var/lib/angie/tmp/fastcgi;
    uwsgi_temp_path       /var/lib/angie/tmp/uwsgi;
    scgi_temp_path        /var/lib/angie/tmp/scgi;
ANGIECONF_PROXY
fi
cat >> "$STAGE/etc/angie/angie.conf" << 'ANGIECONF_TAIL'

    server {
        listen 127.0.0.1:8080;
        server_name localhost;

        location / {
            # Fixed in-config body (no index file open). This exact string is what the
            # DoD curl/wget probe asserts on.
            add_header Content-Type text/plain;
            return 200 "ANGIE_OK_BODY";
        }
    }
}
ANGIECONF_TAIL

# musl loader search path for the guest
printf '/lib\n/usr/lib\n' > "$STAGE/etc-ld-musl-$ARCH.path"

# --- 4. write the staged tree into the UNMOUNTED ext4 via debugfs -------------
# debugfs `rm`+`write` is idempotent for files; dirs via `mkdir`; symlinks via `symlink`
# (this preserves /usr/sbin/angie -> angie-nodebug).
echo "=== [$ARCH] inject tree into $IMG via debugfs -w (no mount, no sync) ==="
DBG=/tmp/angie-debugfs-$ARCH.cmds
: > "$DBG"
# create directories depth-first
( cd "$STAGE" && find . -type d | sort ) | while read -r d; do
  rel="${d#./}"; [ "$rel" = "." ] && continue
  echo "mkdir /$rel" >> "$DBG"
done
# also ensure the runtime dirs exist in the image
for rd in run var/log/angie var/lib/angie var/lib/angie/tmp var/cache/angie; do
  echo "mkdir /$rd" >> "$DBG"
done
# write/overwrite files (rm first so re-runs replace cleanly; ignore rm errors)
( cd "$STAGE" && find . -type f | sort ) | while read -r f; do
  rel="${f#./}"
  [ "$rel" = "etc-ld-musl-$ARCH.path" ] && continue   # placed at its real path below
  echo "rm /$rel" >> "$DBG"
  echo "write $STAGE/$rel /$rel" >> "$DBG"
done
# symlinks: recreate as symlinks (preserves angie -> angie-nodebug etc.)
( cd "$STAGE" && find . -type l | sort ) | while read -r l; do
  rel="${l#./}"; tgt=$(readlink "$STAGE/$rel")
  echo "rm /$rel" >> "$DBG"
  echo "symlink /$rel $tgt" >> "$DBG"
done
# place the ld-musl path file at its real location
echo "rm /etc/ld-musl-$ARCH.path" >> "$DBG"
echo "write $STAGE/etc-ld-musl-$ARCH.path /etc/ld-musl-$ARCH.path" >> "$DBG"

debugfs -w -f "$DBG" "$IMG" >/tmp/angie-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/angie-debugfs-$ARCH.log | grep -viE 'File not found.*rm|rm:.*not found' | head

# --- 5. verify the angie binary + libs + conf landed ---------------------------
echo "=== [$ARCH] verify angie in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
debugfs -R "stat /usr/sbin/angie-nodebug" "$IMG" 2>/dev/null | head -3
echo "  --- /usr/sbin/angie symlink ---"
debugfs -R "stat /usr/sbin/angie" "$IMG" 2>/dev/null | grep -i 'Fast link dest' || echo "  (symlink dest not shown)"
echo "  --- libs ---"
# Source-built (riscv64/loongarch64) angie links ONLY libpcre2-8 + musl libc (no ssl/crypto/z,
# those modules are dropped); apk arches link the full closure. Check the right set per arch.
if [ "$SRCBUILD" = yes ]; then
  CHECK_LIBS="libpcre2-8.so.0"
else
  CHECK_LIBS="libpcre2-8.so.0 libssl.so.3 libcrypto.so.3 libz.so.1"
fi
for lib in $CHECK_LIBS; do
  debugfs -R "ls /usr/lib" "$IMG" 2>/dev/null | tr ' ' '\n' | grep -qx "$lib" && echo "  + $lib" || \
  { debugfs -R "ls /lib" "$IMG" 2>/dev/null | tr ' ' '\n' | grep -qx "$lib" && echo "  + $lib (/lib)" || echo "  ! $lib MISSING"; }
done
debugfs -R "cat /etc/angie/angie.conf" "$IMG" 2>/dev/null | grep -q ANGIE_OK_BODY && echo "  + angie.conf has ANGIE_OK_BODY" || echo "  ! angie.conf body missing"
echo "[$ARCH] DONE -> $IMG"
