package org.starry.dod;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.sql.*;

/* DoD: java.sql JDBC via H2 embedded in-memory DB + SLF4J/Logback logging (第三方).
 * Covers Connection/Statement/PreparedStatement/ResultSet/transaction(commit+rollback).
 * Fat jar, runs on starry with `java -jar`. JDBC_DONE on pass. */
public class JdbcDemo {
    static final Logger log = LoggerFactory.getLogger(JdbcDemo.class);
    static int ok = 0, fail = 0;
    static void check(boolean c, String n) { if (c) ok++; else { fail++; System.out.println("FAIL " + n); } }

    public static void main(String[] args) throws Exception {
        log.info("starting JDBC demo (H2 in-memory)");
        try (Connection conn = DriverManager.getConnection("jdbc:h2:mem:dod;DB_CLOSE_DELAY=-1")) {
            // DDL
            try (Statement st = conn.createStatement()) {
                st.execute("CREATE TABLE users(id INT PRIMARY KEY, name VARCHAR(64), age INT)");
            }
            // PreparedStatement insert
            try (PreparedStatement ps = conn.prepareStatement("INSERT INTO users(id,name,age) VALUES(?,?,?)")) {
                for (int i = 1; i <= 5; i++) { ps.setInt(1, i); ps.setString(2, "u" + i); ps.setInt(3, 20 + i); ps.addBatch(); }
                int[] r = ps.executeBatch();
                check(r.length == 5, "batch-insert");
            }
            // ResultSet query
            try (PreparedStatement ps = conn.prepareStatement("SELECT COUNT(*) c, AVG(age) a FROM users WHERE age > ?")) {
                ps.setInt(1, 22);
                try (ResultSet rs = ps.executeQuery()) {
                    rs.next();
                    check(rs.getInt("c") == 3, "query-count");
                    check(rs.getDouble("a") == 24.0, "query-avg");
                }
            }
            // transaction: rollback
            conn.setAutoCommit(false);
            try (Statement st = conn.createStatement()) {
                st.executeUpdate("UPDATE users SET age=99 WHERE id=1");
                conn.rollback();
            }
            try (Statement st = conn.createStatement(); ResultSet rs = st.executeQuery("SELECT age FROM users WHERE id=1")) {
                rs.next();
                check(rs.getInt(1) == 21, "transaction-rollback");
            }
            // transaction: commit
            try (Statement st = conn.createStatement()) {
                st.executeUpdate("UPDATE users SET age=100 WHERE id=2");
                conn.commit();
            }
            try (Statement st = conn.createStatement(); ResultSet rs = st.executeQuery("SELECT age FROM users WHERE id=2")) {
                rs.next();
                check(rs.getInt(1) == 100, "transaction-commit");
            }
            // metadata
            DatabaseMetaData md = conn.getMetaData();
            check(md.getDatabaseProductName().toLowerCase().contains("h2"), "db-metadata");
        }
        log.info("JDBC demo finished: ok={} fail={}", ok, fail);
        System.out.println("JDBC_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("JDBC_DONE");
    }
}
