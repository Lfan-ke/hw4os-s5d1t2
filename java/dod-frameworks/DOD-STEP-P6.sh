# ============================================================================
# DoD-C / P6 frameworks: ktor / quarkus / wildfly / trino
# Append these lines into each test-suit/starryos/stress/openjdk17-0/openjdk17-0/
# qemu-<arch>.toml `shell_init_cmd`, JUST BEFORE the final `echo "BIGTEST_DONE"`.
# Identical across all 4 arches EXCEPT the trino block (x86_64-only guard).
# Markers must be anchored with `grep -aoE '^EE-...'` when reading run output
# (the `|| { echo ...}` text is echoed by the shell and is NOT the real result).
# ============================================================================

# --- DoD-C / P6: Ktor (Kotlin async web, Netty engine; loopback 127.0.0.1:18082) ---
$JAVA_HOME/bin/java -Xint -Xms64m -Xmx256m -jar /root/dod/ktor-demo.jar >/tmp/ktor.out 2>&1
grep -q KTOR_DONE /tmp/ktor.out && echo "EE-KTOR ok=1" || { echo "EE-KTOR ok=0"; grep -aiE "KTOR_ERR|FAIL|Exception" /tmp/ktor.out | tail -6; }

# --- DoD-C / P6: Quarkus JVM mode (RESTEasy Reactive + Vert.x; loopback 127.0.0.1:18083) ---
$JAVA_HOME/bin/java -Xint -Xms128m -Xmx384m -Djava.net.preferIPv4Stack=true -jar /root/dod/quarkus-demo.jar >/tmp/quarkus.out 2>&1
grep -q QUARKUS_DONE /tmp/quarkus.out && echo "EE-QUARKUS ok=1" || { echo "EE-QUARKUS ok=0"; grep -aiE "QUARKUS_ERR|FAIL|Exception" /tmp/quarkus.out | tail -6; }

# --- DoD-C / P6: WildFly Jakarta EE (RESTEasy JAX-RS on Undertow Servlet; loopback 127.0.0.1:18084) ---
$JAVA_HOME/bin/java -Xint -Xms64m -Xmx256m -jar /root/dod/wildfly-demo.jar >/tmp/wildfly.out 2>&1
grep -q WILDFLY_DONE /tmp/wildfly.out && echo "EE-WILDFLY ok=1" || { echo "EE-WILDFLY ok=0"; grep -aiE "WILDFLY_ERR|FAIL|Exception" /tmp/wildfly.out | tail -6; }

# --- DoD-C / P6: Trino (heavy; in-process LocalQueryRunner + TPCH). x86_64-only until proven. ---
# Trino 435 = last Java-17 release. ~150-250MB lib dir, needs big heap+metaspace. Skip on
# slow/emulated arches by default; flip the guard once host + x86_64 starry are green.
ARCH="$(uname -m 2>/dev/null || echo unknown)"
if [ "$ARCH" = "x86_64" ]; then
  # Trino needs the same --add-opens its launcher sets; confirm/adjust per host ground-truth (SOURCES.md).
  $JAVA_HOME/bin/java -Xint -Xms256m -Xmx1024m -XX:MaxMetaspaceSize=512m \
    --add-opens=java.base/java.nio=ALL-UNNAMED --add-opens=java.base/sun.nio.ch=ALL-UNNAMED \
    --add-opens=java.base/java.lang=ALL-UNNAMED --add-opens=java.base/java.lang.invoke=ALL-UNNAMED \
    -cp '/root/dod/trino-libs/*' org.starry.dod.TrinoDemo >/tmp/trino.out 2>&1
  grep -q TRINO_DONE /tmp/trino.out && echo "EE-TRINO ok=1" || { echo "EE-TRINO ok=0"; grep -aiE "TRINO_ERR|FAIL|Exception|OutOfMemory" /tmp/trino.out | tail -8; }
else
  echo "EE-TRINO skip=1 (arch=$ARCH; heavy JVM, x86_64-only until proven — see SOURCES.md)"
fi
