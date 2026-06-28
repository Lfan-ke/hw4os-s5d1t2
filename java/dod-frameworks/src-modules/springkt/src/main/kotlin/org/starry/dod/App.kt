package org.starry.dod

import org.springframework.boot.CommandLineRunner
import org.springframework.boot.autoconfigure.SpringBootApplication
import org.springframework.boot.runApplication
import org.springframework.stereotype.Component

/* DoD-C / P6: Spring Boot (Kotlin) on starry. data class / 高阶函数 / 空安全 / copy + Spring DI.
 * SPRINGKT_DONE on pass. */
data class Item(val name: String, val qty: Int)

@Component
class Runner : CommandLineRunner {
    override fun run(vararg args: String?) {
        var ok = 0; var fail = 0
        val items = listOf(Item("a", 1), Item("b", 2), Item("c", 3))
        if (items.sumOf { it.qty } == 6) ok++ else { fail++; println("FAIL sum") }
        if (items.map { it.name }.filter { it > "a" } == listOf("b", "c")) ok++ else { fail++; println("FAIL filter") }
        val copy = items[0].copy(qty = 10)
        if (copy.qty == 10 && copy.name == "a") ok++ else { fail++; println("FAIL copy") }
        val maybe: String? = null
        if ((maybe?.length ?: -1) == -1) ok++ else { fail++; println("FAIL nullsafe") }
        println("SPRINGKT_RESULT ok=$ok fail=$fail")
        if (fail == 0 && ok == 4) println("SPRINGKT_DONE")
    }
}

@SpringBootApplication
class App

fun main(args: Array<String>) {
    runApplication<App>(*args)
}
