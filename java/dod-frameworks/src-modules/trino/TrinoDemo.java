package org.starry.dod;

import io.trino.testing.LocalQueryRunner;
import io.trino.Session;
import io.trino.plugin.tpch.TpchPlugin;
import io.trino.testing.MaterializedResult;
import io.trino.testing.MaterializedRow;

import static io.trino.testing.TestingSession.testSessionBuilder;

/* DoD-C / P6: Trino (distributed SQL engine, heavy JVM) single-node, in-process.
 *
 * FEASIBILITY (read SOURCES.md before running): the real Trino server distribution is a
 * ~1 GB unpack that wants a multi-GB heap, G1/JIT, off-heap slices, dozens of modules and a
 * Discovery/coordinator HTTP cluster — it is NOT a realistic target under starry's -Xint +
 * single-core + emulated-arch + capped-RAM constraints. The DoD here is the smallest
 * *real* Trino execution path: io.trino.testing.LocalQueryRunner — the same query engine
 * (parser → analyzer → planner → optimizer → local execution) the server runs, embedded in
 * one JVM with no HTTP cluster — plus the built-in TPCH connector (pure compute, generates
 * data on the fly, no storage). Runs SELECT 1 and a trivial TPCH count, verifies results.
 *
 *   java -Xint -Xms256m -Xmx1024m -XX:MaxMetaspaceSize=512m -cp 'trino-libs/*' org.starry.dod.TrinoDemo
 *
 * Trino itself requires Java 22+ from version 436 onward; the LAST Java-17-compatible line
 * is Trino 435 (Dec 2023). This demo therefore pins Trino 435. See SOURCES.md. */
public class TrinoDemo {
    public static void main(String[] args) {
        int ok = 0, fail = 0;
        LocalQueryRunner runner = null;
        try {
            Session session = testSessionBuilder()
                    .setCatalog("tpch")
                    .setSchema("tiny")
                    .build();
            runner = LocalQueryRunner.create(session);
            runner.installPlugin(new TpchPlugin());
            runner.createCatalog("tpch", "tpch", java.util.Map.of());

            // (1) trivial constant query through the full engine
            MaterializedResult r1 = runner.execute("SELECT 1");
            long v1 = ((Number) r1.getMaterializedRows().get(0).getField(0)).longValue();
            if (v1 == 1L) ok++; else { fail++; System.out.println("FAIL select1=" + v1); }

            // (2) trivial query against the TPCH connector (region table has 5 rows)
            MaterializedResult r2 = runner.execute("SELECT count(*) FROM region");
            long v2 = ((Number) r2.getMaterializedRows().get(0).getField(0)).longValue();
            if (v2 == 5L) ok++; else { fail++; System.out.println("FAIL region_count=" + v2); }

            // (3) a slightly richer aggregate to exercise the optimizer/executor
            MaterializedResult r3 = runner.execute("SELECT count(*) FROM nation WHERE regionkey = 1");
            long v3 = ((Number) r3.getMaterializedRows().get(0).getField(0)).longValue();
            if (v3 == 5L) ok++; else { fail++; System.out.println("FAIL nation_region1=" + v3); }
        } catch (Throwable t) {
            fail++; System.out.println("TRINO_ERR: " + t); t.printStackTrace();
        } finally {
            if (runner != null) try { runner.close(); } catch (Throwable ignored) {}
        }
        System.out.println("TRINO_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0 && ok == 3) System.out.println("TRINO_DONE");
        System.exit(fail == 0 ? 0 : 1);
    }
}
