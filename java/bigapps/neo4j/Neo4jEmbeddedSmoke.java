import java.nio.file.*;
import java.util.*;
import org.neo4j.dbms.api.*;
import org.neo4j.graphdb.*;
import org.neo4j.configuration.GraphDatabaseSettings;
import org.neo4j.kernel.internal.Version;

// Embedded Neo4j 2026.04 comprehensive smoke: boot the REAL graph-DB kernel in-process
// (no HTTP server) and exercise the full graph CRUD surface via Cypher -- create nodes
// AND relationships, traverse, read relationship properties, UPDATE, DELETE a
// relationship, DELETE a node, and assert every step round-trips. This drives: page
// cache + record/token/relationship stores, the transaction log (WAL), the Cypher
// parser->planner->runtime, relationship traversal, and the GraphDatabaseService API --
// the same kernel the full server wraps. Discrete PASS/FAIL tokens feed the gate; the
// success token is printed ONLY by the shell after confirming pass==total.
public class Neo4jEmbeddedSmoke {
  static int pass=0, total=0;
  static void acc(boolean ok, String m){ total++; if(ok){pass++; System.out.println("OK   "+m);} else System.out.println("FAIL "+m); }

  // run a single-row scalar query inside its own tx
  static Object scalar(GraphDatabaseService db, String cy, Map<String,Object> p, String col){
    try (Transaction tx = db.beginTx()) {
      Result r = tx.execute(cy, p);
      Object v = r.hasNext() ? r.next().get(col) : null;
      tx.commit();
      return v;
    }
  }

  public static void main(String[] a) throws Exception {
    // 1) kernel version string must be 2026.04.0
    String ver = Version.getNeo4jVersion();
    System.out.println("NEO4J_KERNEL_VERSION="+ver);
    acc(ver.startsWith("2026.04.0"), "kernel version 2026.04.0");

    java.nio.file.Path home = Files.createTempDirectory("neo4j-embed-");
    DatabaseManagementService dbms = new DatabaseManagementServiceBuilder(home)
        .setConfig(GraphDatabaseSettings.pagecache_memory, 64L*1024*1024)
        .build();
    try {
      GraphDatabaseService db = dbms.database(GraphDatabaseSettings.DEFAULT_DATABASE_NAME);
      // 2) DB available (kernel started + default DB online)
      acc(db.isAvailable(20000), "default DB available");

      // 3) WRITE: create two :Person nodes and a typed relationship with a property
      try (Transaction tx = db.beginTx()) {
        tx.execute(
          "CREATE (a:Person {name:$an, age:$aa}), (b:Person {name:$bn, age:$ba}), " +
          "(a)-[:KNOWS {since:$since}]->(b)",
          Map.of("an","Alice","aa",30L,"bn","Bob","ba",25L,"since",2020L));
        tx.commit();
      }
      acc(true, "cypher CREATE 2 nodes + 1 relationship committed");

      // 4) node count == 2
      Object nc = scalar(db, "MATCH (p:Person) RETURN count(p) AS c", Map.of(), "c");
      acc(nc instanceof Long && (Long)nc==2L, "node count == 2 (got "+nc+")");

      // 5) relationship traversal: Alice-[:KNOWS]->? yields Bob
      Object peer = scalar(db,
        "MATCH (a:Person {name:$n})-[:KNOWS]->(b:Person) RETURN b.name AS bn",
        Map.of("n","Alice"), "bn");
      System.out.println("TRAVERSE Alice-[:KNOWS]->"+peer);
      acc("Bob".equals(peer), "relationship traversal Alice-KNOWS->Bob");

      // 6) relationship PROPERTY round-trip (since == 2020)
      Object since = scalar(db,
        "MATCH (:Person {name:$n})-[r:KNOWS]->(:Person) RETURN r.since AS s",
        Map.of("n","Alice"), "s");
      acc(since instanceof Long && (Long)since==2020L, "relationship property since==2020 (got "+since+")");

      // 7) relationship count == 1
      Object rc = scalar(db, "MATCH ()-[r:KNOWS]->() RETURN count(r) AS c", Map.of(), "c");
      acc(rc instanceof Long && (Long)rc==1L, "relationship count == 1 (got "+rc+")");

      // 8) UPDATE: SET Alice.age = 31, read back
      try (Transaction tx = db.beginTx()) {
        tx.execute("MATCH (a:Person {name:$n}) SET a.age=$v", Map.of("n","Alice","v",31L));
        tx.commit();
      }
      Object age = scalar(db, "MATCH (a:Person {name:$n}) RETURN a.age AS age", Map.of("n","Alice"), "age");
      acc(age instanceof Long && (Long)age==31L, "property UPDATE Alice.age 30->31 (got "+age+")");

      // 9) property query on the other node (Bob.age == 25)
      Object bage = scalar(db, "MATCH (b:Person {name:$n}) RETURN b.age AS age", Map.of("n","Bob"), "age");
      acc(bage instanceof Long && (Long)bage==25L, "property query Bob.age==25 (got "+bage+")");

      // 10) DELETE the relationship; rel count -> 0, nodes still 2
      try (Transaction tx = db.beginTx()) {
        tx.execute("MATCH ()-[r:KNOWS]->() DELETE r");
        tx.commit();
      }
      Object rc2 = scalar(db, "MATCH ()-[r:KNOWS]->() RETURN count(r) AS c", Map.of(), "c");
      Object nc2 = scalar(db, "MATCH (p:Person) RETURN count(p) AS c", Map.of(), "c");
      acc(rc2 instanceof Long && (Long)rc2==0L && nc2 instanceof Long && (Long)nc2==2L,
          "DELETE relationship -> rels=0 nodes=2 (got rels="+rc2+" nodes="+nc2+")");

      // 11) DELETE a node; node count -> 1, remaining is Alice
      try (Transaction tx = db.beginTx()) {
        tx.execute("MATCH (b:Person {name:$n}) DELETE b", Map.of("n","Bob"));
        tx.commit();
      }
      Object nc3 = scalar(db, "MATCH (p:Person) RETURN count(p) AS c", Map.of(), "c");
      Object last = scalar(db, "MATCH (p:Person) RETURN p.name AS n", Map.of(), "n");
      acc(nc3 instanceof Long && (Long)nc3==1L && "Alice".equals(last),
          "DELETE node Bob -> nodes=1 remaining=Alice (got nodes="+nc3+" last="+last+")");
    } finally {
      dbms.shutdown();
    }
    System.out.println("NEO4J_RESULT pass="+pass+" total="+total);
    System.out.println("NEO4J_SMOKE_DONE");
  }
}
