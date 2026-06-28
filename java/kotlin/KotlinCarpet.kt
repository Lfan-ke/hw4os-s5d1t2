// KotlinCarpet.kt — comprehensive Kotlin 2.0.21 language carpet for StarryOS.
// Compiled on host with kotlinc 2.0.21 (-include-runtime) -> a self-contained JAR run on starry's
// JVM, verifying the Kotlin language/stdlib executes correctly across arches. stdlib-only (no
// kotlin-reflect, no kotlinx-coroutines) so the -include-runtime JAR is fully self-contained.
// Prints KOTLIN_OK iff every check passes.
import kotlin.math.sqrt
import kotlin.coroutines.*

var P = 0; var F = 0
fun chk(name: String, cond: Boolean) { if (cond) P++ else { F++; println("  FAIL $name") } }

// ---- type declarations ----
data class Point(val x: Int, val y: Int) { fun dist() = sqrt((x * x + y * y).toDouble()) }
sealed interface Shape
data class Circle(val r: Double) : Shape
data class Rect(val w: Double, val h: Double) : Shape
data object UnitShape : Shape                                  // sealed + data object
fun area(s: Shape): Double = when (s) {                         // exhaustive, no else
    is Circle -> Math.PI * s.r * s.r
    is Rect -> s.w * s.h
    UnitShape -> 0.0
}
enum class Dir { N, E, S, W; fun opp() = when (this) { N -> S; S -> N; E -> W; W -> E } }
// inheritance: abstract / open / override / inner
abstract class Animal(val name: String) { abstract fun sound(): String; open fun greet() = "I am $name" }
open class Dog(name: String) : Animal(name) { override fun sound() = "woof"; override fun greet() = "Dog ${super.greet()}" }
class Puppy(name: String) : Dog(name) { inner class Tag { fun owner() = name } }    // inner class
// fun interface (SAM) / typealias / annotation class / lateinit
fun interface IntOp { fun apply(a: Int, b: Int): Int }
typealias IntPredicate = (Int) -> Boolean
@Target(AnnotationTarget.FUNCTION) annotation class Marked(val tag: String)
@Marked("x") fun marked() = 1
// extensions
fun String.shout() = uppercase() + "!"
val <T> List<T>.secondOrNull: T? get() = if (size >= 2) this[1] else null
// generics: bounded + reified inline + variance (out producer / in consumer) + star
inline fun <reified T> isType(x: Any?): Boolean = x is T
fun <T : Comparable<T>> maxOf3(a: T, b: T, c: T): T = maxOf(a, maxOf(b, c))
class Box<out T>(val v: T)                                      // declaration-site covariance
fun consume(b: Box<*>): String = b.v.toString()                // star projection
fun interface Consumer<in T> { fun take(t: T) }
// operator overloading (multiple conventions)
data class V2(val x: Int, val y: Int) {
    operator fun plus(o: V2) = V2(x + o.x, y + o.y)
    operator fun times(k: Int) = V2(x * k, y * k)
    operator fun unaryMinus() = V2(-x, -y)
    operator fun get(i: Int) = if (i == 0) x else y
    operator fun compareTo(o: V2) = (x * x + y * y) - (o.x * o.x + o.y * o.y)
    operator fun contains(c: Int) = c == x || c == y
    operator fun invoke() = x + y
}
infix fun Int.pow2(times: Int): Int { var r = 1; repeat(times) { r *= this }; return r }
@JvmInline value class Meters(val v: Double)
// delegation: lazy + class delegation + custom property delegate
val lazyVal: String by lazy { "computed" }
interface Greeter { fun hi(): String }
class GreeterImpl : Greeter { override fun hi() = "hello" }
class GDeleg(g: Greeter) : Greeter by g                         // class delegation
class UpperDelegate { operator fun getValue(t: Any?, p: Any?) = "VAL" }  // custom delegate
val delegated: String by UpperDelegate()
// custom accessors + backing field + visibility
class Temp(c: Double) {
    var celsius: Double = c
        get() = field
        set(value) { field = if (value < -273.15) -273.15 else value }
    val fahrenheit: Double get() = celsius * 9 / 5 + 32
    internal val tag = "t"
}
// object + companion + tailrec + lateinit
class Counter private constructor(val n: Int) { companion object { fun of(n: Int) = Counter(n) } }
tailrec fun fact(n: Long, acc: Long = 1): Long = if (n <= 1) acc else fact(n - 1, acc * n)
class Late { lateinit var s: String; fun init() { s = "set" } }
// suspend / coroutines (kotlin.coroutines stdlib only — no kotlinx)
suspend fun suspendAdd(a: Int, b: Int): Int = a + b
suspend fun suspendId(x: Int): Int = suspendCoroutine { c -> c.resume(x) }   // real suspend+resume
fun <T> runSuspend(block: suspend () -> T): T {
    var res: Result<T>? = null
    block.startCoroutine(Continuation(EmptyCoroutineContext) { res = it })
    return res!!.getOrThrow()
}

