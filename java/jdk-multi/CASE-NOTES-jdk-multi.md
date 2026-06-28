# CASE-NOTES — `openjdk-multi-0` StarryOS stress case (JDK 17/21/23/25 multi-version)

Satisfies rcore-os/tgoskits#764 parent item:
`jdk17+ <!-- openjdk - 17 21 23 25 - update-alternatives -->`

A single StarryOS stress case that, on each of 4 arches, runs a **version-specific
language/stdlib/syntax feature test on EACH of JDK 17, 21, 23, 25** (asserting the
running JVM is actually that major), then exercises **Java version switching** two
ways (update-alternatives-style JAVA_HOME+symlink swap, and an offline sdkman-style
candidate switch). Aggregate gate `JDK_MULTI_OK=1` fires ONLY when every sub-suite
passed — no silent pass.

---

## 1. Deliverables (this dir)

| file | role |
|------|------|
| `prep-jdk-multi-rootfs.sh` | builds `rootfs-<arch>-jdk-multi.img` via `debugfs -w` (no mount/sync); stages all 4 JDKs under `/opt/jdk{17,21,23,25}` + sources + sdkman candidates |
| `qemu-{x86_64,aarch64,riscv64,loongarch64}.toml` | 4-arch qemu cases; identical `shell_init_cmd` harness, per-arch machine/mem |
| `programs/Jdk17Features.java` | JDK17 features |
| `programs/Jdk21Features.java` | JDK21 features |
| `programs/Jdk23Features.java` | JDK23 features (preview) |
| `programs/Jdk25Features.java` | JDK25 features (preview) |
| `host-ref/*.out` | expected host outputs (each ends with `JDKxx_OK`) |
| `host-ref/harness-host-sim.sh` | host simulation of the full toml gate (NOT shipped into image) |
| `host-ref/toml-body-host.out` | proof the exact toml `shell_init_cmd` body yields `JDK_MULTI_OK=1` on host |

---

## 2. Per-version features tested (each prints `JDKxx_OK` only if ALL asserts pass)

Every test first does the **version red-line**: `Runtime.version().feature()` must equal
the expected major (17/21/23/25), else it throws — a wrong-version JDK is a hard FAIL,
never a silent pass. (Belt-and-braces with the switch sub-test which parses `java -version`.)

- **JDK17** (LTS, all final): records (components/equals/hashCode/accessors/toString) ·
  sealed interface + permitted records · pattern matching for `instanceof` · text blocks ·
  switch expressions (arrow + `yield`) · `Stream.toList()` (+ immutability).
- **JDK21** (LTS, all final): **virtual threads** (`Thread.ofVirtual` + `Executors.newVirtualThreadPerTaskExecutor`,
  fan-out 1000 tasks) · record patterns (nested deconstruction) · pattern matching for
  `switch` incl. **guarded** patterns (`when`) and `null`/`default` labels · **sequenced
  collections** (`SequencedCollection.getFirst/getLast/addFirst/addLast/reversed`, `LinkedHashSet`) ·
  `Math.clamp` (int + double).
- **JDK23** (preview features gated `--enable-preview --source 23`): **Flexible Constructor
  Bodies** (JEP 482 — statements/validation before `super()`) · **Stream Gatherers**
  (JEP 473 — `Stream::gather` + `Gatherers.windowFixed` / `Gatherers.fold`) · plus STABLE
  nested record patterns in switch + `Stream.mapMulti`.
- **JDK25** (LTS): **Scoped Values** (JEP 506, FINAL — `ScopedValue.where(...).call(...)`,
  unbound-outside-scope, nested rebind) · **Module Import Declarations** (JEP 511, FINAL —
  `import module java.base` / `import module java.management`, using unqualified
  `List`/`Map`/`Collections`/`ManagementFactory`) · **Compact Object Headers** (JEP 519,
  FINAL product flag — the harness runs the program a 2nd time with
  `-XX:+UseCompactObjectHeaders` and the program asserts the flag is present + object
  identity/hashCode intact) · **Stable Values** (JEP 502, preview — `StableValue.of()` +
  `orElseSet` compute-once, supplier must not run twice).

### Preview-feature notes
- JDK23's two headline features (flexible ctor bodies, Gatherers) are **preview in 23**,
  so the JDK23 test runs under `--enable-preview --source 23`. Verified on host: without
  preview the compiler rejects them; with preview they run.
- JDK25's StableValue is **preview**; ScopedValue + module imports + compact-headers are
  **final**. The whole JDK25 test runs under `--enable-preview --source 25` (final features
  run fine under preview too). Verified on host.
- Single-file source mode uses `--source <N>` (not `--release`); confirmed working for both.

---

## 3. Version gate

```
PASS=0 TOTAL=0; acc() records each sub-suite (TOTAL++; PASS++ on ok=1)
sub-suites: JDK17_OK, JDK21_OK, JDK23_OK, JDK25_OK, JDK25-COMPACT, SWITCH, SDK-SWITCH  (=7)
V=1 iff PASS==TOTAL && TOTAL>0
printf 'JDK_MULTI_OK=%s\n' "$V"
success_regex = (?m)^JDK_MULTI_OK=1
fail_regex    = (?i)\bpanic(?:ked)?\b
```
The `printf`-emitted token avoids `success`-substring false positives.

## 4. Version switching sub-test (the #764 "update-alternatives" intent)

All 4 JDKs are installed side-by-side under `/opt/jdk{17,21,23,25}` (the candidate roots).

