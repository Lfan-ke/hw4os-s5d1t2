#!/bin/bash
# build-jse-jars.sh — rebuild the java-jse carpet test jars (CI-like, no bundled artifacts).
#
# Inputs (co-located in this delivery repo + one fetched dependency):
#   - a JDK 17 toolchain (javac/jar)
#   - the dod-frameworks library fat jars at ../dod-frameworks/jars/ (provide jackson/guava/
#     commons-lang3 in realdep-demo.jar, H2/slf4j+logback in jdbc-demo.jar, sqlite-jdbc in
#     sqlite-demo.jar) and the cross-built sqlite musl JNI under
#     ../dod-frameworks/jars/sqlite-musl-jni/
#   - the lombok 1.18.34 annotation processor (fetch from Maven Central, see SOURCES.md);
#     point LOMBOK_JAR at it.
# Output: assets/{realdep-demo,jdbc-demo,sqlite-demo,jse-suite}.jar + assets/native/*.so,
# which prebuild.sh stages into the StarryOS per-app rootfs.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
JAVAC="${JAVAC:-javac}"; JAR="${JAR:-jar}"
DODJ="$HERE/../dod-frameworks/jars"
LOMBOK="${LOMBOK_JAR:?set LOMBOK_JAR to lombok-1.18.34.jar (Maven Central; see SOURCES.md)}"
OUT="$HERE/assets"; mkdir -p "$OUT/native"
B="$(mktemp -d)"; trap 'rm -rf "$B"' EXIT

cp "$DODJ/realdep-demo.jar" "$DODJ/jdbc-demo.jar" "$DODJ/sqlite-demo.jar" "$OUT/"
"$JAVAC" --release 17 -cp "$OUT/realdep-demo.jar" -d "$B/realdep" "$HERE/lib-carpets/JacksonCarpet.java" "$HERE/lib-carpets/GuavaCarpet.java" "$HERE/lib-carpets/Lang3Carpet.java"
( cd "$B/realdep" && "$JAR" uf "$OUT/realdep-demo.jar" org )
"$JAVAC" --release 17 -cp "$OUT/jdbc-demo.jar" -d "$B/jdbc" "$HERE/lib-carpets/H2Carpet.java" "$HERE/lib-carpets/LogCarpet.java"
( cd "$B/jdbc" && "$JAR" uf "$OUT/jdbc-demo.jar" org )
"$JAVAC" --release 17 -cp "$OUT/sqlite-demo.jar" -d "$B/sqlite" "$HERE/lib-carpets/SqliteJdbcCarpet.java"
( cd "$B/sqlite" && "$JAR" uf "$OUT/sqlite-demo.jar" org )

"$JAVAC" --release 17 -cp "$LOMBOK" -processorpath "$LOMBOK" -d "$B/suite" "$HERE/jse-suite/LombokCarpet.java"
"$JAVAC" --release 17 -d "$B/suite" "$HERE"/jse-suite/*Test.java "$HERE/jse-suite/ConcurrencyDeep.java"
( cd "$B/suite" && "$JAR" cf "$OUT/jse-suite.jar" . )

cp "$DODJ/sqlite-musl-jni/libsqlitejdbc-riscv64.so"     "$OUT/native/libsqlitejdbc-riscv64.so"
cp "$DODJ/sqlite-musl-jni/libsqlitejdbc-loongarch64.so" "$OUT/native/libsqlitejdbc-loongarch64.so"
echo "build-jse-jars: assets/{realdep-demo,jdbc-demo,sqlite-demo,jse-suite}.jar + native/ built"
