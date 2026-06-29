#!/bin/sh
# run-jweb.sh — on-target gate for the StarryOS java-web JEE framework carpet.
# Industrial-grade carpets for Jetty/Netty/Undertow (loopback HTTP servers), MyBatis/Hibernate
# (ORM, in-memory DB), R2DBC (reactive DB), and a real .war deployment into an embedded Jetty
# servlet container. TEST PASSED only when every module passes (PASS==TOTAL, no skip).
set -u
case "$(uname -m)" in x86_64) A=x86_64;; aarch64) A=aarch64;; riscv64) A=riscv64;; loongarch64) A=loongarch64;; *) A="$(uname -m)";; esac
JH=/opt/jdk17
printf '/lib\n/usr/lib\n%s/lib\n%s/lib/server\n' "$JH" "$JH" > "/etc/ld-musl-$A.path"
export JAVA_HOME="$JH" PATH="$JH/bin:$PATH"
J="$JH/bin/java -Xint -Xms32m -Xmx512m"
D=/root/jweb
# sqlite-jdbc (MyBatis/Hibernate) bundles musl native only for x86_64/aarch64; riscv64/loongarch64
# load the cross-built JNI .so staged by prebuild at $D/native/libsqlitejdbc.so.
case "$A" in riscv64|loongarch64) SQLP="-Dorg.sqlite.lib.path=$D/native -Dorg.sqlite.lib.name=libsqlitejdbc.so" ;; *) SQLP="" ;; esac
PASS=0; TOTAL=0
run(){ name="$1"; mk="$2"; shift 2; TOTAL=$((TOTAL+1)); "$@" >"/tmp/$name.out" 2>&1; if grep -aq "$mk" "/tmp/$name.out" 2>/dev/null; then echo "  OK   $name ($mk)"; PASS=$((PASS+1)); else echo "  FAIL $name ($mk)"; grep -aiE 'exception|error|fail|bind|address' "/tmp/$name.out"|tail -6; fi; }
echo "=== java-web: JEE framework carpets (jetty/netty/undertow | mybatis/hibernate/r2dbc | war) ==="
run jetty     JETTY_DONE     $J -cp $D/jetty-demo.jar     org.starry.dod.JettyCarpet
run netty     NETTY_DONE     $J -cp $D/netty-demo.jar     org.starry.dod.NettyCarpet
run mybatis   MYBATIS_DONE   $J $SQLP -cp $D/mybatis-demo.jar   org.starry.dod.MyBatisCarpet
run hibernate HIBERNATE_DONE $J $SQLP -cp $D/hibernate-demo.jar org.starry.dod.HibernateCarpet
run r2dbc     R2DBC_DONE     $J -cp $D/r2dbc-demo.jar     org.starry.dod.R2dbcCarpet
run war       WAR_DONE       $J -cp $D/jetty-demo.jar     org.starry.dod.WarCarpet
echo "AGGREGATE: PASS=$PASS TOTAL=$TOTAL"
if [ "$PASS" = "$TOTAL" ] && [ "$TOTAL" -gt 0 ]; then echo "JAVA_WEB_OK=$PASS/$TOTAL"; echo "TEST PASSED"; exit 0; fi
echo "TEST FAILED"; exit 1
