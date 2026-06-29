# java-web — JEE framework carpet

Industrial-grade, on-target test of a set of real JEE/JVM frameworks, run by **OpenJDK 17**
on StarryOS across all four architectures (x86_64 / aarch64 / riscv64 / loongarch64).

Each module is a self-contained carpet that exercises one framework's public API surface
(hundreds of exact-value assertions; HTTP servers are driven over real IPv4 loopback with
`HttpURLConnection`, ORMs run against an in-memory database). A module prints an anchored
`*_DONE` marker only when its internal fail count is zero; `run-jweb.sh` runs every module
and emits `TEST PASSED` only when all of them pass (no skip).

## Run

```
cargo xtask starry app qemu -t java-web --arch x86_64
cargo xtask starry app qemu -t java-web --arch aarch64
cargo xtask starry app qemu -t java-web --arch riscv64
cargo xtask starry app qemu -t java-web --arch loongarch64
```

`prebuild.sh` stages a full JDK17 (the per-arch musl build) and the framework carpet jars
into the per-app rootfs (grown to 2.5G); each module runs with `-Xint -Xmx512m`. A developer
who already has the JDK apks/tarball locally can point `JAVA_DL_ROOT` at that cache.

## Coverage

| module | framework | dimension | marker |
|:--|:--|:--|:--|
| jetty | Eclipse Jetty | embedded HTTP server (handlers / routing / methods / status-body-header assertions over loopback) | `JETTY_DONE` |
| netty | Netty 4.x | ByteBuf, EmbeddedChannel codec/handler unit tests, real loopback TCP echo + HTTP-codec server | `NETTY_DONE` |
| mybatis | MyBatis | SqlSessionFactory / mappers / annotations / dynamic SQL / batch / transactions over an in-memory DB | `MYBATIS_DONE` |
| hibernate | Hibernate / JPA | SessionFactory / entities / CRUD / HQL-JPQL / Criteria / relationships / paging over an in-memory DB | `HIBERNATE_DONE` |
| r2dbc | R2DBC | reactive ConnectionFactory / Statement / Result, deterministic subscription, transactions | `R2DBC_DONE` |
| war | Jakarta Servlet | a real `.war` (servlet + `web.xml`) deployed into an embedded Jetty `WebAppContext`, hit over loopback HTTP | `WAR_DONE` |

The carpet sources live in `programs/carpets/`; the framework libraries are provided by the
arch-independent fat jars in `assets/`. MyBatis and Hibernate run over sqlite-jdbc 3.46.1.3;
its jar bundles a musl native only for x86_64/aarch64, so `prebuild.sh` stages a cross-built
musl JNI (`assets/native/libsqlitejdbc-<arch>.so`) for riscv64/loongarch64 and `run-jweb.sh`
points `org.sqlite.lib.path` at it.
