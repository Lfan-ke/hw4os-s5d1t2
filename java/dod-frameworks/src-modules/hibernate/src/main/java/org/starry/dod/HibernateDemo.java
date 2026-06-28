package org.starry.dod;

import jakarta.persistence.Entity;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;
import org.hibernate.SessionFactory;
import org.hibernate.boot.MetadataSources;
import org.hibernate.boot.registry.StandardServiceRegistryBuilder;

import java.util.List;

/* DoD-C: Hibernate ORM / JPA (sqlite3 后端, 复用 sqlite-jdbc musl JNI). 程序化 SessionFactory
 * + @Entity + hbm2ddl create-drop + persist/HQL query. HIBERNATE_DONE on pass. */
public class HibernateDemo {


    public static void main(String[] args) {
        int ok = 0, fail = 0;
        var reg = new StandardServiceRegistryBuilder()
                .applySetting("hibernate.connection.url", "jdbc:sqlite:/tmp/hib.db")
                .applySetting("hibernate.connection.driver_class", "org.sqlite.JDBC")
                .applySetting("hibernate.dialect", "org.hibernate.community.dialect.SQLiteDialect")
                .applySetting("hibernate.hbm2ddl.auto", "create-drop")
                .applySetting("hibernate.show_sql", "false")
                .build();
        try {
            SessionFactory sf = new MetadataSources(reg).addAnnotatedClass(Person.class).buildMetadata().buildSessionFactory();
            sf.inTransaction(s -> {
                s.persist(new Person("Alice", 30));
                s.persist(new Person("Bob", 25));
                s.persist(new Person("Carol", 40));
            });
            Long cnt = sf.fromSession(s -> s.createQuery("select count(p) from Person p", Long.class).getSingleResult());
            if (cnt == 3) ok++; else { fail++; System.out.println("FAIL count=" + cnt); }
            List<Person> adults = sf.fromSession(s -> s.createQuery("from Person p where p.age >= 30 order by p.age", Person.class).getResultList());
            if (adults.size() == 2 && adults.get(0).name.equals("Alice")) ok++; else { fail++; System.out.println("FAIL query size=" + adults.size()); }
            sf.inTransaction(s -> {
                Person p = s.createQuery("from Person where name='Bob'", Person.class).getSingleResult();
                p.age = 26;
            });
            Integer bobAge = sf.fromSession(s -> s.createQuery("select age from Person where name='Bob'", Integer.class).getSingleResult());
            if (bobAge == 26) ok++; else { fail++; System.out.println("FAIL update age=" + bobAge); }
            sf.close();
        } catch (Throwable t) {
            String m = String.valueOf(t.getMessage());
            System.out.println((m.toLowerCase().contains("native") || m.contains("UnsatisfiedLink")) ? "HIBERNATE_NATIVE_FAIL: " + m : "HIBERNATE_ERR: " + m);
            t.printStackTrace();
        }
        System.out.println("HIBERNATE_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0 && ok == 3) System.out.println("HIBERNATE_DONE");
    }
}
