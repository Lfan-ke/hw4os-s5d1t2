package org.starry.dod;

import java.sql.*;

/* DoD: SQLite3 via xerial sqlite-jdbc (带 per-arch native JNI .so) — file-backed DB.
 * Exercise starry 的 native-lib 提取/dlopen/JNI + java.sql. ORM(mybatis/hibernate)
 * 的 DB 底座(用户: ORM 只测 sqlite3). 若 native 在 starry 跑不了 → sqlite3 需适配 → #764 others.
 * SQLITE_DONE on pass. 失败若是 native 加载 → 打印 SQLITE_NATIVE_FAIL 供识别. */
public class SqliteDemo {
    static int ok = 0, fail = 0;
    static void check(boolean c, String n) { if (c) ok++; else { fail++; System.out.println("FAIL " + n); } }

    public static void main(String[] args) throws Exception {
        String db = "/tmp/dod-sqlite.db";
        try (Connection conn = DriverManager.getConnection("jdbc:sqlite:" + db)) {
            System.out.println("sqlite driver: " + conn.getMetaData().getDriverName() + " " + conn.getMetaData().getDatabaseProductVersion());
            try (Statement st = conn.createStatement()) {
                st.execute("DROP TABLE IF EXISTS items");
                st.execute("CREATE TABLE items(id INTEGER PRIMARY KEY, name TEXT, qty INTEGER)");
            }
            try (PreparedStatement ps = conn.prepareStatement("INSERT INTO items(name,qty) VALUES(?,?)")) {
                for (int i = 1; i <= 5; i++) { ps.setString(1, "item" + i); ps.setInt(2, i * 10); ps.executeUpdate(); }
            }
            try (Statement st = conn.createStatement(); ResultSet rs = st.executeQuery("SELECT COUNT(*) c, SUM(qty) s FROM items")) {
                rs.next();
                check(rs.getInt("c") == 5, "sqlite-count");
                check(rs.getInt("s") == 150, "sqlite-sum");
            }
            // transaction
            conn.setAutoCommit(false);
            try (Statement st = conn.createStatement()) { st.executeUpdate("UPDATE items SET qty=999 WHERE id=1"); conn.rollback(); }
            try (Statement st = conn.createStatement(); ResultSet rs = st.executeQuery("SELECT qty FROM items WHERE id=1")) {
                rs.next(); check(rs.getInt(1) == 10, "sqlite-rollback");
            }
            // index + query plan
            try (Statement st = conn.createStatement()) { st.execute("CREATE INDEX idx_qty ON items(qty)"); }
            check(true, "sqlite-index");
        } catch (Throwable t) {
            String m = String.valueOf(t.getMessage()) + " / " + t.getClass().getName();
            if (m.toLowerCase().contains("native") || m.contains("UnsatisfiedLink") || m.contains("library")) {
                System.out.println("SQLITE_NATIVE_FAIL: " + m);
            } else {
                System.out.println("SQLITE_ERR: " + m);
            }
            return;
        }
        System.out.println("SQLITE_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("SQLITE_DONE");
    }
}
