# CASE-NOTES — javatail (ktor / quarkus / wildfly40 / sdkman)

StarryOS × 4-arch stress-test assets for the "java tail" group of #764. The
integrator copies these into `tgoskits/test-suit/starryos/stress/`, builds the
rootfs images, and runs the 4-arch matrix.

All four cases share the same shape as the existing `ktor-0` / `openjdk17-0` stress
cases: a per-arch `qemu-<arch>.toml` (the harness) + a `prep-<app>-rootfs.sh`
(builds the rootfs via `debugfs -w` on the UNMOUNTED ext4 image — NEVER mount,
NEVER `sync`, per the WSL2 D-state-deadlock constraint).

---

## Files delivered

```
javatail/
  CASE-NOTES-javatail.md           <- this file
  ktor/    qemu-{x86_64,aarch64,riscv64,loongarch64}.toml + prep-ktor-rootfs.sh
  quarkus/ qemu-{x86_64,aarch64,riscv64,loongarch64}.toml + prep-quarkus-rootfs.sh
  wildfly/ qemu-{x86_64,aarch64,riscv64,loongarch64}.toml + prep-wildfly-rootfs.sh
  sdkman/  qemu-{x86_64,aarch64,riscv64,loongarch64}.toml + prep-sdkman-rootfs.sh
```

---

## Common design (all four)

* **Base image** = `rootfs-<arch>-java.img` (already in `tgoskits/tmp/axbuild/rootfs/`,
  ~3 GB, carries the 4-arch musl **OpenJDK17** JRE + maven/gradle/kotlin + the dod
  jars). Every case is a thin overlay on top — no new JRE needed. All four target
  apps are JDK-17-compatible (ktor 2.3.x, quarkus CLI runner, WildFly 40 supports
  JDK 17+, sdkman is bash + uses whatever JDK it manages — here just JDK17).
