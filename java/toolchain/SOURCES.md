# java-apps — Java 17 工具链与复杂程序（方案二 java17 适配补充资源）

为验证 starry 上 java17 能运行**复杂程序 + 构建工具**而下载（均为 JVM 字节码，架构无关，java 能运行即可在四架构通用）。

| 文件 | 版本 | 用途 | 来源 |
|------|------|------|------|
| `apache-maven-3.9.9-bin.tar.gz` | 3.9.9 | Maven 构建工具（本身是大型 JVM app） | archive.apache.org/dist/maven/maven-3/3.9.9/binaries/ |
| `gradle-8.10.2-bin.zip` | 8.10.2 | Gradle 构建工具 | services.gradle.org/distributions/ |
| `kotlin-compiler-2.0.21.zip` | 2.0.21 | Kotlin 编译器（大型 JVM app） | github.com/JetBrains/kotlin/releases/tag/v2.0.21 |
| `complex-demo/` | — | 自写复杂 Java 17 程序 + pom.xml + build.gradle（records/sealed/streams/concurrent/reflection/regex/time/NIO；无外部依赖，可被 maven+gradle 离线构建）| 本地编写 |

**使用**：在 starry 上将对应 tar/zip 解包进 rootfs，`java -jar`/`mvn`/`gradle`/`kotlinc` 运行。注意 java17 在 starry 当前需 bring-up flags：`-Xint -Xmx256m -Xms256m`，loongarch64 另加 `-XX:-UsePerfData -XX:+ReduceSignalUsage`。complex-demo 主机 ground-truth：`javac --release 17 -d out src/main/java/demo/*.java && java -cp out demo.App`。

下载日期：2026-05-20。
