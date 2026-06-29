#!/usr/bin/env bash
# prebuild.sh — provision the node-lib library carpet for StarryOS.
#
# A set of common Node.js libraries — less / stylus / scss(sass) (CSS preprocessors),
# @babel/core (+ TS/JSX presets) / terser / eslint (JS transform / minify / lint) and
# CommonJS<->ESM interop — each run on-target by Node.js v22.22.2 (full V8 JIT, no kernel
# change) with an anchored self-check.
#
# It stages the musl-native Node.js v22.22.2 apk closure into the per-app rootfs /usr, plus the
# arch-independent carpet sources + the library node_modules closure, then the harness injects
# the overlay and qemu-<arch>.toml runs /usr/bin/run-nlib.sh.
#
# Per-arch Node.js source: the Alpine v3.22 nodejs-22.22.2-r0 apk closure (node + icu + libstdc++
# + libgcc + openssl + c-ares + nghttp2 + simdjson/simdutf + ada + brotli + zstd + zlib + musl),
# identical layout across x86_64 / aarch64 / riscv64 / loongarch64.
#
# ROOTFS SIZE: the harness copies the ~1 GiB base alpine rootfs to a per-app image, runs THIS
# prebuild, then injects the overlay via debugfs WITHOUT resizing — large files get silently
# truncated if the fs is full. The node closure (~110 MiB) + node_modules (~14 MiB) fit after
# we grow the image to 2 GiB.
#
# Env from the app runner: STARRY_ARCH, STARRY_OVERLAY_DIR, STARRY_APP_DIR, STARRY_ROOTFS,
# STARRY_STAGING_ROOT.
set -euo pipefail

app_dir="${STARRY_APP_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)}"
arch="${STARRY_ARCH:?prebuild: STARRY_ARCH required}"
overlay_dir="${STARRY_OVERLAY_DIR:?prebuild: STARRY_OVERLAY_DIR required}"
rootfs_img="${STARRY_ROOTFS:?prebuild: STARRY_ROOTFS required}"

ASSETS="$app_dir/assets"
PROG="$app_dir/programs"

# ── Node apk closure cache (PORTABLE) ─────────────────────────────────────────────────
# Holds the per-arch Alpine node apk closure under nodejs-apks/<arch>/. A developer who already
# has it points NODE_DL_ROOT at their cache; otherwise each apk is fetched from the Alpine pool.
DL="${NODE_DL_ROOT:-${STARRY_STAGING_ROOT:-$app_dir}/.cache/node-dl}"
ALPINE_CDN="${ALPINE_CDN:-https://dl-cdn.alpinelinux.org/alpine}"
ROOTFS_SIZE="${NWEB_ROOTFS_SIZE:-2048M}"