1. **update-alternatives-style** (primary, the #764 intent): retarget a single
   `/opt/jdk-current` symlink + set `JAVA_HOME=/opt/jdk-current`, then `java -version`
   must report the selected major. Done for all 4 → `SWITCH ok=4/4`. This is exactly the
   mechanism `update-alternatives --set java` automates (it manages a symlink in the
   alternatives tree); StarryOS rootfs has only busybox (no `update-alternatives` binary),
   so the underlying symlink-swap is performed directly — semantically identical.
2. **sdkman-style** (offline): the rootfs pre-seeds `~/.sdkman/candidates/java/{17,21,23,25}-open`
   symlinks; the test retargets `~/.sdkman/candidates/java/current` (what `sdk use java <v>`
   does) and re-checks `java -version` → `SDK-SWITCH ok=4/4`.
   **sdkman caveat**: `sdk install` needs network (api.sdkman.io) + bash; the offline
   image cannot install candidates at runtime, so candidates are pre-seeded and only the
   offline `sdk use` (candidate switch) is exercised — which is the version-switching
   behavior #764 asks for. sdkman's own zip + bash/curl/unzip/zip apks are staged for
   completeness (staged from `java/java-tail/sdkman/package/`).

Parsing note (real bug found + fixed): `JAVA_TOOL_OPTIONS=-Xint` makes the JVM print a
`Picked up JAVA_TOOL_OPTIONS: -Xint` banner as the FIRST stderr line of `java -version`,
so the switch check greps the `... version "<major>` line (not `head -1`).

---

## 5. Per-arch JDK source mapping (musl vs glibc)

|              | JDK17                  | JDK21               | JDK23               | JDK25               |
|--------------|------------------------|---------------------|---------------------|---------------------|
| x86_64       | openjdk17 apk (musl)   | BellSoft musl tar   | BellSoft musl tar   | BellSoft musl tar   |
| aarch64      | openjdk17 apk (musl)   | BellSoft musl tar   | BellSoft musl tar   | BellSoft musl tar   |
| riscv64      | native-musl cross tar  | BellSoft **glibc**  | BellSoft **glibc**  | BellSoft **glibc**  |
| loongarch64  | openjdk17-loong apk (musl) | Loongson **glibc** | Loongson **glibc**  | Alpine native **musl** (C2 JIT port) |

**glibc caveat** (riscv64 21/23/25; loong 21/23): no upstream musl JDK exists for those
arch/version cells, so the prep script stages the **gcompat** shim (from the openjdk17-apks
arch dir) into the rootfs to bridge glibc `libc.so.6`/`ld-linux` references on Alpine-musl —
same approach the ros2 case uses. If a glibc JDK still fails under gcompat in qemu, that is
a real arch-coverage gap to record accurately (no fake-pass); the musl cells (all of
x86_64/aarch64, plus 17 + 25 on loong) are the clean path.

JIT note: StarryOS JIT is still unstable (#206) → every JVM forced to `-Xint` via
`JAVA_TOOL_OPTIONS` + `-Xint`. The loong JDK25 is a C2-JIT port but is also run with `-Xint`
here for parity/stability.

---

## 6. debugfs build steps (per arch)

```
source <tgoskits>/.starry-env.sh         # qemu-10; loong RAM>1G needs it
cargo xtask starry rootfs --arch <arch>  # ensure rootfs-<arch>-alpine.img base exists
bash <仓库根>/java/jdk-multi/case/prep-jdk-multi-rootfs.sh <arch>
#   -> writes $TGOSKITS_ROOT/tmp/axbuild/rootfs/rootfs-<arch>-jdk-multi.img (6G; loong fits 8G mem)
#   stages /opt/jdk{17,21,23,25}, /opt/jdk-current symlink, /root/jdkm/*.java,
#          ~/.sdkman candidates, gcompat (riscv/loong), /etc/ld-musl-<arch>.path
#   uses debugfs -w into the UNMOUNTED ext4 (no mount, no sync -> no WSL2 D-state deadlock)
#   prints "jdkNN/bin/java OK" for each version at the end
```
Image name: **`rootfs-<arch>-jdk-multi.img`** (matches each `qemu-<arch>.toml` `-drive file=`).

## 7. Integration steps

1. Create `test-suit/starryos/stress/openjdk-multi-0/openjdk-multi-0/` and copy in the 4
   `qemu-<arch>.toml` + `programs/Jdk{17,21,23,25}Features.java`. Add the 4
   `build-<target>.toml` files (copy from `openjdk17-0/build-*.toml` verbatim — same
   target/env/features; java cases use `plat_dyn=false`, `features=["qemu"]`).
2. Build the 4 rootfs imgs with the prep script (§6).
3. Run: `source <tgoskits>/.starry-env.sh; cargo xtask starry qemu --arch <arch>` per arch and
   confirm `JDK_MULTI_OK=1`.
4. Only then tick the #764 `openjdk 17 21 23 25 update-alternatives` item (据实打勾: tests
   complete + in rootfs + all-green 4-arch + 2-review).

## 8. Host validation

Host has glibc JDK17 + JDK21 system installs; JDK23/25 (and 21) musl tars run on the glibc
host via the musl loader + libz (from the openjdk17-apks zlib apk) on `LD_LIBRARY_PATH`.
- Each `Jdk{17,21,23,25}Features.java` → prints `JDKxx_OK` (see `host-ref/*.out`).
- JDK25 compact-headers run → `compact-object-headers flag present = true` + `JDK25_OK`.
- The **exact** toml `shell_init_cmd` body, remapped to host JDK homes, →
  `AGGREGATE: PASS=7 TOTAL=7` / `JDK_MULTI_OK=1` (`host-ref/toml-body-host.out`).
- The prep script's exact extraction commands → all 4 `/opt/jdkNN/bin/java` report the
  correct major; all feature suites pass against the staged payload.
Guest (StarryOS qemu) runs are performed during integration; this section covers host-level validation.
