package org.starry.dod

import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.*
import java.util.concurrent.atomic.AtomicInteger

/* DoD-C / P7: Kotlin 协程运行时 (kotlinx-coroutines). 在 host 编译, starry 跑 jar(规避 #237
 * 编译器崩, 这测协程 runtime). launch/async/await + Channel + Flow + 结构化并发 + Dispatchers
 * 并行. COROUTINES_DONE on pass. */
fun main() = runBlocking {
    var ok = 0; var fail = 0
    fun chk(c: Boolean, m: String) { if (c) ok++ else { fail++; println("FAIL $m") } }

    // 1. async/await 并行求和
    val a = async { delay(20); (1..1000).sum() }
    val b = async { delay(20); (1001..2000).sum() }
    chk(a.await() + b.await() == (1..2000).sum(), "async/await parallel sum")

    // 2. 1000 个 launch 并发自增 (结构化并发, 全部 join)
    val counter = AtomicInteger(0)
    coroutineScope { repeat(1000) { launch { delay(1); counter.incrementAndGet() } } }
    chk(counter.get() == 1000, "1000 launch structured = ${counter.get()}")

    // 3. Channel 生产/消费
    val ch = Channel<Int>(50)
    val prod = launch { for (i in 1..200) ch.send(i); ch.close() }
    var chSum = 0
    for (x in ch) chSum += x
    prod.join()
    chk(chSum == (1..200).sum(), "channel sum=$chSum")

    // 4. Flow map/filter/reduce
    val flowSum = (1..100).asFlow().map { it * 2 }.filter { it % 3 == 0 }.toList().sum()
    chk(flowSum == (1..100).map { it * 2 }.filter { it % 3 == 0 }.sum(), "flow pipeline")

    // 5. Dispatchers.Default 并行计算 (多线程派发)
    val parSum = withContext(Dispatchers.Default) {
        (0 until 8).map { k -> async { (k * 100_000L until (k + 1) * 100_000L).sum() } }.awaitAll().sum()
    }
    chk(parSum == (0L until 800_000L).sum(), "Dispatchers.Default parallel")

    // 6. withTimeout 正常完成 + 超时捕获
    val tv = withTimeoutOrNull(5000) { delay(10); 42 }
    chk(tv == 42, "withTimeout completes")
    val to = withTimeoutOrNull(30) { delay(5000); 1 }
    chk(to == null, "withTimeout times out -> null")

    println("COROUTINES_RESULT ok=$ok fail=$fail")
    if (fail == 0 && ok == 7) println("COROUTINES_DONE")
}
