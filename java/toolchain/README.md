# java/toolchain/ — Java 构建工具（maven / gradle / kotlin）

#764 的构建工具子项（**javac/maven/gradle/kotlinc/kotlin 已全部据实打勾**）。这三个本身就是大型 JVM 应用，在 StarryOS 上能离线构建/编译，即对 JVM 的文件 I/O、class 加载、futex、信号、大 heap 的综合压力验证。

> **架构无关**：三者都是 JVM 字节码，一份包四架构通用；四架构覆盖由 `../openjdk17/` 的 4 架构 musl JRE 保证。

## 内容 + 来源

| 文件 | 版本 | 用途 | 来源 |
|------|------|------|------|
| `maven/apache-maven-3.9.9-bin.tar.gz` | 3.9.9 | Maven 构建工具 | `archive.apache.org/dist/maven/maven-3/3.9.9/binaries/` |
| `gradle/gradle-8.10.2-bin.zip` | 8.10.2 | Gradle 构建工具 | `services.gradle.org/distributions/` |
| `kotlin/kotlin-compiler-2.0.21.zip` | 2.0.21 | Kotlin 编译器 | `github.com/JetBrains/kotlin/releases/tag/v2.0.21` |

来源与下载日期见 `SOURCES.md`（即 java-apps 原始 README）。

## 在 StarryOS 上怎么用（落到 `rootfs-<arch>-java.img`）

解包进 rootfs：maven → `/opt/apache-maven-3.9.9`，gradle → `/opt/gradle-8.10.2`，kotlin → `/opt/kotlinc`。运行约定（`openjdk17-0` 用例的实际运行命令）：

```sh
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk
export JAVA_TOOL_OPTIONS="-Xint"          # 强制所有嵌套 JVM 走解释器（JIT #206）
export MAVEN_OPTS="-Xint -Xmx256m -Xms32m"

# maven 离线构建（仓库预热在 /root/.m2）
/opt/apache-maven-3.9.9/bin/mvn -o -Dmaven.repo.local=/root/.m2/repository -DskipTests package   # 期望 BUILD SUCCESS

# gradle 离线、无 daemon
/opt/gradle-8.10.2/bin/gradle --offline --no-daemon -g /root/.gradle build                       # 期望 BUILD SUCCESSFUL

# kotlin：busybox 无 bash，kotlinc 包装脚本无法运行 → 直接用 java 调编译器主类
$JAVA_HOME/bin/java -Xint -Xms64m -Xmx512m -cp "/opt/kotlinc/lib/*" \
    org.jetbrains.kotlin.cli.jvm.K2JVMCompiler /tmp/K.kt -include-runtime -d /tmp/K.jar          # 编译
$JAVA_HOME/bin/java -Xint -Xms32m -Xmx256m -jar /tmp/K.jar                                        # 运行，期望 KOTLIN_OK
```

> kotlinc 必须显式 `-Xms`：StarryOS 堆 ergonomics 会把初始堆算小（"Too small initial heap"）。`maven`/`gradle` 的构建对象就是 `../complex-demo/`（无外部依赖，离线可建）。
>
> 这些命令是 tgoskits `openjdk17-0` 用例 `shell_init_cmd` 里的 MVN/GRADLE/KOTLIN 段；4 架构均已通过运行验证。
