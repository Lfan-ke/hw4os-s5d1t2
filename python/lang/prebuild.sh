#!/usr/bin/env bash
# prebuild.sh — build a CPython 3.14 rootfs for the python-lang carpet suite and
# stage the test modules. Self-contained + relocatable: the Alpine-edge
# pure-interpreter CPython 3.14 closure is bundled in ./apks/<arch>; the Alpine
# base image + output image are provided by the app runner via env. No absolute
# host paths.
#
# Env (set by `cargo xtask starry app qemu`): STARRY_ARCH, STARRY_ROOTFS (working
# copy seeded from the registered Alpine base), STARRY_OVERLAY_DIR. A configured
# prebuild makes the runner skip the managed-image ensure, so installing the 3.14
# closure over the working copy is safe.
#
# NOTE (loongarch64): the guest must run a StarryOS kernel that detects real RAM
# from the FDT (honors qemu `-m`) — rcore-os/tgoskits #239, see SOURCES.md — and
# qemu-loongarch64.toml sets `-m 2048M`; else child-interpreter spawning
# (multiprocessing / `python -i` / venv) OOMs. aarch64/riscv64/x86_64 use ~512M.
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOTFS="${STARRY_ROOTFS:?prebuild: STARRY_ROOTFS required}"
ARCH="${STARRY_ARCH:?prebuild: STARRY_ARCH required}"
OVERLAY="${STARRY_OVERLAY_DIR:?prebuild: STARRY_OVERLAY_DIR required}"
APKDIR="$HERE/apks/$ARCH"
command -v debugfs >/dev/null 2>&1 || { echo "prebuild: need e2fsprogs (debugfs)" >&2; exit 1; }

have_py314() { debugfs -R "ls /usr/lib/python3.14" "$ROOTFS" 2>/dev/null | grep -q '[A-Za-z]'; }

if ! have_py314; then
    echo "prebuild: installing CPython 3.14 closure into $ROOTFS ($ARCH)"
    ls "$APKDIR"/*.apk >/dev/null 2>&1 || { echo "prebuild: missing closure $APKDIR" >&2; exit 2; }
    truncate -s 2G "$ROOTFS" 2>/dev/null || true
    e2fsck -fy "$ROOTFS" >/dev/null 2>&1 || true
    resize2fs "$ROOTFS" >/dev/null 2>&1 || true
    STAGE="$(mktemp -d)"; DBG="$(mktemp)"
    for apk in "$APKDIR"/*.apk; do
        tar xzf "$apk" -C "$STAGE" \
            --exclude='.PKGINFO' --exclude='.SIGN.*' --exclude='.pre-install' \
            --exclude='.post-install' --exclude='.pre-upgrade' --exclude='.post-upgrade' \
            --exclude='.trigger' 2>/dev/null || true
    done
    printf '/lib\n/usr/lib\n' > "$STAGE/etc-ld-musl-$ARCH.path"
    ( cd "$STAGE" && find . -type d | sort ) | while read -r d; do
        rel="${d#./}"; [ "$rel" = "." ] || echo "mkdir /$rel" >> "$DBG"; done
    ( cd "$STAGE" && find . -type f | sort ) | while read -r f; do
        rel="${f#./}"; echo "rm /$rel" >> "$DBG"; echo "write $STAGE/$rel /$rel" >> "$DBG"; done
    ( cd "$STAGE" && find . -type l | sort ) | while read -r l; do
        rel="${l#./}"; tgt="$(readlink "$STAGE/$rel")"; echo "rm /$rel" >> "$DBG"; echo "symlink /$rel $tgt" >> "$DBG"; done
    echo "rm /etc/ld-musl-$ARCH.path" >> "$DBG"
    echo "write $STAGE/etc-ld-musl-$ARCH.path /etc/ld-musl-$ARCH.path" >> "$DBG"
    debugfs -w -f "$DBG" "$ROOTFS" >/dev/null 2>&1
    rm -rf "$STAGE" "$DBG"
    have_py314 || { echo "prebuild: 3.14 closure injection failed (no /usr/lib/python3.14)" >&2; exit 2; }
fi

mkdir -p "$OVERLAY/usr/bin"
n=0
for f in "$HERE"/python/*.py; do
    [ -f "$f" ] || continue
    cp -f "$f" "$OVERLAY/usr/bin/"; n=$((n + 1))
done
echo "prebuild: python3.14 ready + staged $n test module(s) into overlay for $ARCH"
