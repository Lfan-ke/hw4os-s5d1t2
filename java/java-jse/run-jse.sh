#!/bin/sh
# run-jse.sh — on-target gate for the StarryOS java-jse J2SE library + JSE stdlib carpet.
#
# Staged into the rootfs by prebuild.sh and invoked as the ENTIRE shell_init_cmd
# (`sh /usr/bin/run-jse.sh`). The gate lives in a staged script, not inline in the toml, so
# the harness does not echo a literal `TEST PASSED` back over the serial console and
# self-match success_regex: TEST PASSED is printed ONLY by this script's real stdout, ONLY
# when every module passed (PASS==TOTAL). No silent skip — every module runs on every arch.
#
# Each module is an industrial-grade carpet exercising the full public API surface of one
# library / JDK package (hundreds of exact-value assertions per module), terminated by an
# anchored *_DONE marker that is printed only when its own internal fail count is zero.
set -u

case "$(uname -m)" in
  x86_64)      ARCH=x86_64 ;;
  aarch64)     ARCH=aarch64 ;;
  riscv64)     ARCH=riscv64 ;;
  loongarch64) ARCH=loongarch64 ;;
  *)           ARCH="$(uname -m)" ;;
esac

JH=/opt/jdk17
# musl dynamic-loader search path: only this JDK's lib dirs (libjvm.so lives in lib/server).
printf '/lib\n/usr/lib\n%s/lib\n%s/lib/server\n' "$JH" "$JH" > "/etc/ld-musl-$ARCH.path"
export JAVA_HOME="$JH" PATH="$JH/bin:$PATH"

# StarryOS JIT is still unstable (#206) -> force the interpreter on every JVM.
J="$JH/bin/java -Xint -Xms32m -Xmx384m"
D=/root/jse

PASS=0
TOTAL=0
run() { # run <name> <marker> <cmd...>
    name="$1"; marker="$2"; shift 2
    TOTAL=$((TOTAL + 1))
    "$@" > "/tmp/$name.out" 2>&1
    if grep -aq "$marker" "/tmp/$name.out" 2>/dev/null; then
        echo "  OK   $name ($marker)"
        PASS=$((PASS + 1))
    else
        echo "  FAIL $name ($marker)"
        grep -aiE 'exception|error|fail|not found' "/tmp/$name.out" | tail -6
    fi
}

echo "=== java-jse: J2SE library carpets (jackson/guava/commons-lang3 | H2/slf4j+logback | sqlite-jdbc | lombok) ==="
run jackson JACKSON_DONE $J -cp $D/realdep-demo.jar org.starry.dod.JacksonCarpet
run guava   GUAVA_DONE   $J -cp $D/realdep-demo.jar org.starry.dod.GuavaCarpet
run lang3   LANG3_DONE   $J -cp $D/realdep-demo.jar org.starry.dod.Lang3Carpet
run h2      H2_DONE      $J -cp $D/jdbc-demo.jar    org.starry.dod.H2Carpet
run log     LOG_DONE     $J -cp $D/jdbc-demo.jar    org.starry.dod.LogCarpet

# sqlite-jdbc: x86_64/aarch64 use the jar's bundled musl native; riscv64/loongarch64 load the
# cross-built JNI .so staged at $D/native/libsqlitejdbc.so.
case "$ARCH" in
    riscv64|loongarch64) SQLP="-Dorg.sqlite.lib.path=$D/native -Dorg.sqlite.lib.name=libsqlitejdbc.so" ;;
    *)                   SQLP="" ;;
esac
run sqlite  SQLITEJDBC_DONE $J $SQLP -cp $D/sqlite-demo.jar org.starry.dod.SqliteJdbcCarpet
run lombok  LOMBOK_DONE  $J -cp $D/jse-suite.jar org.starry.dod.LombokCarpet

echo "=== java-jse: JSE standard-library carpets (15 modules) ==="
for pair in \
    AlgoTest:ALGO_DONE ConcurrencyTest:CONC_DONE ConcurrencyDeep:CONCURRENCY_DEEP_DONE \
    CryptoTest:CRYPTO_DONE ExtraTest:EXTRA_DONE FileTest:FILE_DONE JvmTest:JVM_DONE \
    LangUtilTest:LANGUTIL_DONE NetTest:NET_DONE NioChannelTest:NIOCH_DONE ProcessTest:PROCESS_DONE \
    StdlibTest:STDLIB_DONE SyntaxTest:SYNTAX_DONE TimeTest:TIME_DONE XmlTest:XML_DONE
do
    cls="${pair%%:*}"
    mk="${pair##*:}"
    run "jse_$cls" "$mk" $J -cp $D/jse-suite.jar "$cls"
done

echo "AGGREGATE: PASS=$PASS TOTAL=$TOTAL"
if [ "$PASS" = "$TOTAL" ] && [ "$TOTAL" -gt 0 ]; then
    echo "JAVA_JSE_OK=$PASS/$TOTAL"
    echo "TEST PASSED"
    exit 0
fi
echo "TEST FAILED"
exit 1
