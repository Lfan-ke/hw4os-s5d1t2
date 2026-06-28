# java/ — Java / JVM 全栈 StarryOS 适配交付

[#764](https://github.com/rcore-os/tgoskits/issues/764) 的 **Java 子课题**全部交付物：从 OpenJDK 17 运行时本身，到构建工具（maven/gradle/kotlin）、复杂程序、十多个真实框架 demo、多版本 JDK、以及收尾中的 Java 收官项（ktor/quarkus/wildfly/sdkman）和大型应用（neo4j/trino 等）。

> **关键事实——架构无关**：Java 字节码（`.jar`）与 JVM 应用（构建工具、框架）不区分 CPU 架构，同一份文件在 x86_64/aarch64/riscv64/loongarch64 上都能跑。**四架构覆盖完全由「4 架构的 musl OpenJDK 运行时」保证**（见 `openjdk17/` 与 `jdk-multi/`）。因此除了 JDK 运行时本身按架构分目录，其余 jar/构建工具都只存一份。
>
> **StarryOS 运行约定**（所有用例统一）：JIT 在 StarryOS 仍不稳定（[#206](https://github.com/rcore-os/tgoskits/issues/206)），所有 JVM 一律 `-Xint`（解释器）+ 显式 `-Xms/-Xmx`（StarryOS 堆 ergonomics 会把初始堆算错 → "Too small initial heap"）；loongarch64 另加 `-XX:-UsePerfData -XX:+ReduceSignalUsage`。

---

## #764 Java 子课题打勾状态

逐项与上游 issue 的 checklist 对齐（`√` = 已据实打勾；`!` = 用例就绪、待 4 架构运行确认后打勾）：

| 项目 | 状态 | 交付位置 | 说明 |
|------|------|----------|------|
| **javac** | √ | `openjdk17/`（openjdk17-0 用例 JAVAC 段） | rootfs 内 `javac` 编译 5×，4 架构通过 |
| **kotlinc** | √ | `toolchain/kotlin/` + openjdk17-0 KOTLIN 段 | 用 `java -cp kotlinc/lib/* K2JVMCompiler` 调用（busybox 无 bash 包装） |
| **maven** | √ | `toolchain/maven/` + openjdk17-0 MVN 段 | `mvn -o package` 离线构建 complex-demo |
| **gradle** | √ | `toolchain/gradle/` + openjdk17-0 GRADLE 段 | `gradle --offline --no-daemon build` |
| **kotlin (JVM)** | √ | `dod-frameworks/`（springkt/exposed/coroutines/ktor 等 host 编译 jar） | Kotlin 运行时在 starry 运行 |
| **jetty** | √ | `dod-frameworks/jars/jetty-demo.jar` | 内嵌 HTTP server 回环自测 |
| **netty** | √ | `dod-frameworks/jars/netty-demo.jar` | NIO TCP echo 回环 |
| **undertow** | √ | `dod-frameworks/jars/undertow-demo.jar` | 内嵌 HTTP 回环 |
| **spring** (boot, java+kotlin) | √ | `dod-frameworks/jars/{spring,springdata,springkt}-demo.jar` | Spring Boot + 内嵌 Tomcat |
| **mybatis** | √ | `dod-frameworks/jars/mybatis-demo.jar` | ORM × sqlite3 (musl JNI) |
| **hibernate** (JPA) | √ | `dod-frameworks/jars/hibernate-demo.jar` | JPA × sqlite3 |
| **exposed** | √ | `dod-frameworks/jars/exposed-demo.jar` | Kotlin ORM/DSL × sqlite3 |
| **lombok** | √ | `dod-frameworks/jars/lombok-demo.jar` | 注解处理器/编译期代码生成 |
| **sdkman** | ! | `java-tail/sdkman/` | 离线 surface（version/help/candidate cache）用例就绪 |
| **wildfly** | ! | `java-tail/wildfly/` | 完整 40.0.0.Final app server 启动+回环用例就绪 |
| **quarkus** | ! | `java-tail/quarkus/` | CLI 3.35.4 离线 surface 用例就绪 |
| **ktor** | ! | `java-tail/ktor/` + `dod-frameworks/jars/ktor-demo.jar` | Kotlin/Netty async web 用例就绪 |
| **trino** | ! | `dod-frameworks/src-modules/trino/` + `bigapps/` | 内嵌 LocalQueryRunner（x86_64 优先；待扩展） |
| **jdk 17/21/23/25 + update-alternatives** | ! | `jdk-multi/` | 多版本特性 + 两种版本切换用例就绪 |
| **nodejs**（含 kotlin-js） | √ | （非本目录，见仓外 nodejs 交付） | 已通过 |
| **python**（3.14/uv/venv） | √ | （非本目录，见仓外 python 交付） | 已通过 |

> 打勾准则：测例齐全（核心 + 高级 + 版本新特性 + 边界）→ host ground-truth → qemu-Linux 4 架构 → StarryOS 4 架构运行 → 用 `printf` 防 `success_regex` 假阳性 → 才据实把 `[ ]` 翻成 `[x]`。标 `!` 的项已完成 host 验证，待补的是 StarryOS guest 4 架构运行确认。

---

## 子目录导览

| 目录 | 内容 | README |
|------|------|--------|
| `openjdk17/` | OpenJDK 17 musl 运行时（4 架构 apk + riscv64 glibc 回退方案 + gcompat + 源码编译资料）；是所有 java 用例的运行时基座，对应 tgoskits `openjdk17-0` 用例 | `openjdk17/README.md` |
| `jdk-multi/` | JDK 17/21/23/25 全套（musl/glibc 按架构）+ 版本切换（update-alternatives 风格 + sdkman 风格）；对应 `openjdk-multi-0` 用例 | `jdk-multi/README.md` |
| `toolchain/` | maven 3.9.9 / gradle 8.10.2 / kotlin-compiler 2.0.21 构建工具包 | `toolchain/README.md` |
| `complex-demo/` | 自写复杂 Java 17 程序（records/sealed/streams/concurrent/reflection/...），maven+gradle 离线可构建，无外部依赖 | `complex-demo/README.md` |
| `dod-frameworks/` | 已通过的框架 demo（jetty/spring/netty/undertow/hibernate/mybatis/exposed/lombok/coroutines/r2dbc/...）的 fat jar + 源码模块 + JSE 标准库套件 | `dod-frameworks/README.md` |
| `java-tail/` | 收尾中的 4 个用例：ktor / quarkus / wildfly / sdkman（各含 4 架构 toml + prep 脚本 + 上游包） | `java-tail/README.md` |
| `bigapps/` | 大数据/EE 大型应用：neo4j / iceberg / paimon / trino / ozone / wildfly / quarkus（含 SOURCES + 小型 runtime jar，大体积 server tar 以引用为主） | `bigapps/README.md` |

---

## 运行时基座与用例的关系

`openjdk17/` 的 4 架构 musl JRE 被构建进基础镜像 **`rootfs-<arch>-java.img`**（tgoskits `tmp/axbuild/rootfs/` 下，约 3 GB，含 maven/gradle/kotlin + dod jars）。`java-tail/` 的每个用例都是在这个基座上的**增量覆盖**：prep 脚本把 `rootfs-<arch>-java.img` 复制为 `rootfs-<arch>-<app>.img`，只注入该用例多出来的 payload（如 wildfly tar、quarkus CLI、sdkman + bash 闭包），不重建 JRE。`jdk-multi/` 例外——它要并排安装 4 个 JDK，所以从 alpine 基础镜像新建 `rootfs-<arch>-jdk-multi.img`。