* **`-Xint`** forced on every JVM (JIT still unstable on starry, #206): ktor via
  the jar launch flag, quarkus via `JAVA_TOOL_OPTIONS`, wildfly via `JAVA_OPTS`.
* **debugfs prep**: copy base → `rootfs-<arch>-<app>.img`, (resize if needed),
  stage the payload tree, replay `mkdir`(depth-first)/`rm`+`write`/`symlink` into
  the unmounted image via `debugfs -w -f <cmds>`, then `e2fsck` + verify the key
  files landed with `debugfs -R stat`. Idempotent (re-runs `rm` then re-`write`).
  The inject-tree logic was validated on a throwaway 32 MB ext4 image (dirs,
  file-content, and symlink-target all round-tripped correctly).
* **Anti-false-positive** (a repeated source of error in earlier cases): the success token `<APP>_OK=1`
  appears ONLY in the final `printf '<APP>_OK=%s' "$V"` line + the `success_regex`
  — verified by grep across all 16 tomls (zero leakage into echo/comment/assignment).
  Internal bookkeeping uses `PASS`/`TOTAL`/`SRV_READY`, never the success token.
  `V=1` iff `PASS==TOTAL` AND `TOTAL==<expected count>` (and `SRV_READY==1` for the
  server cases). Version asserts are IN-GATE (wrong/old version → FAIL). Assertions
  check REAL output (status line + body / version string / banner), never exit-0.

---

## Image names (what prep produces / what each toml mounts)

| case    | image                              | base                  | resize |
|---------|------------------------------------|-----------------------|--------|
| ktor    | `rootfs-<arch>-ktor.img`           | `rootfs-<arch>-java.img` | none (base ~3G is ample) |
| quarkus | `rootfs-<arch>-quarkus.img`        | `rootfs-<arch>-java.img` | none |
| wildfly | `rootfs-<arch>-wildfly.img`        | `rootfs-<arch>-java.img` | 5 G (WildFly unpacks ~500 MB) |
| sdkman  | `rootfs-<arch>-sdkman.img`         | `rootfs-<arch>-java.img` | 4 G (bash closure + framework) |

debugfs commands/logs land in `/tmp/<app>-debugfs-<arch>.{cmds,log}`; stage trees in
`/tmp/<app>-stage-<arch>`.

---

## Per-app feasibility + JDK + what is asserted

### ktor — Kotlin/JVM async web server (Ktor 2.3.x, Netty engine)  → `KTOR_OK`
* **Payload**: the host-compiled fat jar `java/dod-frameworks/jars/ktor-demo.jar`
  (16 MB; Kotlin compiled on host to dodge on-starry kotlinc crash #237, same as
  exposed/springkt). Injected to `/root/ktor/ktor-demo.jar`. JDK17.
* **Run**: `java -Xint -jar ktor-demo.jar` binds `127.0.0.1:18082`, prints
  `KTOR_READY`, blocks. Harness drives 6 routes + 1 content-type proof over busybox
  `nc` (raw HTTP/1.0), asserts exact status line + body + the negotiated
  `application/json` ctype (genuine-stack proof of Ktor + kotlinx.serialization),
  then `kill`s the JVM. Gate: `SRV_READY && PASS==TOTAL && TOTAL==7`.
* **Feasibility**: FEASIBLE. Same class of workload as the existing `ktor-0` case
  (this re-uses its proven harness, extended with the ctype proof). Exercises Kotlin
  coroutines + Netty NIO event loop + Ktor routing over the starry net stack
  (#223/#225) on IPv4 loopback. `timeout=3000`.

### quarkus — CLI 3.35.4 (supersonic-subatomic-Java)  → `QUARKUS_OK`
* **Payload**: official `quarkus-cli-3.35.4.tar.gz` (20 MB) extracted to
  `/opt/quarkus-cli-3.35.4`. `bin/quarkus` is `#!/usr/bin/env sh` → execs
  `$JAVA_HOME/bin/java io.quarkus.cli.Main` from the runner jar. JDK17. Runs under
  busybox ash; `JAVA_TOOL_OPTIONS=-Xint`.
* **Asserts (all OFFLINE, host-verified on java 17)**:
  1. `quarkus --version` → prints exactly `3.35.4` (in-gate version assert; matched
     with `^3\.35\.4$`).
  2. `quarkus --help` → picocli usage banner (`Usage: quarkus`) + a real subcommand
     (`create`) — proves the full CLI runtime (picocli + Quarkus bootstrap) booted.
  3. `quarkus version` (subcommand) → `3.35.4` again, second independent path.
  Gate: `PASS==TOTAL && TOTAL==3`. `timeout=1800`.
* **Feasibility**: FEASIBLE for the CLI's self-contained offline surface.
  **INFEASIBLE OFFLINE (documented honestly, NOT in the gate)**: `quarkus create`
  (project scaffolding) and `quarkus dev`/`build` pull the Quarkus platform BOM +
  the extension registry from Maven Central + `registry.quarkus.io` over the
  network, which the offline starry guest cannot reach. To exercise a full
  Quarkus *app* on starry, the existing in-process route is `java/dod-frameworks/
  quarkus` (a pre-built RESTEasy uber-jar that self-tests on `:18083`) — that is a
  separate, complementary case, not this one. This case is specifically the **CLI
  binary** as requested.

### wildfly — 40.0.0.Final, full Jakarta EE 10 app server  → `WILDFLY_OK`
* **Payload**: official `wildfly-40.0.0.Final.tar.gz` (251 MB) extracted to
  `/opt/wildfly-40.0.0.Final`. `bin/standalone.sh` + `bin/common.sh` are POSIX
  `#!/bin/sh` (verified: no `[[`, no `local`/arrays/`function`-keyword) → run under
  the base image's **busybox ash; NO bash needed** for WildFly. JDK17 (WildFly 40
  supports 17+). standalone.conf default heap `-Xms64m -Xmx512m` kept; `-Xint`
  prepended via `JAVA_OPTS`.
* **Run + asserts**: boot `standalone.sh -b 127.0.0.1` in the background, capture
  `$PID`. Wait (≤600s; full EE boot under -Xint + emulated arch is slow) for the
  `WFLYSRV0025` "started in" banner (confirmed as the real boot-complete message
  code, `WFLYSRV0025: %s`, in `wildfly-server-*.jar`). Then probe `:8080` with
  busybox `nc` and assert:
  1. HTTP `200 OK` status line (Undertow web subsystem).
  2. welcome-content body contains `Welcome to WildFly` (the served
     `welcome-content/index.html` `<title>`).
  3. welcome body contains `Your WildFly instance is running.`
  4. boot log contains `40.0.0.Final` (in-gate version assert).
  Gate: `SRV_READY && PASS==TOTAL && TOTAL==4`.
* **SHUTDOWN = `kill $PID`** (key design decision):
  **NOT** `jboss-cli.sh :shutdown`. jboss-cli forks a SECOND full JVM (doubling the
  already-huge -Xint startup cost) and its management-port SSL/SASL handshake times
  out under the emulated single-core guest. A plain `kill` on the server PID is the
  cheap, reliable teardown; `pkill -f org.jboss.as.standalone` is added as
  an additional safeguard so the emulator frees cleanly.
* **Feasibility**: HEAVIEST case — a real app-server process + modular classloader +
  multi-minute boot under -Xint. Plausible but slow; `timeout=4500` (loongarch with
  8 GB RAM in the toml gives headroom). If a given arch can't finish boot in time it
  will report `WILDFLY_NOT_READY` + the tail of the boot log (honest FAIL, no
  fake-pass). Watch loongarch/riscv64 boot time first.

### sdkman — 5.23.0, the SDK manager for the JVM  → `SDKMAN_OK`
* **Payload**: (a) a real `/bin/bash` + its closure (curl/zip/unzip/readline/
  ncurses/…) overlaid from the 22-apk sdkman closure in `java/java-tail/sdkman/package/apks/<arch>/`
  (present for all 4 arches) — REQUIRED because `sdk` is a bash FUNCTION using bash
  arrays / `[[ ]]` that busybox ash cannot run; this is the case's distinguishing
  dependency. (b) the SDKMAN framework (from `java/java-tail/sdkman/package/zip-inspect/
  sdkman-5.23.0/`) into `/root/.sdkman/{bin,src,libexec,etc,var,…}` plus pre-seeded
  offline state: `var/version=5.23.0`, `var/platform`, `var/candidates` (the
  candidate-CSV cache that `sdkman-init.sh` reads at source time — without it every
  `sdk` call warns "var/candidates: No such file"), and `etc/config` with
  `sdkman_auto_selfupdate=false`, `sdkman_selfupdate_feature=false`,
  `sdkman_colour_enable=false`, `sdkman_offline_mode=true`.
* **Asserts (all OFFLINE, host-verified)**:
  0. `/bin/bash` runs on starry (`BASH_5…`).
  1. `sdk version` → prints exactly `SDKMAN 5.23.0` (in-gate version assert;
     reads `var/version`; matched `^SDKMAN 5\.23\.0$`).
  2. `sdk help` → `Usage: sdk <command> [candidate] [version]` banner + a real
     subcommand line (`install   or i`) — proves every `src/*.sh` module sourced and
     the function dispatcher works.
  3. candidate cache wired: after sourcing, `${#SDKMAN_CANDIDATES[@]}` is populated
     (82 candidates on host) and contains `java` — proves the offline candidate
     cache parsed into the array.
  Gate: `PASS==TOTAL && TOTAL==4`. `timeout=1800`.
* **`sdk` is a function**, so the harness invokes
  `/bin/bash -c 'source $SDKMAN_DIR/bin/sdkman-init.sh; sdk <cmd>'`
  (NOT `sdk` as a standalone binary — there isn't one).
* **Feasibility**: FEASIBLE offline for version + help + framework self-check.
  **NET NEEDED (documented honestly, NOT in the gate)**: `sdk install <candidate>
  [version]`, `sdk list`, `sdk update`, `sdk selfupdate` all hit `api.sdkman.io` /
  `broker.sdkman.io` + the candidate download URLs — unreachable from the offline
  starry guest. So the case proves "SDKMAN's runtime works on starry" (the bash
  framework loads + dispatches + reads its offline state), not live SDK installation.

---

## Integration steps

1. Create the four case dirs under `tgoskits/test-suit/starryos/stress/`:
   `ktor-0/`, `quarkus-0/`, `wildfly-0/`, `sdkman-0/` (mirror the existing
   `ktor-0` layout: `build-<target>.toml` × 4 at the case root + an inner
   `<case>-0/qemu-<arch>.toml` × 4). Copy the `build-*.toml` from the existing
   `ktor-0` case (they are app-agnostic: `features=["qemu"]`, `plat_dyn=false`,
   per-arch target triple) — these four cases all need the same JVM-on-starry
   build, identical to ktor-0/openjdk17-0.
2. Copy the `qemu-<arch>.toml` files from here into each case's inner dir.
3. Build the rootfs images: for each app run
   `bash <app>/prep-<app>-rootfs.sh <arch>` for arch in
   `x86_64 aarch64 riscv64 loongarch64`. Each reads `rootfs-<arch>-java.img` and
   writes `rootfs-<arch>-<app>.img` into `tgoskits/tmp/axbuild/rootfs/`. (debugfs,
   no mount/sync — safe on this WSL2 host.)
4. Run the 4-arch matrix; gate on `success_regex`. Suggested order by cost/risk:
   quarkus + sdkman (fast CLI, low risk) → ktor (server, medium) → wildfly
   (heaviest boot, watch loongarch/riscv64 timeout first).
5. Anti-false-positive recheck before any #764 tick: confirm `<APP>_OK=1` came
   from a genuine `PASS==TOTAL` (the harness echoes `<APP>_RESULT pass=N total=M`
   right before the gate — verify N==M==expected in the run log, not just the
   success_regex hit).

## Honest gaps / risks (no fake-pass)
* **quarkus** and **sdkman** offline gates deliberately exclude their network-bound
  features (`quarkus create`/`dev`, `sdk install`/`list`/`update`). These are the
  only fully-offline-deterministic surfaces; everything net-bound is documented as
  out-of-scope, not faked.
* **wildfly** boot time under -Xint on emulated riscv64/loongarch64 is the main
  schedule risk; if it exceeds `timeout=4500` the case honestly FAILs with the boot
  log tail (it does not emit `WILDFLY_OK=1`). Bumping the timeout or the toml `-m`
  is the lever if a real boot just needs more wall-clock.
* Host-side checks cover: TOML validity (16/16
  parse), prep-script `bash -n` (4/4), asset existence, debugfs inject round-trip on
  a tiny image, and host-JVM capture of the exact quarkus/sdkman version+help output
  strings the asserts match. Full on-starry 4-arch runs are performed during integration.
```
