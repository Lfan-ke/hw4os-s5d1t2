package org.starry.dod;

import java.util.List;
import org.springframework.boot.CommandLineRunner;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;

/* DoD-C / P5: Spring Data JPA × sqlite3 (spring-boot + hibernate + sqlite-jdbc musl JNI).
 * 全栈 ORM×框架集成: @Entity + JpaRepository + 派生查询 + @SpringBootApplication. SDJ_DONE on pass. */
@SpringBootApplication
public class SpringDataDemo implements CommandLineRunner {

    private final AccountRepository repo;
    public SpringDataDemo(AccountRepository repo) { this.repo = repo; }

    public static void main(String[] args) {
        SpringApplication.run(SpringDataDemo.class, args);
    }

    @Override
    public void run(String... args) {
        int ok = 0, fail = 0;
        try {
            repo.save(new Account("alice", 100));
            repo.save(new Account("bob", 50));
            repo.save(new Account("carol", 200));
            if (repo.count() == 3) ok++; else { fail++; System.out.println("FAIL count=" + repo.count()); }
            List<Account> rich = repo.findByBalanceGreaterThanEqualOrderByBalanceDesc(100);
            if (rich.size() == 2 && rich.get(0).getOwner().equals("carol")) ok++; else { fail++; System.out.println("FAIL derived size=" + rich.size()); }
            Account bob = repo.findByOwner("bob");
            bob.balance = 75;
            repo.save(bob);
            if (repo.findByOwner("bob").getBalance() == 75) ok++; else { fail++; System.out.println("FAIL update"); }
        } catch (Throwable t) {
            String m = String.valueOf(t.getMessage());
            System.out.println((m.toLowerCase().contains("native") || m.contains("UnsatisfiedLink")) ? "SDJ_NATIVE_FAIL: " + m : "SDJ_ERR: " + m);
            t.printStackTrace();
        }
        System.out.println("SDJ_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0 && ok == 3) System.out.println("SDJ_DONE");
        System.exit(fail == 0 ? 0 : 1);
    }
}
