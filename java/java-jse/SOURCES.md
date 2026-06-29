# java-jse — 来源与构建说明

J2SE 第三方类库 + JSE 标准库的工业级地毯测试。上游载体为 `apps/starry/java-jse`
（PR [rcore-os/tgoskits#1437](https://github.com/rcore-os/tgoskits/pull/1437)）。本目录是
CI-like 可构建交付：提交 carpet 源码 + 构建脚本 + 运行配置，依赖按下表获取后由
`build-jse-jars.sh` 重建测试 jar，不随仓库 bundle 大体积产物。

## 被测库及其来源

| 库 | 版本 | 来源 |
|:--|:--|:--|
| jackson-databind / core / annotations | 2.17.x | 由 `../dod-frameworks/jars/realdep-demo.jar` 提供（Maven Central `com.fasterxml.jackson.core`） |
| guava | 33.2.1-jre | `realdep-demo.jar`（Maven Central `com.google.guava:guava`） |
| commons-lang3 | 3.14.0 | `realdep-demo.jar`（Maven Central `org.apache.commons:commons-lang3`） |
| H2 | 2.2.224 | `../dod-frameworks/jars/jdbc-demo.jar`（Maven Central `com.h2database:h2`） |
| slf4j-api / logback-classic | 2.0.x / 1.5.x | `jdbc-demo.jar`（Maven Central `org.slf4j` / `ch.qos.logback`） |
| sqlite-jdbc | 3.46.1.3 | `../dod-frameworks/jars/sqlite-demo.jar`（Maven Central `org.xerial:sqlite-jdbc`） |
| sqlite musl JNI（riscv64 / loongarch64） | 3.46.1.3 | `../dod-frameworks/jars/sqlite-musl-jni/libsqlitejdbc-{riscv64,loongarch64}.so`（从 xerial sqlite-jdbc 源码交叉编译，上游 jar 不含这两个架构的 musl 原生库） |
| lombok（注解处理器，仅编译期） | 1.18.34 | Maven Central `org.projectlombok:lombok:1.18.34`（`https://repo1.maven.org/maven2/org/projectlombok/lombok/1.18.34/lombok-1.18.34.jar`），构建时 `LOMBOK_JAR` 指向它 |
| OpenJDK 17（各架构 musl 运行时） | 见 `../openjdk17/SOURCES.md` | 由 `apps-starry/prebuild.sh` 从官方源获取（x86_64/aarch64 Alpine apk、loongarch64 Alpine apk、riscv64 native-musl 源码交叉编译 tar） |

## 构建

```
LOMBOK_JAR=/path/to/lombok-1.18.34.jar bash build-jse-jars.sh
```

产出 `assets/{realdep-demo,jdbc-demo,sqlite-demo,jse-suite}.jar` + `assets/native/*.so`，
即上游 `apps/starry/java-jse/assets/` 的内容；`apps-starry/prebuild.sh` 将其与 OpenJDK 17
注入 StarryOS per-app rootfs。

## 运行

按 `apps-starry/README.md`，在装好 qemu-10 的机器上将 `apps-starry/` 的内容置于 tgoskits
`apps/starry/java-jse/` 后：

```
cargo xtask starry app qemu -t java-jse --arch <x86_64|aarch64|riscv64|loongarch64>
```

四架构单核实测 `AGGREGATE PASS=22/22` + `TEST PASSED`。
