#!/usr/bin/env bash
# prebuild.sh — provision the J2SE library + JSE standard-library carpet for StarryOS.
#
# This is the J2SE case for #764 "JSE classic tools / common libraries": a set of real
# third-party J2SE libraries (jackson / guava / commons-lang3 / h2 / slf4j / logback /
# sqlite-jdbc + native JNI / lombok) and a 15-module JSE standard-library suite, each run
# on-target by OpenJDK 17 with an anchored self-check marker.
#
# It stages ONE full JDK17 (the openjdk-multi JDK17 cell, proven 4-arch green) plus the
# arch-independent demo jars + the compiled jse-suite jar into the per-app rootfs, then the
# harness injects the overlay and qemu-<arch>.toml runs /usr/bin/run-jse.sh.
#
# Per-arch JDK17 source (== openjdk-multi, the proven recipe):
#   x86_64 / aarch64 : Alpine openjdk17 apks (full JDK, musl native)
#   loongarch64      : Alpine openjdk17-loongarch apks (musl native)
#   riscv64          : native-musl cross-built JDK17 tarball
#
# ROOTFS SIZE: the harness copies the ~1 GiB base alpine rootfs to a per-app image, runs THIS
# prebuild, then injects the overlay via debugfs WITHOUT resizing — large files get silently
# truncated if the fs is full. One JDK17 (~330 MiB) + the demo jars (~24 MiB) fit after we
# grow the image to 2.5 GiB (truncate + e2fsck + resize2fs). The running JVM only maps a
# -Xmx256m heap, so the larger image is free.
#
# Env from the app runner: STARRY_ARCH, STARRY_OVERLAY_DIR, STARRY_APP_DIR, STARRY_ROOTFS,
# STARRY_STAGING_ROOT.
set -euo pipefail

app_dir="${STARRY_APP_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)}"
arch="${STARRY_ARCH:?prebuild: STARRY_ARCH required}"
overlay_dir="${STARRY_OVERLAY_DIR:?prebuild: STARRY_OVERLAY_DIR required}"
rootfs_img="${STARRY_ROOTFS:?prebuild: STARRY_ROOTFS required}"

PROG="$app_dir/programs"
ASSETS="$app_dir/assets"

# ── Asset cache root (PORTABLE) ───────────────────────────────────────────────────────
# Holds the JDK17 distributions (same tree layout openjdk-multi prep uses). A developer who
# already has the assets points JAVA_DL_ROOT at their cache; otherwise each JDK apk is
# fetched from its Alpine pool URL into here and re-used. The riscv64 native-musl cross JDK
# is not downloadable (built once from source); it must be present in the cache.
DL="${JAVA_DL_ROOT:-${STARRY_STAGING_ROOT:-$app_dir}/.cache/java-dl}"
ALPINE_CDN="${ALPINE_CDN:-https://dl-cdn.alpinelinux.org/alpine}"
ROOTFS_SIZE="${JSE_ROOTFS_SIZE:-2560M}"

# ── Portable fetch-ensure layer ───────────────────────────────────────────────────────
ensure_asset() {
    local dest="$1" url="$2"
    [[ -f "$dest" ]] && { echo "prebuild: cache hit $dest"; return 0; }
    command -v curl >/dev/null 2>&1 || { echo "prebuild: need curl to fetch $url" >&2; exit 4; }
    [[ -n "$url" ]] || { echo "prebuild: no cached $dest and no URL" >&2; exit 4; }
    echo "prebuild: fetching $(basename "$dest") <- $url"
    mkdir -p "$(dirname "$dest")"
    curl -fSL --retry 3 --connect-timeout 20 "$url" -o "$dest.tmp"
    mv -f "$dest.tmp" "$dest"
}

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

# Grow the per-app rootfs so the injected JDK fits without truncation. Idempotent.
grow_rootfs() {
    [[ -f "$rootfs_img" ]] || { echo "prebuild: rootfs image missing: $rootfs_img" >&2; exit 2; }
    truncate -s "$ROOTFS_SIZE" "$rootfs_img"
    e2fsck -f -y "$rootfs_img" >/dev/null 2>&1 || true
    resize2fs "$rootfs_img" >/dev/null 2>&1
    echo "prebuild: rootfs grown to $(( $(stat -c %s "$rootfs_img")/1024/1024 )) MiB"
}

untar_strip1() {
    local arc="$1" dest="$2"
    [[ -f "$arc" ]] || { echo "prebuild: missing archive $arc" >&2; exit 2; }
    mkdir -p "$dest"; tar xzf "$arc" -C "$dest" --strip-components=1
}

