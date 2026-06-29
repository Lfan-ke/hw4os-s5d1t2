# java-web — 来源与构建说明

JEE/JVM 框架矩阵的工业级地毯测试。上游载体为 `apps/starry/java-web`
（PR [rcore-os/tgoskits#1438](https://github.com/rcore-os/tgoskits/pull/1438)）。本目录是
CI-like 可构建交付：提交 carpet 源码 + 构建脚本 + 运行配置，依赖按下表获取后由
`build-jweb-jars.sh` 重建测试 jar，不随仓库 bundle 大体积产物。

## 被测框架及其来源

框架库由 `../dod-frameworks/jars/` 下的对应 fat jar 提供（这些 fat jar 由 dod-frameworks 从
Maven Central 收集，坐标 / 版本见下表）；`build-jweb-jars.sh` 把本目录 `carpets/` 的 carpet
源码以 `--release 17` 编译进各框架 fat jar，产出 `assets/` 下的测试 jar。

| 框架 | 版本 | 坐标 / 来源 |
|:--|:--|:--|
| Eclipse Jetty | 11.0.21 | `org.eclipse.jetty:jetty-server`/`jetty-servlet`/`jetty-webapp`，由 `../dod-frameworks/jars/jetty-demo.jar` 提供 |
| Netty | 4.1.112.Final | `io.netty:netty-all`，由 `../dod-frameworks/jars/netty-demo.jar` 提供 |
| MyBatis | 3.5.16 | `org.mybatis:mybatis`，由 `../dod-frameworks/jars/mybatis-demo.jar` 提供 |
| Hibernate ORM | 6.x（Jakarta Persistence 3.1，`jakarta.persistence-api` 3.1.0） | `org.hibernate.orm:hibernate-core`，由 `../dod-frameworks/jars/hibernate-demo.jar` 提供 |
| R2DBC | r2dbc-spi / r2dbc-h2 1.0.0.RELEASE + Project Reactor（`io.projectreactor:reactor-core` 3.x） | `io.r2dbc:r2dbc-h2`，由 `../dod-frameworks/jars/r2dbc-demo.jar` 提供 |
| sqlite-jdbc（MyBatis / Hibernate 的 JDBC 驱动） | 3.46.1.3 | `org.xerial:sqlite-jdbc`，已含于上述 mybatis / hibernate fat jar |
| sqlite musl JNI（riscv64 / loongarch64） | 3.46.1.3 | `../dod-frameworks/jars/sqlite-musl-jni/libsqlitejdbc-{riscv64,loongarch64}.so`（从 xerial sqlite-jdbc 源码交叉编译，上游 jar 不含这两个架构的 musl 原生库） |
| Jakarta Servlet（`.war` 部署面） | Jetty 11 自带 `jakarta.servlet` API | `WarCarpet` 用 `javax.tools` 在 target 上现编 servlet，打 `.war` 部署进内嵌 Jetty `WebAppContext` |
| OpenJDK 17（各架构 musl 运行时） | 见 `../openjdk17/SOURCES.md` | 由 `apps-starry/prebuild.sh` 从官方源获取（x86_64/aarch64 Alpine apk、loongarch64 Alpine apk、riscv64 native-musl 源码交叉编译 tar） |

## 构建

```
bash build-jweb-jars.sh
```

产出 `assets/{jetty,netty,mybatis,hibernate,r2dbc}-demo.jar` + `assets/native/*.so`，即上游
`apps/starry/java-web/assets/` 的内容；`apps-starry/prebuild.sh` 将其与 OpenJDK 17 注入
StarryOS per-app rootfs。`--release 17` 保证测试类为 JDK17 兼容字节码（与 host javac 版本无关）。

## 运行

按 `apps-starry/README.md`，在装好 qemu-10 的机器上将 `apps-starry/` 的内容置于 tgoskits
`apps/starry/java-web/` 后：

```
cargo xtask starry app qemu -t java-web --arch <x86_64|aarch64|riscv64|loongarch64>
```

四架构单核实测 `AGGREGATE PASS=6/6` + `JAVA_WEB_OK=6/6` + `TEST PASSED`（合计 589 条断言）。
