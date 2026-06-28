# java/java-lang — 来源与说明

OpenJDK 语言级地毯式测试（#764 的 `Java`/`javac`/`SE25` 项语言层）。对应上游 app 化目录
`apps/starry/java-lang`，交付仓库内为同一套源 + 复现资产指针。

## 覆盖范围

- **多版本 JDK**：OpenJDK **17 / 21 / 23 / 25**，每个可运行版本各跑：
  - `Jdk{17,21,23,25}Features.java` —— 版本感知的语言/库新特性（records/sealed/pattern matching/
    switch 表达式/text block/virtual threads/sequenced collections/`var`/foreign 等，按 JDK 主版本门控）。
  - `JavaLangCarpet.java` —— 综合语言地毯（泛型/lambda/stream/注解/反射/并发/NIO/集合/数值），断言计数门控。
  - `JavaGrammar.java` —— 语法面覆盖。
  - `java-cli-core.sh` —— `java` / `javac` 命令选项树地毯（逐 `--help` 项）。
  - `BackCompat.java` + `programs/backcompat/` —— **Java 8 向后兼容**：一份 `--release 8`（bytecode 52）
    编译的 JUnit4 套件（Commons IO/Math3/Lang3/Collections4、Log4j2、H2+HSQLDB、Gson、BeanShell），
    在每个可运行 JDK 上跑出 identical `BACKCOMPAT_REAL_OK`。
- **聚合门控**：`run-java.sh` 跑全部 leg，打印 `AGGREGATE PASS=N TOTAL=M` + `JDK_MULTI_OK` + `TEST PASSED`。

## 4 架构 × 4 版本矩阵（据实）

| arch | 可运行 JDK | 备注 |
|:--:|:--:|:--|
| x86_64 | 17 / 21 / 23 / 25 (4/4) | Bellsoft musl，经上游 CI |
| aarch64 | 17 / 21 / 23 / 25 (4/4) | Bellsoft musl |
| riscv64 | 17 / 21 / 23 (3/4) | JDK23 经**真 Debian glibc 运行时桥**（见下）；JDK25 为 Alpine Zero-VM，启动 `IllegalInstruction`（上游无 rv glibc JDK25 资产），据实记为架构墙 |
| loongarch64 | 17 / 21 / 25 (3/4) | JDK23 唯一资产为 Loongson 旧 ABI（链接 `GLIBC_2.27`），与上游新 ABI musl/glibc 不匹配，据实记为架构墙 |

## 资产来源

- **JDK 包**：复用同仓 `../jdk-multi/packages/`：
  - Bellsoft Liberica musl `jdk/jre {17,21,23,25}`（x86_64 / aarch64 / riscv64），来源 bell-sw.com。
  - Loongson `jdk21`（`loongson21.10.25-fx-jdk21.0.10_7`）、`jdk23`（`loongson23.1.17-fx-jdk23_37`），来源 loongnix。
- **glibc 运行时桥（rv-JDK23）**：`glibc-debian/riscv64/libc6_2.41-12+deb13u3_riscv64.deb`
  （Debian trixie `pool/main/g/glibc`，sha256 `fee42ebb2a148cc0dbc46ba938d8d69495b6dd5250cecafed9d585c567550b7a`）。
  glibc 版 JDK23 需真 glibc 闭包（`ld-linux-riscv64-lp64d.so.1` + `lib/riscv64-linux-gnu/{libc,libm,librt,libdl,libpthread}.so`），
  musl 的 gcompat 垫片不足；`prebuild.sh` 从该 deb 抽出闭包注入 overlay，配 `-Xint -Xmx512m -Xms64m`
  与默认 CDS，rv 上 glibc JDK23 即可运行。
- **构建时**：`prebuild.sh` 经 `ensure_asset` 从资产缓存目录 `JAVA_DL_ROOT`（缺省为脚本相对的
  `<staging>/.cache/java-dl`，可由维护者指向本机已有的下载缓存）+ 官方 URL 按 sha256 命中获取，任意宿主可复现。

## 运行

```sh
cargo xtask starry app qemu -t java-lang --arch x86_64   # aarch64 / riscv64 / loongarch64
```

`success_regex = (?m)^TEST PASSED\s*$`；rootfs 在 `prebuild.sh` 内 `resize2fs` 扩到 6G 以容纳多 JDK。
