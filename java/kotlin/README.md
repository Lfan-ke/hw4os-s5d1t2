# java/kotlin — Kotlin 2.0.21 language carpet (StarryOS)

Kotlin **2.0.21** language-level carpet. `KotlinCarpet.kt` is compiled on the host
with `kotlinc 2.0.21` (`-include-runtime` → a self-contained JAR bundling
kotlin-stdlib), and the resulting JAR runs on StarryOS's JVM (`/opt/jdk17`) across
architectures. This host-compile → run-on-target model matches the Go (`go126test`)
and HDL (testbins) deliveries: the heavy compiler stays host-side; the language's
emitted bytecode is verified executing on starry.

## Coverage (82 checks → `KOTLIN_OK`)

literals + string-template families · null safety (`?.`/`?:`/`!!`/NPE-catch/safe-call
chain/smart-cast) · scope functions (let/run/with/apply/also/takeIf) · data class
(copy/componentN/hashCode/toString) + `data object` · sealed interface + exhaustive
`when` (subjectless/range/multi-value) · enum · inheritance (abstract/open/override/
super/inner class) · type checks & casts (`is`/`!is`/`as`/`as?`/`in`/`!in`) ·
referential equality (`===`/`!==`) · extension functions & properties · generics
(bounded, `reified inline`, declaration-site variance `out`/`in`, star projection) ·
operator overloading (plus/times/unaryMinus/get/compareTo/contains/invoke) · `infix` ·
`@JvmInline value class` · lambdas/HOF/closures · `fun interface` (SAM) · `typealias` ·
method references · collections (map/filter/fold/reduce/groupBy/associate/partition/
flatten/zip/sumOf/sorted) · lazy sequences · `buildString`/`buildList` · `Pair`/`Triple`
· `Regex` · ranges/progressions (`..`/`..<`/`until`/`downTo`/`step`/char) · control-flow
loops (while/do-while/for/labeled break-continue) · delegation (`by lazy`/class
delegation/custom property delegate) · custom accessors (get/set + backing `field`) +
visibility · `object`/companion · `tailrec` · `lateinit` · reflection (`::class`/
javaClass) · annotations · **suspend/coroutines** (stdlib `kotlin.coroutines`,
`startCoroutine`/`suspendCoroutine`+resume) · exceptions (`try`/`catch`/`finally`,
`runCatching`) · stdlib math · threads (`Thread`/join).

## Run

The carpet JAR is staged into `rootfs-<arch>-jdk-multi.img` at `/root/KotlinCarpet.jar`
(the openjdk-multi rootfs already carries `/opt/jdk{17,21,23,25}`). The case:

```sh
cargo xtask starry test qemu --arch <arch> -g stress -c kotlin-0
```

runs `/opt/jdk17/bin/java -Xint -Xmx512m -jar /root/KotlinCarpet.jar` and gates on
`KOTLIN_GATE=1` (emitted only when the JAR prints `KOTLIN_OK`, i.e. all 82 checks pass).

## Validation

qemu-10 single-core starry: **aarch64 / riscv64 / loongarch64** all `KOTLIN_GATE=1`
+ `SUCCESS PATTERN MATCHED`; x86_64 via CI. Host golden: `P=32 F=0 KOTLIN_OK`.

## Note on `kotlinc` (compiler)

`kotlinc` itself is a large JVM application; running it on starry under `-Xint`/TCG is
impractically slow (same constraint that keeps Go's `go1.26.3` and the HDL toolchains
host-side). The compiler is therefore run host-side to produce the JAR; the Kotlin
**language/runtime** is what is verified executing on starry, 4-arch.
