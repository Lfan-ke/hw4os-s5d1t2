#!/bin/bash
# build-jweb-jars.sh — rebuild the java-web JEE framework carpet jars (CI-like, no bundled artifacts).
#
# Inputs (co-located in this delivery repo):
#   - a JDK 17 toolchain (javac/jar/zip)
#   - the dod-frameworks framework fat jars at ../dod-frameworks/jars/ which provide the
#     framework libraries: jetty-demo.jar (Eclipse Jetty + Jakarta Servlet), netty-demo.jar
#     (Netty 4.x), mybatis-demo.jar (MyBatis + sqlite-jdbc 3.46.1.3), hibernate-demo.jar
#     (Hibernate/JPA + sqlite-jdbc), r2dbc-demo.jar (R2DBC + r2dbc-h2 + Reactor), and the
#     cross-built sqlite musl JNI under ../dod-frameworks/jars/sqlite-musl-jni/
#     (framework maven coordinates / versions: see ../dod-frameworks/SOURCES.md).
# Output: assets/{jetty,netty,mybatis,hibernate,r2dbc}-demo.jar + assets/native/*.so, each a
#   framework fat jar with this delivery's carpet classes (org.starry.dod.*) layered in;
#   apps-starry/prebuild.sh stages them into the StarryOS per-app rootfs.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
JAVAC="${JAVAC:-javac}"; JAR="${JAR:-jar}"; ZIP="${ZIP:-zip}"
DODJ="$HERE/../dod-frameworks/jars"
C="$HERE/carpets"
OUT="$HERE/assets"; mkdir -p "$OUT/native"
B="$(mktemp -d)"; trap 'rm -rf "$B"' EXIT

# layer(<jar> <build-subdir> <carpet.java>...) : copy the framework fat jar, drop any stale
# org/starry/dod/ carpet classes, compile this delivery's carpets against it, re-add them.
layer() {
    local jar="$1"; shift; local sub="$1"; shift
    cp "$DODJ/$jar" "$OUT/$jar"
    "$ZIP" -q -d "$OUT/$jar" 'org/starry/dod/*' >/dev/null 2>&1 || true
    # --release 17: the target runs OpenJDK 17, so emit 17-compatible bytecode regardless of
    # the host javac version.
    "$JAVAC" --release 17 -cp "$OUT/$jar" -d "$B/$sub" "$@"
    ( cd "$B/$sub" && "$JAR" uf "$OUT/$jar" org )
    echo "build-jweb: $jar <- $(basename "$1" .java)$([ $# -gt 1 ] && echo ' (+more)')"
}

# JettyCarpet and WarCarpet both run from jetty-demo.jar (Jetty + embedded servlet container).
layer jetty-demo.jar     jetty     "$C/JettyCarpet.java" "$C/WarCarpet.java"
layer netty-demo.jar     netty     "$C/NettyCarpet.java"
layer mybatis-demo.jar   mybatis   "$C/MyBatisCarpet.java"
layer hibernate-demo.jar hibernate "$C/HibernateCarpet.java"
layer r2dbc-demo.jar     r2dbc     "$C/R2dbcCarpet.java"

# sqlite-jdbc musl JNI for riscv64/loongarch64 (the upstream jar bundles only x86_64/aarch64).
cp "$DODJ/sqlite-musl-jni/libsqlitejdbc-riscv64.so"     "$OUT/native/libsqlitejdbc-riscv64.so"
cp "$DODJ/sqlite-musl-jni/libsqlitejdbc-loongarch64.so" "$OUT/native/libsqlitejdbc-loongarch64.so"
echo "build-jweb: assets/{jetty,netty,mybatis,hibernate,r2dbc}-demo.jar + native/ built"
