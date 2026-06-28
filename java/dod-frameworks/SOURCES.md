# DoD P6 framework demos — sources, versions, feasibility, build/integrate/run

> Four #764 java17 frameworks: **wildfly / quarkus / ktor / trino**. Each = a minimal real
> app + correctness self-check + a DoD shell step (modeled on `openjdk17-0`'s pattern:
> run an app, `grep -q MARKER /tmp/x.out && echo "STEP ok=1" || ...`). Demo source trees live
> under `<下载缓存目录>/java-apps/dod/{ktor,quarkus,wildfly,trino}/`. Built fat jars (or lib
> dirs) go to `dod/jars/` then are staged into `rootfs-<arch>-java.img:/root/dod/`.
>
> This file documents what to fetch + how to build + how to integrate + how to run. All jars
> are JVM bytecode → arch-independent → build once, stage into all four rootfs images.

Pattern reference: `dod/undertow/` (embedded HTTP loopback self-test) and `dod/exposed/`
(host-compiled Kotlin → shaded jar). Run flags: `-Xint` + explicit `-Xms/-Xmx`.

---

## Feasibility summary

| framework | mode | footprint | realistic on starry? | notes |
|-----------|------|-----------|----------------------|-------|
| **ktor** | Netty engine, JVM | ~10–15 MB jar, modest heap | **YES** — like netty/jetty, already 4/4 √ | needs Kotlin (host-compiled jar, like exposed). Coroutines runtime already passes (coroutines-demo 4/4). |
| **quarkus** | **JVM mode** (NOT native) | uber-jar ~25–35 MB, ~150–256 MB heap | **YES (JVM mode)** — Vert.x + RESTEasy Reactive, comparable to spring-boot which is 4/4 √ | **native-image is INFEASIBLE** (GraalVM AOT needs PROT_EXEC codegen + huge build; starry JIT itself unstable, rcore-os/tgoskits#206). JVM mode is the only viable path. |
| **wildfly** | EE components in-process (RESTEasy on Undertow Servlet) | ~15–20 MB jar, ~256 MB heap | **YES (component path)** — same Jakarta EE deploy→serve path as the real server | **full WildFly distribution (~250 MB, forks Host Controller + server, mgmt+http+ajp ports, multi-min boot) is INFEASIBLE** under -Xint/single-core/emulated. Demo uses WildFly's actual JAX-RS impl (RESTEasy) + web subsystem (Undertow Servlet). |
| **trino** | in-process `LocalQueryRunner` + TPCH | many jars (~150–250 MB lib dir), ~512 MB–1 GB heap, big metaspace | **MARGINAL** — try x86_64 first; riscv64/loongarch likely too slow/large | **full Trino server (~1 GB, multi-GB heap, Discovery cluster, off-heap) is INFEASIBLE.** Even the embedded engine is the heaviest demo. **Trino 435 is the LAST Java-17 release** (436+ need Java 22). Mark as stretch / x86_64-only if others gate it out. |

---

## 1. ktor (Kotlin async web, Netty engine)  — REALISTIC

- **Source app**: `dod/ktor/src/main/kotlin/org/starry/dod/KtorDemo.kt` (+ `pom.xml`).
- **Marker**: `KTOR_DONE`. Binds `127.0.0.1:18082`, one GET route, loopback self-test (retry 60 s).
- **Versions / coords** (Maven Central):
  - `io.ktor:ktor-server-core-jvm:2.3.12`
  - `io.ktor:ktor-server-netty-jvm:2.3.12`
  - `org.jetbrains.kotlin:kotlin-stdlib:1.9.25`
  - `ch.qos.logback:logback-classic:1.4.14`
  - (Ktor 2.3.x is the last line comfortably on Kotlin 1.9 / Java 17; Ktor 3.x bumps Kotlin 2.x.)
- **Download size**: small (~12 MB of jars). **OK to fetch when building.**
- **Build (host, offline-warmed repo like the others)**:
  ```sh
  cd <下载缓存目录>/java-apps/dod/ktor
  <maven 安装目录>/bin/mvn -q \
      -Dmaven.repo.local=$HOME/.m2/repository package
  cp target/ktor-demo.jar ../jars/ktor-demo.jar
  ```
  (Kotlin compiled on host to dodge the on-starry kotlinc crash #237, exactly like exposed/springkt.)
- **host ground-truth** (must pass before staging): `java -jar target/ktor-demo.jar` → `KTOR_DONE`.

## 2. quarkus (JVM mode — NOT native)  — REALISTIC in JVM mode

- **Source app**: `dod/quarkus/src/main/java/org/starry/dod/{PingResource,SelfTest}.java`
  + `src/main/resources/application.properties` (binds `127.0.0.1:18083`) + `pom.xml`.
- **Marker**: `QUARKUS_DONE`. `SelfTest` observes `StartupEvent`, spawns a daemon thread that
  loopback-GETs `/ping`, then `Runtime.halt`.
- **Versions / coords**: Quarkus platform BOM `io.quarkus.platform:quarkus-bom:3.15.1`
  (3.15 LTS = last line with Java 17 baseline; 3.20+/3.25 move toward 21). Extensions:
  `io.quarkus:quarkus-resteasy-reactive`, `io.quarkus:quarkus-arc`.
- **Package type**: `uber-jar` (set via `quarkus.package.jar.type=uber-jar` in pom) so the DoD
  step is a plain `java -jar quarkus-demo-runner.jar`.
- **Download size**: medium (~40–60 MB of jars first time; Quarkus pulls many deps). **OK to
  fetch when building, but warm the repo carefully (Quarkus has a large dep graph).**
- **Build (host)**:
  ```sh
  cd <下载缓存目录>/java-apps/dod/quarkus
  <maven 安装目录>/bin/mvn -q \
      -Dmaven.repo.local=$HOME/.m2/repository package
  # uber-jar lands as target/quarkus-demo-runner.jar
  cp target/quarkus-demo-runner.jar ../jars/quarkus-demo.jar
  ```
- **host ground-truth**: `java -jar target/quarkus-demo-runner.jar` → `QUARKUS_DONE`.
- **NATIVE explicitly skipped**: do NOT attempt `-Pnative` / GraalVM native-image — it needs
  AOT code generation and a multi-GB build, and starry's JIT/PROT_EXEC path is itself unstable
  (rcore-os/tgoskits#206). JVM mode is the supported #764 target.

## 3. wildfly (Jakarta EE app server)  — REALISTIC via EE component path

- **Source app**: `dod/wildfly/src/main/java/org/starry/dod/{HelloResource,RestApp,WildflyDemo}.java`
  + `pom.xml`.
- **Marker**: `WILDFLY_DONE`. Deploys a JAX-RS app (`@ApplicationPath("/api")`, `/hello`
  resource) into a real Undertow Servlet deployment via RESTEasy's `HttpServlet30Dispatcher`,
  binds `127.0.0.1:18084`, loopback-GETs `/api/hello` (retry 90 s).
- **Why not the full distribution**: `wildfly-31.0.1.Final.zip` is ~250 MB, forks a Host
  Controller + standalone server, opens management(9990)+http(8080)+ajp ports, and boots in
  minutes even on bare metal — infeasible under -Xint/single-core/emulated/capped-RAM. This
  demo uses WildFly's **own EE runtime components**: RESTEasy (the JAX-RS impl WildFly ships,
  `org.jboss.resteasy`) on the Undertow **Servlet** container (the WildFly web subsystem).
  Same Jakarta EE deploy → start → serve → curl path, minus the app-server process machinery.
- **Versions / coords** (match WildFly 31's component versions, Jakarta EE 10, `jakarta.*`):
  - `org.jboss.resteasy:resteasy-core:6.2.9.Final`
  - `org.jboss.resteasy:resteasy-undertow:6.2.9.Final`
  - `io.undertow:undertow-core:2.3.18.Final`  (already used by undertow-demo √)
  - `io.undertow:undertow-servlet:2.3.18.Final`
- **Download size**: small (~18 MB). **OK to fetch when building.**
- **Build (host)**:
  ```sh
  cd <下载缓存目录>/java-apps/dod/wildfly
  <maven 安装目录>/bin/mvn -q \
      -Dmaven.repo.local=$HOME/.m2/repository package
  cp target/wildfly-demo.jar ../jars/wildfly-demo.jar
  ```
- **host ground-truth**: `java -jar target/wildfly-demo.jar` → `WILDFLY_DONE`.
- **Optional future stretch (document only, do not build now)**: if a full app-server boot is
  ever wanted, `wildfly-glow` / Galleon thin provisioning can trim WildFly to a `jaxrs-server`
  layer (~80 MB), but it still forks a server process + opens mgmt ports — keep as a separate,
  later, x86_64-only experiment, NOT part of the standard 4-arch DoD.

## 4. trino (distributed SQL engine, heavy JVM)  — MARGINAL, stretch

- **Source app**: `dod/trino/TrinoDemo.java` (+ `pom.xml`). Note: `sourceDirectory` is the
  module root (single flat .java), no `src/main/java` nesting.
- **Marker**: `TRINO_DONE`. Starts a single-node **in-process** `io.trino.testing.LocalQueryRunner`
  (full engine: parse→analyze→plan→optimize→execute, no HTTP cluster), installs the **TPCH**
  connector (pure compute, data generated on the fly, no storage), runs `SELECT 1`,
  `SELECT count(*) FROM region` (=5), `SELECT count(*) FROM nation WHERE regionkey=1` (=5).
- **Why not the server**: `trino-server-435.tar.gz` is ~1 GB unpacked, wants multi-GB heap +
  G1 + JIT + off-heap slices + Discovery/coordinator HTTP cluster — infeasible on starry.
  Even the embedded engine is the heaviest demo here.
- **Java-version ceiling**: **Trino 435 (Dec 2023) is the LAST release runnable on Java 17**;
  436+ require Java 22. Pin `io.trino:*:435`. (Do NOT pull a newer Trino — it will not even
  start on the openjdk17 in the rootfs.)
- **Versions / coords** (Maven Central):
  - `io.trino:trino-main:435` (+ its **test-jar** classifier — `LocalQueryRunner` lives there)
  - `io.trino:trino-testing:435`
  - `io.trino:trino-tpch:435`
  - `io.trino:trino-spi:435`
- **Download size**: **LARGE** — the resolved runtime+test classpath is ~150–250 MB across
  many jars (Airlift, Guice, Jackson, Slice, etc.). **DO NOT fetch now.** Document only.
- **Build / collect (host, LATER)** — produces a flat lib dir (no shading; Trino has many
  service-loader collisions + signed jars that break uber-jars):
  ```sh
  cd <下载缓存目录>/java-apps/dod/trino
  <maven 安装目录>/bin/mvn -q \
      -Dmaven.repo.local=$HOME/.m2/repository package
  # → target/classes/org/starry/dod/TrinoDemo.class  + target/trino-libs/*.jar
  rm -rf ../jars/trino-libs && mkdir -p ../jars/trino-libs
  cp target/trino-libs/*.jar ../jars/trino-libs/
  cp -r target/classes/* ../jars/trino-libs/      # demo class onto the same -cp
  ```
- **host ground-truth** (do this FIRST to confirm the embedded engine + tpch even fit a 1 GB
  heap before committing to staging):
  ```sh
  java -Xint -Xms256m -Xmx1024m -XX:MaxMetaspaceSize=512m \
       -cp 'target/trino-libs/*' org.starry.dod.TrinoDemo   # expect TRINO_DONE
  ```
  Trino also needs several `--add-opens`/`--add-exports` on Java 17 (it normally runs them via
  its launcher `jvm.config`). If the in-process run hits `InaccessibleObjectException`, add:
  `--add-opens=java.base/java.nio=ALL-UNNAMED --add-opens=java.base/sun.nio.ch=ALL-UNNAMED
  --add-opens=java.base/java.lang=ALL-UNNAMED --add-opens=java.base/java.lang.invoke=ALL-UNNAMED`
  to BOTH the host ground-truth and the starry DoD step. **Record the exact working flag set once host-verified.**
- **starry expectation**: try **x86_64 first**. If it does not fit/finish, gate trino to
  x86_64-only in the DoD step (skip on riscv64/loongarch) and document as a known limit — do
  NOT block the other three frameworks on it.

---

## Integrate

The four (small) jars + the trino lib dir are staged into each `rootfs-<arch>-java.img` under
`/root/dod/` (and `/root/dod/trino-libs/` for trino), exactly like the existing demo jars are
injected via debugfs/`jar uf`. Steps (per arch):

1. Build/copy jars: `dod/jars/{ktor,quarkus,wildfly}-demo.jar` and `dod/jars/trino-libs/`.
2. Inject into `/root/dod/` of each `rootfs-<arch>-java.img` (debugfs `-w`, like spring/netty).
   trino: also inject the whole `trino-libs/` tree to `/root/dod/trino-libs/`.
3. No image is rebuilt — only files are added, matching the existing flow.

## Run (the DoD shell step)

The DoD shell snippet to append to each `qemu-<arch>.toml` `shell_init_cmd` (before
`echo "BIGTEST_DONE"`) is in `dod/DOD-STEP-P6.sh` in this dir. It is identical across arches
EXCEPT trino, which is wrapped in an arch guard (x86_64-only by default until proven on the
others). Manual 4-arch run:
```sh
cd <tgoskits> && source .starry-env.sh
cargo xtask starry test qemu --arch x86_64      -g stress -c openjdk17-0
cargo xtask starry test qemu --arch aarch64     -g stress -c openjdk17-0
cargo xtask starry test qemu --arch loongarch64 -g stress -c openjdk17-0
cargo xtask starry test qemu --arch riscv64     -g stress -c openjdk17-0
```
Success markers (anchor grep `^EE-`): `EE-KTOR ok=1`, `EE-QUARKUS ok=1`, `EE-WILDFLY ok=1`,
and (x86_64) `EE-TRINO ok=1`.

Created: 2026-05-23.
