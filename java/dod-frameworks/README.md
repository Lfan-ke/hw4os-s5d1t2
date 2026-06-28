# java/dod-frameworks/ — 已通过的框架 demo（jetty/spring/netty/undertow/hibernate/mybatis/exposed/lombok/...）+ JSE 套件

#764 中**已据实打勾**的一批 Java 框架与标准库验证项的全部产物：每个框架一个最小真实 demo（自测 marker `XXX_DONE`），host 编译成 fat jar，注入到 `rootfs-<arch>-java.img:/root/dod/`，由 tgoskits `openjdk17-0` 用例的 `shell_init_cmd` 逐个 `java -jar` 跑 + `grep -q MARKER` 判定。

> **架构无关**：所有 jar 一份四架构通用；唯一例外是 sqlite-JDBC 的**原生 JNI .so**（C 代码，需按架构）——见 `jars/sqlite-musl-jni/`。

## 内容

```
dod-frameworks/
├── SOURCES.md          各框架的 maven 坐标 / 版本 / 可行性 / 构建命令（权威来源说明）
├── DOD-STEP-P6.sh      追加到 qemu toml shell_init_cmd 的 DoD 步骤片段
├── jars/               17 个已通过的 demo fat jar（注入 /root/dod/）
│   └── sqlite-musl-jni/    sqlite-jdbc 的 riscv64/loongarch64 原生 JNI .so（musl 交叉编译）
├── jse-suite/          15 个 JSE 标准库/语言/运行时测试源码（注入 /root/dod/，编译后跑）
└── src-modules/        每个框架的 pom.xml + src/（源码，便于复现重建；不含 target/ 构建缓存）
```

## jars/ —— 已绿框架（marker → #764 项）

| jar | marker | #764 项 | 验证内容 |
|-----|--------|---------|----------|
| `jetty-demo.jar` | `JETTY_DONE` | jetty √ | 内嵌 Jetty HTTP server 回环自测 |
| `netty-demo.jar` | `NETTY_DONE` | netty √ | Netty NIO TCP echo 回环 |
| `undertow-demo.jar` | `UNDERTOW_DONE` | undertow √ | 内嵌 Undertow HTTP 回环 (127.0.0.1) |
| `spring-demo.jar` | `SPRING_DONE` | spring √ | Spring Boot + 内嵌 Tomcat（bind 127.0.0.1） |
| `springdata-demo.jar` | `SDJ_DONE` | spring √ | Spring Data JPA × sqlite3 全栈 |
| `springkt-demo.jar` | `SPRINGKT_DONE` | spring/kotlin √ | Spring Boot (Kotlin) data class/高阶/空安全 + DI |
| `mybatis-demo.jar` | `MYBATIS_DONE` | mybatis √ | MyBatis ORM × sqlite3 |
| `hibernate-demo.jar` | `HIBERNATE_DONE` | hibernate(JPA) √ | Hibernate ORM/JPA × sqlite3 |
| `exposed-demo.jar` | `EXPOSED_DONE` | exposed √ | Kotlin Exposed ORM/DSL × sqlite3 |
| `lombok-demo.jar` | `LOMBOK_DONE` | lombok √ | Lombok 注解处理器/编译期生成 |
| `coroutines-demo.jar` | `COROUTINES_DONE` | kotlin √ | kotlinx-coroutines 运行时 |
| `r2dbc-demo.jar` | `R2DBC_DONE` | (DoD-C) | R2DBC reactive DB（r2dbc-h2 + Reactor） |
| `sqlite-demo.jar` | `SQLITE_DONE` | (ORM 地基) | xerial sqlite-jdbc（musl JNI） |
| `jdbc-demo.jar` / `realdep-demo.jar` | `*_DONE` | (JSE) | JDBC / 真实依赖图 |
| `ktor-demo.jar` | `KTOR_DONE` | ktor ! | Kotlin/Netty async web（也供 `../java-tail/ktor/` 用例） |
| `net-test.jar` | — | (JSE) | 网络/算法等综合 |

> kotlin 类框架（exposed/springkt/coroutines/ktor）的 jar 都在 **host 上编译**（绕开 starry 上 kotlinc 崩溃 [#237](https://github.com/rcore-os/tgoskits/issues/237)）。
> `jars/sqlite-musl-jni/libsqlitejdbc-{riscv64,loongarch64}.so` 是为 sqlite/mybatis/hibernate/exposed/springdata 提供的原生 JNI（这些架构无现成 musl 预编译，自交叉编译）。

## jse-suite/ —— JSE 标准库套件（15 项，全 √）

`NetTest AlgoTest StdlibTest ConcurrencyTest ConcurrencyDeep FileTest JvmTest SyntaxTest TimeTest XmlTest LangUtilTest ExtraTest CryptoTest ProcessTest NioChannelTest`，各打印 `*_DONE`。用例里编译后 `java -cp /root/dod <T>` 逐个跑。

## 怎么用（手动验证单个框架）

jar 已注入 guest 的 `/root/dod/`。在 guest shell 里：

```sh
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk
$JAVA_HOME/bin/java -Xint -Xms64m -Xmx256m -jar /root/dod/jetty-demo.jar        # 期望 JETTY_DONE
$JAVA_HOME/bin/java -Xint -Xms64m -Xmx512m -jar /root/dod/hibernate-demo.jar    # 期望 HIBERNATE_DONE
$JAVA_HOME/bin/java -Xint -Xms64m -Xmx512m -Dserver.port=18080 -Dserver.address=127.0.0.1 \
    -Djava.net.preferIPv4Stack=true -jar /root/dod/spring-demo.jar              # 期望 SPRING_DONE
# JSE 套件（先 javac 进 /root/dod，再跑）：
$JAVA_HOME/bin/java -Xint -Xms32m -Xmx256m -cp /root/dod NetTest               # 期望 *_DONE
```

整套由 `openjdk17-0` 用例聚合判定（见 `../openjdk17/README.md`）。各框架的 maven 坐标 / 版本上限 / 可行性 / host 构建命令见 `SOURCES.md`。