# Ensure the per-arch JDK17 apks/tarball are present (cache-first; Alpine pool best-effort).
ensure_jdk17() {
    local d="$DL/openjdk17-apks/$arch"
    case "$arch" in
        x86_64|aarch64)
            local alp; [[ "$arch" == x86_64 ]] && alp=x86_64 || alp=aarch64
            local a v=17.0.18_p8-r0
            for a in openjdk17-jdk openjdk17-jmods openjdk17-jre-headless openjdk17-jre; do
                [[ -n "$(ls "$d/${a}-"*.apk 2>/dev/null | head -1)" ]] && continue
                ensure_asset "$d/${a}-${v}.apk" "$ALPINE_CDN/v3.22/community/$alp/${a}-${v}.apk"
            done ;;
        loongarch64)
            local a v=17.0.17_p10-r0
            for a in openjdk17-loongarch-jdk openjdk17-loongarch-jmods openjdk17-loongarch-jre-headless openjdk17-loongarch-jre; do
                [[ -n "$(ls "$d/${a}-"*.apk 2>/dev/null | head -1)" ]] && continue
                ensure_asset "$d/${a}-${v}.apk" "$ALPINE_CDN/edge/community/loongarch64/${a}-${v}.apk"
            done ;;
        riscv64)
            [[ -f "$d/openjdk17-riscv64-musl-NATIVE-cross.tar.gz" ]] || {
                echo "prebuild: riscv64 native-musl JDK17 not in cache: $d/openjdk17-riscv64-musl-NATIVE-cross.tar.gz" >&2
                echo "prebuild: this JDK is cross-built from source once (see SOURCES); place it in the cache." >&2
                exit 4; }
    esac
}

# Stage JDK17 into $overlay_dir/opt/jdk17 (full JDK), per-arch source (== openjdk-multi).
stage_jdk17() {
    local jdst="$overlay_dir/opt/jdk17" d="$DL/openjdk17-apks/$arch"
    rm -rf "$jdst"; mkdir -p "$jdst"
    case "$arch" in
        x86_64|aarch64)
            local T; T="$(mktemp -d)"; local a apk
            for a in openjdk17-jdk openjdk17-jmods openjdk17-jre-headless openjdk17-jre; do
                apk="$(ls "$d/${a}-"*.apk 2>/dev/null | head -1)"
                [[ -n "$apk" ]] && tar xzf "$apk" -C "$T" 2>/dev/null || true
            done
            cp -a "$T/usr/lib/jvm/java-17-openjdk/." "$jdst/"; rm -rf "$T" ;;
        loongarch64)
            local T; T="$(mktemp -d)"; local a apk
            for a in openjdk17-loongarch-jdk openjdk17-loongarch-jmods openjdk17-loongarch-jre-headless openjdk17-loongarch-jre; do
                apk="$(ls "$d/${a}-"*.apk 2>/dev/null | head -1)"
                [[ -n "$apk" ]] && tar xzf "$apk" -C "$T" 2>/dev/null || true
            done
            cp -a "$T"/usr/lib/jvm/*/. "$jdst/"; rm -rf "$T" ;;
        riscv64)
            untar_strip1 "$d/openjdk17-riscv64-musl-NATIVE-cross.tar.gz" "$jdst" ;;
    esac
    [[ -x "$jdst/bin/java" ]] || { echo "prebuild: jdk17 staged without java for $arch" >&2; exit 3; }
    echo "prebuild: jdk17 staged ($(du -sh "$jdst" | cut -f1))"
}

# Stage the demo jars + jse-suite jar into /root/jse, the per-arch sqlite JNI native into
# /root/jse/native, and the run-jse.sh gate into /usr/bin.
stage_payload() {
    local jse="$overlay_dir/root/jse"
    install -d "$jse" "$jse/native"
    install -m0644 "$ASSETS/realdep-demo.jar" "$jse/"
    install -m0644 "$ASSETS/jdbc-demo.jar"    "$jse/"
    install -m0644 "$ASSETS/sqlite-demo.jar"  "$jse/"
    install -m0644 "$ASSETS/jse-suite.jar"    "$jse/"
    # sqlite-jdbc bundles musl native for x86_64/aarch64; riscv64/loongarch64 need the
    # cross-built JNI .so staged as libsqlitejdbc.so (run-jse.sh points lib.path at it).
    case "$arch" in
        riscv64)     install -m0644 "$ASSETS/native/libsqlitejdbc-riscv64.so"     "$jse/native/libsqlitejdbc.so" ;;
        loongarch64) install -m0644 "$ASSETS/native/libsqlitejdbc-loongarch64.so" "$jse/native/libsqlitejdbc.so" ;;
    esac
    install -Dm0755 "$PROG/run-jse.sh" "$overlay_dir/usr/bin/run-jse.sh"
    echo "prebuild: staged demo jars + jse-suite.jar into /root/jse, run-jse.sh into /usr/bin"
}

main() {
    case "$arch" in x86_64|aarch64|riscv64|loongarch64) ;; *) echo "prebuild: unsupported arch: $arch" >&2; exit 1 ;; esac
    ensure_host_tools
    ensure_jdk17
    grow_rootfs
    stage_jdk17
    stage_payload
    echo "prebuild: java-jse overlay ready for $arch — $(du -sh "$overlay_dir" | cut -f1)"
}

main "$@"
