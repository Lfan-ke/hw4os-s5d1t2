package org.starry.dod;

import jakarta.persistence.Entity;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;

@Entity
public class Account {
    @Id @GeneratedValue(strategy = GenerationType.IDENTITY)
    Long id;
    String owner;
    long balance;

    public Account() {}
    public Account(String o, long b) { this.owner = o; this.balance = b; }
    public String getOwner() { return owner; }
    public long getBalance() { return balance; }
}
