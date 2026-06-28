package org.starry.dod;

import java.util.List;
import org.springframework.data.jpa.repository.JpaRepository;

public interface AccountRepository extends JpaRepository<Account, Long> {
    List<Account> findByBalanceGreaterThanEqualOrderByBalanceDesc(long min);
    Account findByOwner(String owner);
}
