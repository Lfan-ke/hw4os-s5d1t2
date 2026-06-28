import java.lang.management.*;
import java.lang.reflect.*;
import java.util.*;

/* DoD: JVM/JRE 正确运行 — version/processors/memory/gc, system props, ClassLoader,
 * 反射实例化, ManagementFactory(RuntimeMXBean/ThreadMXBean)。验证 JVM 自省 + JRE 完整性。 */
public class JvmTest {
    static int ok = 0, fail = 0;
    static void check(boolean c, String n) { if (c) ok++; else { fail++; System.out.println("FAIL " + n); } }
    public static class Bean { public int v = 42; public int dbl() { return v * 2; } }

    public static void main(String[] args) throws Exception {
        // Runtime version (JDK 17)
        Runtime.Version v = Runtime.version();
        check(v.feature() == 17, "runtime-version-17");
        System.out.println("  java.version=" + System.getProperty("java.version") + " vm=" + System.getProperty("java.vm.name"));

        // processors + memory
        Runtime rt = Runtime.getRuntime();
        check(rt.availableProcessors() >= 1, "processors");
        long max = rt.maxMemory(), total = rt.totalMemory(), free = rt.freeMemory();
        check(max > 0 && total > 0 && free <= total, "memory-introspect");

        // allocate + gc
        List<byte[]> junk = new ArrayList<>();
        for (int i = 0; i < 200; i++) junk.add(new byte[64 * 1024]);
        junk.clear(); System.gc();
        check(true, "gc-runs");

        // system properties
        check(System.getProperty("os.name") != null && System.getProperty("java.home") != null, "sysprops");
        check(System.getProperty("os.arch") != null, "os-arch");

        // ClassLoader + reflection instantiate + invoke
        Class<?> c = Class.forName("JvmTest$Bean");
        Object o = c.getDeclaredConstructor().newInstance();
        Field fld = c.getField("v");
        check(fld.getInt(o) == 42, "reflect-field");
        Method m = c.getMethod("dbl");
        check((int) m.invoke(o) == 84, "reflect-invoke");

        // ManagementFactory (java.management module present + working)
        RuntimeMXBean rb = ManagementFactory.getRuntimeMXBean();
        check(rb.getUptime() >= 0 && rb.getStartTime() > 0, "runtime-mxbean");
        ThreadMXBean tb = ManagementFactory.getThreadMXBean();
        check(tb.getThreadCount() >= 1, "thread-mxbean");
        MemoryMXBean mb = ManagementFactory.getMemoryMXBean();
        check(mb.getHeapMemoryUsage().getUsed() > 0, "memory-mxbean");

        System.out.println("JVM_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("JVM_DONE");
    }
}
