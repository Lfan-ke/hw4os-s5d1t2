package org.starry.dod;

import io.r2dbc.spi.Connection;
import io.r2dbc.spi.ConnectionFactories;
import io.r2dbc.spi.ConnectionFactory;
import reactor.core.publisher.Flux;
import reactor.core.publisher.Mono;

/* DoD-C / P6: R2DBC reactive relational DB (r2dbc-h2 in-mem + Reactor). 反应式 (非阻塞)
 * DB 访问范式. block() 在测试里收敛结果. R2DBC_DONE on pass. */
public class R2dbcDemo {
    public static void main(String[] args) {
        int ok = 0, fail = 0;
        try {
            ConnectionFactory cf = ConnectionFactories.get("r2dbc:h2:mem:///testdb");
            Connection conn = Mono.from(cf.create()).block();
            Mono.from(conn.createStatement(
                    "CREATE TABLE acct (id INT PRIMARY KEY, name VARCHAR(32), bal INT)").execute())
                    .block();
            int[] inserted = {0};
            for (Object[] row : new Object[][]{{1,"alice",100},{2,"bob",50},{3,"carol",200}}) {
                Integer n = Flux.from(conn.createStatement(
                        "INSERT INTO acct VALUES (" + row[0] + ",'" + row[1] + "'," + row[2] + ")").execute())
                        .flatMap(r -> r.getRowsUpdated()).map(Long::intValue).blockFirst();
                inserted[0] += (n == null ? 0 : n);
            }
            if (inserted[0] == 3) ok++; else { fail++; System.out.println("FAIL insert=" + inserted[0]); }
            Long cnt = Flux.from(conn.createStatement("SELECT COUNT(*) c FROM acct").execute())
                    .flatMap(r -> r.map((rw, md) -> ((Number) rw.get(0)).longValue())).blockFirst();
            if (cnt != null && cnt == 3) ok++; else { fail++; System.out.println("FAIL count=" + cnt); }
            String top = Flux.from(conn.createStatement(
                    "SELECT name FROM acct ORDER BY bal DESC").execute())
                    .flatMap(r -> r.map((rw, md) -> (String) rw.get(0))).blockFirst();
            if ("carol".equals(top)) ok++; else { fail++; System.out.println("FAIL top=" + top); }
            Mono.from(conn.close()).block();
        } catch (Throwable t) {
            fail++; System.out.println("R2DBC_ERR: " + t); t.printStackTrace();
        }
        System.out.println("R2DBC_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0 && ok == 3) System.out.println("R2DBC_DONE");
        System.exit(fail == 0 ? 0 : 1);
    }
}
