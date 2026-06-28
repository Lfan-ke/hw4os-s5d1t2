// IcebergSmoke.java — single-core smoke driver for the `iceberg-0` StarryOS stress case.
//
// Apache Iceberg ships ONLY as a LIBRARY jar (iceberg-spark-runtime-3.5_2.12-1.11.0.jar);
// there is no standalone "iceberg" daemon to launch. The smoke therefore drives the core
// table-format library directly on the openjdk17 musl JVM: it puts the shaded runtime jar
// on the classpath, loads two core classes (org.apache.iceberg.Schema +
// org.apache.iceberg.types.Types), builds an in-memory Iceberg Schema with three typed
// NestedFields, and introspects it. NO filesystem / catalog / network is touched — this is
// pure in-JVM compute that proves the library loads and its public API works under starry.
//
// The driver is launched on the guest via Java 17 single-file source-code mode
// (`java -cp <jar> /root/iceberg/IcebergSmoke.java`) so there is no precompiled .class to
// stage and no bytecode-version skew (the guest JVM compiles it to its own level).
//
// Host-validated (OpenJDK 17.0.18, -Xint, jar on cp): ICEBERG_RESULT pass=4 total=4 + ICEBERG_OK=1.
import org.apache.iceberg.Schema;
import org.apache.iceberg.types.Types;

public class IcebergSmoke {
  static int PASS = 0, TOTAL = 0;
  static void acc(boolean ok, String name) {
    TOTAL++;
    if (ok) { PASS++; System.out.println("OK   " + name); }
    else    {        System.out.println("FAIL " + name); }
  }
  public static void main(String[] args) {
    // 1) core class loads from the shaded runtime jar (classpath wiring proof)
    try {
      Class<?> c = Class.forName("org.apache.iceberg.Schema");
      acc(c != null, "load org.apache.iceberg.Schema");
    } catch (Throwable t) { acc(false, "load org.apache.iceberg.Schema [" + t + "]"); }
    // 2) types factory class loads
    try {
      Class<?> c = Class.forName("org.apache.iceberg.types.Types");
      acc(c != null, "load org.apache.iceberg.types.Types");
    } catch (Throwable t) { acc(false, "load org.apache.iceberg.types.Types [" + t + "]"); }
    // 3) build an in-memory Schema (real Iceberg API exercise: NestedField + Types)
    Schema schema = null;
    try {
      schema = new Schema(
        Types.NestedField.required(1, "id", Types.LongType.get()),
        Types.NestedField.optional(2, "name", Types.StringType.get()),
        Types.NestedField.required(3, "ts", Types.TimestampType.withZone())
      );
      acc(schema != null, "build in-memory Schema (3 fields)");
    } catch (Throwable t) { acc(false, "build Schema [" + t + "]"); }
    // 4) introspect the built schema: column count + a field lookup round-trip
    try {
      boolean ok = schema != null
        && schema.columns().size() == 3
        && schema.findField("name") != null
        && schema.findField(1).name().equals("id");
      acc(ok, "schema introspection (3 cols, findField id/name)");
    } catch (Throwable t) { acc(false, "schema introspection [" + t + "]"); }
    System.out.println("ICEBERG_RESULT pass=" + PASS + " total=" + TOTAL);
    int v = (PASS == TOTAL && TOTAL == 4) ? 1 : 0;
    // NOTE: driver emits ICEBERG_DRIVER_OK (NOT the bare success token) so the qemu toml's
    // success_regex ^ICEBERG_OK=1 can ONLY match the shell's own final printf (no silent pass).
    System.out.println("ICEBERG_DRIVER_OK=" + v);
    System.out.println("ICEBERG_DONE");
  }
}
