package org.starry.dod

import org.jetbrains.exposed.sql.*
import org.jetbrains.exposed.sql.SqlExpressionBuilder.greaterEq
import org.jetbrains.exposed.sql.transactions.transaction

/* DoD-C / P6: Exposed (JetBrains Kotlin ORM/DSL) on sqlite3. host 编译 jar, starry 跑(规避
 * #237 编译器崩). 复用 sqlite-jdbc musl JNI. EXPOSED_DONE on pass. */
object Accounts : Table("accounts") {
    val id = integer("id").autoIncrement()
    val owner = varchar("owner", 32)
    val bal = integer("bal")
    override val primaryKey = PrimaryKey(id)
}

fun main() {
    var ok = 0; var fail = 0
    fun chk(c: Boolean, m: String) { if (c) ok++ else { fail++; println("FAIL $m") } }
    try {
        Database.connect("jdbc:sqlite:/tmp/exposed.db", driver = "org.sqlite.JDBC")
        transaction {
            SchemaUtils.drop(Accounts); SchemaUtils.create(Accounts)
            Accounts.insert { it[owner] = "alice"; it[bal] = 100 }
            Accounts.insert { it[owner] = "bob"; it[bal] = 50 }
            Accounts.insert { it[owner] = "carol"; it[bal] = 200 }
            chk(Accounts.selectAll().count() == 3L, "count==3")
            val rich = Accounts.selectAll().where { Accounts.bal greaterEq 100 }
                .orderBy(Accounts.bal to SortOrder.DESC).map { it[Accounts.owner] }
            chk(rich == listOf("carol", "alice"), "ordered query = $rich")
            Accounts.update({ Accounts.owner eq "bob" }) { it[bal] = 75 }
            val bobBal = Accounts.selectAll().where { Accounts.owner eq "bob" }.single()[Accounts.bal]
            chk(bobBal == 75, "update bob=$bobBal")
        }
    } catch (t: Throwable) {
        val m = t.message ?: ""
        println(if (m.contains("native", true) || m.contains("UnsatisfiedLink")) "EXPOSED_NATIVE_FAIL: $m" else "EXPOSED_ERR: $t")
        t.printStackTrace()
    }
    println("EXPOSED_RESULT ok=$ok fail=$fail")
    if (fail == 0 && ok == 3) println("EXPOSED_DONE")
    System.exit(if (fail == 0) 0 else 1)
}