ensure_host_tools() {
    local missing=()
    command -v tar       >/dev/null 2>&1 || missing+=(tar)
    command -v resize2fs >/dev/null 2>&1 || missing+=(e2fsprogs)
    command -v e2fsck    >/dev/null 2>&1 || missing+=(e2fsprogs)
    if [[ ${#missing[@]} -gt 0 ]]; then
        if command -v apt-get >/dev/null 2>&1; then
            apt-get update && apt-get install -y --no-install-recommends "${missing[@]}"
        else
            echo "prebuild: missing host tools and no apt-get: ${missing[*]}" >&2; exit 1
        fi
    fi
}

# Grow the per-app rootfs so the injected node closure fits without truncation. The alpine base
# img is shared across apps: grow the FILE only when it is smaller than target (never shrink — a
# larger fs left by another app must not be truncated), but ALWAYS run e2fsck + resize2fs so the
# filesystem fills the file (resize2fs is a no-op if it already matches; this is what guarantees
# the debugfs overlay injection has room — checking the file size alone is not enough, the fs
# inside may still be the smaller base size).
grow_rootfs() {
    [[ -f "$rootfs_img" ]] || { echo "prebuild: rootfs image missing: $rootfs_img" >&2; exit 2; }
    local cur target
    cur=$(stat -c %s "$rootfs_img")
    target=$(( ${ROOTFS_SIZE%M} * 1024 * 1024 ))
    [[ "$cur" -lt "$target" ]] && truncate -s "$ROOTFS_SIZE" "$rootfs_img"
    e2fsck -f -y "$rootfs_img" >/dev/null 2>&1 || true
    resize2fs "$rootfs_img" >/dev/null 2>&1 || { echo "prebuild: resize2fs failed on $rootfs_img" >&2; exit 2; }
    echo "prebuild: rootfs fs sized to $(( $(stat -c %s "$rootfs_img")/1024/1024 )) MiB file"
}

# Ensure the node apk closure for this arch is present (cache-first; Alpine pool best-effort).
ensure_node_apks() {
    local d="$DL/nodejs-apks/$arch"
    [[ -d "$d" && -n "$(ls "$d"/nodejs-22*.apk 2>/dev/null)" ]] && { echo "prebuild: node apk closure cache hit ($d)"; return 0; }
    echo "prebuild: node apk closure not cached at $d" >&2
    echo "prebuild: place the Alpine v3.22 nodejs-22.22.2-r0 apk closure (see SOURCES) under $d" >&2
    exit 4
}

# Extract the node apk closure into $overlay_dir/usr (+ /lib /etc as the apks carry), skipping
# apk metadata members. node lands at /usr/bin/node, shared libs at /usr/lib.
stage_node() {
    local d="$DL/nodejs-apks/$arch" apk name
    install -d "$overlay_dir/usr/bin" "$overlay_dir/etc"
    for apk in "$d"/*.apk; do
        [[ -f "$apk" ]] || continue
        name="$(basename "$apk")"
        tar xzf "$apk" -C "$overlay_dir" \
            --exclude='.PKGINFO' --exclude='.SIGN.*' --exclude='.pre-install' \
            --exclude='.post-install' --exclude='.pre-upgrade' --exclude='.post-upgrade' \
            --exclude='.trigger' 2>/dev/null || echo "prebuild: $name (partial)"
    done
    printf '/lib\n/usr/lib\n' > "$overlay_dir/etc/ld-musl-$arch.path"
    [[ -x "$overlay_dir/usr/bin/node" ]] || { echo "prebuild: node staged without /usr/bin/node for $arch" >&2; exit 3; }
    echo "prebuild: node staged ($(du -sh "$overlay_dir/usr" | cut -f1)); node apk = $(basename "$(ls "$d"/nodejs-22*.apk | head -1)")"
}

# Stage the carpet sources + the less/stylus/scss/babel/terser/eslint node_modules closure into
# /root/nlib and the run-nlib.sh gate into /usr/bin. Each carpet resolves require() from
# /root/nlib/node_modules (run-nlib.sh cd's there) and writes its own scratch under __dirname.
stage_payload() {
    local nw="$overlay_dir/root/nlib"
    install -d "$nw/carpets"
    install -m0644 "$PROG/carpets/LessCarpet.js"   "$nw/carpets/"
    install -m0644 "$PROG/carpets/StylusCarpet.js" "$nw/carpets/"
    install -m0644 "$PROG/carpets/ScssCarpet.js"   "$nw/carpets/"
    install -m0644 "$PROG/carpets/BabelCarpet.js"  "$nw/carpets/"
    install -m0644 "$PROG/carpets/TerserCarpet.js" "$nw/carpets/"
    install -m0644 "$PROG/carpets/EslintCarpet.js" "$nw/carpets/"
    install -m0644 "$PROG/carpets/CjsEsmCarpet.js" "$nw/carpets/"
    cp -a "$ASSETS/node_modules" "$nw/node_modules"
    install -Dm0755 "$PROG/run-nlib.sh" "$overlay_dir/usr/bin/run-nlib.sh"
    echo "prebuild: staged carpets + node_modules ($(du -sh "$nw/node_modules" | cut -f1)) into /root/nlib, run-nlib.sh into /usr/bin"
}

main() {
    case "$arch" in x86_64|aarch64|riscv64|loongarch64) ;; *) echo "prebuild: unsupported arch: $arch" >&2; exit 1 ;; esac
    ensure_host_tools
    ensure_node_apks
    grow_rootfs
    stage_node
    stage_payload
    echo "prebuild: node-lib overlay ready for $arch — $(du -sh "$overlay_dir" | cut -f1)"
}

main "$@"