fun main() {
    // ---- literals + string templates (families) ----
    val name = "starry"
    chk("template", "hi $name ${1 + 1}" == "hi starry 2" && "len=${name.length}" == "len=6")
    chk("raw_string", """a${'$'}b
c""".lines().size == 2)
    chk("num_literals", 1_000_000 == 1000000 && 0xFF == 255 && 0b1010 == 10 && 1_000L == 1000L && 3.14e2 == 314.0 && 'A'.code == 65)
    chk("char_unicode", 'A' == 'A' && "\t\n".length == 2)
    // ---- null safety (9 mechanisms) ----
    val s: String? = null; chk("nullsafe_qmark", (s?.length ?: -1) == -1 && (s ?: "d") == "d")
    val t: String? = "abc"; chk("smartcast", if (t != null) t.length == 3 else false)
    t?.let { chk("scope_let", it == "abc") }
    chk("not_null_assert", run { val x: String? = "y"; x!!.length == 1 })            // !!
    chk("npe_caught", try { val n: String? = null; n!!.length; false } catch (e: NullPointerException) { true })
    chk("safe_call_chain", (null as String?)?.uppercase()?.length == null)
    chk("elvis_throw", run { val v: Int? = 5; (v ?: error("x")) == 5 })
    // ---- scope functions: let/run/with/apply/also ----
    chk("scope_run", "ab".run { length + 1 } == 3)
    chk("scope_with", with(StringBuilder()) { append("a"); append("b"); toString() } == "ab")
    chk("scope_apply", StringBuilder().apply { append("x"); append("y") }.toString() == "xy")
    chk("scope_also", run { var c = 0; listOf(1, 2).also { c = it.size }; c == 2 })
    chk("takeIf_unless", (5.takeIf { it > 3 } == 5) && (5.takeUnless { it > 3 } == null))
    // ---- data class (copy / componentN / equals / toString / hashCode) ----
    val p = Point(1, 2); val p2 = p.copy(y = 5); val (px, py) = p
    chk("dataclass", p == Point(1, 2) && p2 == Point(1, 5) && px == 1 && py == 2)
    chk("dataclass_extra", p.toString() == "Point(x=1, y=2)" && p.hashCode() == Point(1, 2).hashCode() && p.component2() == 2)
    chk("data_object", UnitShape == UnitShape && UnitShape.toString() == "UnitShape")
    // ---- sealed + when (forms) ----
    chk("sealed_when", area(Rect(2.0, 3.0)) == 6.0 && area(Circle(1.0)) > 3.0 && area(UnitShape) == 0.0)
    chk("when_subjectless", run { val n = 7; when { n < 0 -> "neg"; n == 0 -> "z"; else -> "pos" } == "pos" })
    chk("when_multi_range", run { val n = 5; when (n) { 1, 2, 3 -> "lo"; in 4..6 -> "mid"; else -> "hi" } == "mid" })
    chk("when_expr_assign", run { val r = when (3 % 2) { 0 -> "even"; else -> "odd" }; r == "odd" })
    chk("enum", Dir.N.opp() == Dir.S && Dir.valueOf("E") == Dir.E && Dir.entries.size == 4 && Dir.N.ordinal == 0)
    // ---- inheritance / polymorphism ----
    val d: Animal = Puppy("rex"); chk("inheritance", d.sound() == "woof" && d.greet() == "Dog I am rex")
    chk("inner_class", Puppy("a").Tag().owner() == "a")
    // ---- type checks & casts: is / !is / as / as? / in / !in ----
    val any: Any = "hello"
    chk("is_smartcast", any is String && (any as String).length == 5)
    chk("safe_cast", (any as? Int) == null && (any as? String) == "hello")
    chk("not_is", 5 !is String)
    chk("in_range", (5 in 1..10) && (15 !in 1..10) && ('c' in 'a'..'z'))
    // ---- referential vs structural equality ----
    val a1 = listOf(1, 2); val a2 = listOf(1, 2); val a3 = a1
    chk("equality_ref", a1 == a2 && a1 !== a2 && a1 === a3 && (a1 !== a2))
    // ---- extensions / generics ----
    chk("ext_fun", "hi".shout() == "HI!"); chk("ext_prop", listOf(1, 2, 3).secondOrNull == 2 && listOf(1).secondOrNull == null)
    chk("reified", isType<String>("x") && !isType<Int>("x")); chk("bounded", maxOf3(3, 9, 5) == 9 && maxOf3("a", "z", "m") == "z")
    chk("variance", Box(42).v == 42 && consume(Box("hi")) == "hi")
    // ---- operator overloading breadth ----
    val v = V2(1, 2)
    chk("op_plus_times", (v + V2(2, 3)) == V2(3, 5) && (v * 3) == V2(3, 6))
    chk("op_unary_get", (-v) == V2(-1, -2) && v[0] == 1 && v[1] == 2)
    chk("op_compare_in", (V2(3, 4) > V2(1, 1)) && (1 in v) && (9 !in v))
    chk("op_invoke", v() == 3); chk("infix", (2 pow2 10) == 1024); chk("valueclass", Meters(3.0).v == 3.0)
    // ---- lambdas / HOF / closures / fun interface / typealias ----
    val add: (Int, Int) -> Int = { x, y -> x + y }; chk("lambda", add(2, 3) == 5)
    var captured = 10; val inc = { captured++ }; inc(); inc(); chk("closure", captured == 12)
    chk("hof", listOf(1, 2, 3).map { it * 2 }.filter { it > 2 } == listOf(4, 6))
    chk("fun_interface", IntOp { x, y -> x * y }.apply(3, 4) == 12)
    val isEven: IntPredicate = { it % 2 == 0 }; chk("typealias", isEven(4) && !isEven(3))
    chk("method_ref", listOf("a", "bb").map(String::length) == listOf(1, 2))
    // ---- collections / stdlib ----
    val m = mapOf("a" to 1, "b" to 2); chk("map", m["a"] == 1 && m.getValue("b") == 2 && m.keys.sorted() == listOf("a", "b"))
    chk("fold", (1..5).fold(0) { acc, i -> acc + i } == 15 && (1..4).reduce { a, b -> a * b } == 24)
    chk("groupBy", listOf(1, 2, 3, 4).groupBy { it % 2 } == mapOf(1 to listOf(1, 3), 0 to listOf(2, 4)))
    chk("associate", listOf("a", "bb").associateWith { it.length } == mapOf("a" to 1, "bb" to 2))
    chk("collops", listOf(3, 1, 2).sorted() == listOf(1, 2, 3) && listOf(1, 2, 3).sumOf { it } == 6 && listOf(1, 2, 3, 4).partition { it % 2 == 0 } == Pair(listOf(2, 4), listOf(1, 3)))
    chk("flatmap_zip", listOf(listOf(1), listOf(2, 3)).flatten() == listOf(1, 2, 3) && listOf(1, 2).zip(listOf("a", "b")) == listOf(1 to "a", 2 to "b"))
    chk("sequence", generateSequence(1) { it + 1 }.map { it * it }.take(3).toList() == listOf(1, 4, 9))
    chk("build", buildString { append("a"); append(1) } == "a1" && buildList { add(1); add(2) } == listOf(1, 2))
    chk("pair_triple", Pair(1, "a").first == 1 && Triple(1, 2, 3).third == 3)
    chk("regex", Regex("\\d+").findAll("a1b22").map { it.value }.toList() == listOf("1", "22") && "a-b".replace(Regex("-"), "_") == "a_b")
    chk("strops", "a,b,c".split(",") == listOf("a", "b", "c") && "  x ".trim() == "x" && "abc".substring(1) == "bc" && "Hi".repeat(2) == "HiHi")
    // ---- ranges / progressions ----
    chk("range_step", (1..10 step 2).toList() == listOf(1, 3, 5, 7, 9) && (5 downTo 1).first() == 5)
    chk("range_until", (0..<5).toList() == listOf(0, 1, 2, 3, 4) && (0 until 3).toList() == listOf(0, 1, 2))
    chk("char_range", ('a'..'e').toList() == listOf('a', 'b', 'c', 'd', 'e'))
    // ---- control-flow loops ----
    chk("while_loop", run { var i = 0; var s = 0; while (i < 5) { s += i; i++ }; s == 10 })
    chk("dowhile_loop", run { var i = 0; do { i++ } while (i < 3); i == 3 })
    chk("for_loops", run { var s = 0; for (i in 1..3) s += i; for (c in listOf(10, 20)) s += c; s == 36 })
    chk("labeled", run { var found = -1; outer@ for (i in 0..3) for (j in 0..3) if (i + j == 4) { found = i * 10 + j; break@outer }; found == 13 })
    // ---- delegation / accessors / lateinit ----
    chk("lazy", lazyVal == "computed"); chk("class_delegation", GDeleg(GreeterImpl()).hi() == "hello"); chk("custom_delegate", delegated == "VAL")
    val temp = Temp(25.0); temp.celsius = -300.0; chk("accessors", temp.celsius == -273.15 && Temp(0.0).fahrenheit == 32.0 && temp.tag == "t")
    chk("lateinit", Late().apply { init() }.s == "set")
    chk("companion", Counter.of(7).n == 7); chk("tailrec", fact(10) == 3628800L)
    // ---- reflection (stdlib KClass, no kotlin-reflect) ----
    chk("reflection", Point(1, 2)::class.simpleName == "Point" && String::class.simpleName == "String" && "x".javaClass.simpleName == "String")
    chk("annotation", marked() == 1 && ::marked.name == "marked")
    // ---- suspend / coroutines (stdlib kotlin.coroutines) ----
    chk("suspend_basic", runSuspend { suspendAdd(2, 3) } == 5)
    chk("suspend_resume", runSuspend { suspendId(7) + suspendAdd(1, 1) } == 9)
    // ---- exceptions ----
    val r = try { "12a".toInt() } catch (e: NumberFormatException) { -1 }; chk("exception", r == -1)
    chk("runCatching", runCatching { error("boom") }.isFailure && runCatching { 5 }.getOrNull() == 5)
    chk("finally", run { var f = false; try { } finally { f = true }; f })
    // ---- stdlib math + concurrency ----
    chk("math", sqrt(16.0) == 4.0 && maxOf(1, 2, 3) == 3 && 7 % 3 == 1 && Point(3, 4).dist() == 5.0)
    val box = IntArray(1); val th = Thread { box[0] = 42 }; th.start(); th.join(); chk("thread", box[0] == 42)

    println("KOTLIN_CARPET: P=$P F=$F")
    if (F == 0) println("KOTLIN_OK") else println("KOTLIN_FAIL")
}
