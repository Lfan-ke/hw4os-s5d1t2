package org.starry.dod;

import org.apache.ibatis.annotations.*;
import org.apache.ibatis.datasource.pooled.PooledDataSource;
import org.apache.ibatis.mapping.Environment;
import org.apache.ibatis.session.*;
import org.apache.ibatis.transaction.jdbc.JdbcTransactionFactory;

import javax.sql.DataSource;
import java.util.List;

/* DoD-C: MyBatis ORM over SQLite3 (#764 mybatis; ORM 只测 sqlite3). Programmatic
 * Configuration + annotation mappers (no XML). CRUD via mapper interface.
 * Exercises mybatis + sqlite-jdbc native (JNI) on starry. MYBATIS_DONE on pass. */
public class MyBatisDemo {
    public record User(Integer id, String name, Integer age) {}

    public interface UserMapper {
        @Update("CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, age INTEGER)")
        void createTable();
        @Insert("INSERT INTO users(name, age) VALUES(#{name}, #{age})")
        int insert(User u);
        @Select("SELECT id, name, age FROM users WHERE age >= #{minAge} ORDER BY id")
        List<User> findByMinAge(int minAge);
        @Select("SELECT COUNT(*) FROM users")
        int count();
    }

    public static void main(String[] args) {
        int ok = 0, fail = 0;
        DataSource ds = new PooledDataSource("org.sqlite.JDBC", "jdbc:sqlite:/tmp/dod-mybatis.db", null, null);
        Environment env = new Environment("dev", new JdbcTransactionFactory(), ds);
        Configuration config = new Configuration(env);
        config.addMapper(UserMapper.class);
        SqlSessionFactory factory = new SqlSessionFactoryBuilder().build(config);

        try (SqlSession session = factory.openSession()) {
            UserMapper m = session.getMapper(UserMapper.class);
            m.createTable();
            session.getConnection().createStatement().execute("DELETE FROM users");
            for (int i = 1; i <= 5; i++) m.insert(new User(null, "u" + i, 18 + i));
            session.commit();
            int c = m.count();
            if (c == 5) ok++; else { fail++; System.out.println("FAIL count=" + c); }
            List<User> adults = m.findByMinAge(21);
            if (adults.size() == 3 && adults.get(0).name().equals("u3")) ok++;
            else { fail++; System.out.println("FAIL findByMinAge=" + adults); }
        } catch (Throwable t) {
            String msg = t + " / " + (t.getCause() != null ? t.getCause() : "");
            System.out.println(msg.toLowerCase().contains("native") || msg.contains("UnsatisfiedLink")
                ? "MYBATIS_NATIVE_FAIL: " + msg : "MYBATIS_ERR: " + msg);
            return;
        }
        System.out.println("MYBATIS_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("MYBATIS_DONE");
    }
}
