# java-jse — J2SE library + JSE standard-library carpet

Industrial-grade, on-target test of a set of real J2SE third-party libraries and the JSE
standard library, run by **OpenJDK 17** on StarryOS across all four architectures
(x86_64 / aarch64 / riscv64 / loongarch64).

Each module is a self-contained carpet that exercises the full public API surface of one
library / JDK package — hundreds of exact-value assertions grounded in the official API
docs — and prints an anchored `*_DONE` marker only when its internal fail count is zero.
`run-jse.sh` runs every module and emits `TEST PASSED` only when all of them pass (no skip).
The suite is 22 modules / ~5650 assertions in total.

## Run

```
cargo xtask starry app qemu -t java-jse --arch x86_64
cargo xtask starry app qemu -t java-jse --arch aarch64
cargo xtask starry app qemu -t java-jse --arch riscv64
cargo xtask starry app qemu -t java-jse --arch loongarch64
```

`prebuild.sh` stages a full JDK17 (the per-arch musl build), the library jars and the
compiled `jse-suite.jar` into the per-app rootfs (grown to 2.5G); each module runs with
`-Xint -Xmx384m`. A developer who already has the JDK apks/tarball locally can point
`JAVA_DL_ROOT` at that cache to short-circuit the download.

## Coverage

J2SE third-party libraries (`assets/realdep-demo.jar`, `assets/jdbc-demo.jar`,
`assets/sqlite-demo.jar` provide the libraries on the classpath; carpet sources in
`programs/lib-carpets/`):

| module | library | marker |
|:--|:--|:--|
| jackson | jackson-databind (streaming / databind / tree / annotations / polymorphic / features) | `JACKSON_DONE` |
| guava | Guava (immutable collections, Multimap/BiMap/Table/Multiset/RangeSet, hashing, cache, …) | `GUAVA_DONE` |
| lang3 | commons-lang3 (StringUtils/ArrayUtils/NumberUtils/builders/tuple/reflection/…) | `LANG3_DONE` |
| h2 | H2 JDBC (DDL/DML/DQL/joins/window/transactions/types/constraints) + `org.h2.tools.*` CLI | `H2_DONE` |
| log | slf4j + logback (levels, parameterized, MDC, programmatic appenders, pattern, filtering) | `LOG_DONE` |
| sqlite | xerial sqlite-jdbc (full JDBC + PRAGMA / type affinity / FK / triggers / CTE / UPSERT / json1) | `SQLITEJDBC_DONE` |
| lombok | lombok annotations (@Data/@Builder/@Value/@With/@NonNull/@SneakyThrows/@Cleanup/@Slf4j/…) | `LOMBOK_DONE` |

JSE standard-library carpets (`programs/jse-suite/*.java`, compiled to `assets/jse-suite.jar`):
Algo, Concurrency, ConcurrencyDeep, Crypto, Extra, File, Jvm, LangUtil, Net, NioChannel,
Process, Stdlib, Syntax, Time, Xml — each covering the matching `java.*` package with its
own `*_DONE` marker.

The sqlite JNI `.so` for riscv64/loongarch64 is cross-compiled from xerial sqlite-jdbc
against musl (the upstream jar ships native only for glibc and for x86_64/aarch64 musl);
see `assets/native/`.
