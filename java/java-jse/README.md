# java/java-jse — J2SE 类库 + JSE 标准库地毯测试

在 StarryOS 上用 OpenJDK 17 对一组 J2SE 第三方类库与 JSE 标准库做工业级地毯测试，四架构
（x86_64 / aarch64 / riscv64 / loongarch64）单核 qemu-10 运行。每个模块依各自官方 API 全集
逐项铺满（数百条精确值断言），合计 **22 模块 / 约 5650 条断言**；`run-jse.sh` 跑全部模块，
全部通过（无 skip）才输出 `TEST PASSED`。

上游载体：`apps/starry/java-jse`，PR [rcore-os/tgoskits#1437](https://github.com/rcore-os/tgoskits/pull/1437)。

## 覆盖

J2SE 第三方类库（`lib-carpets/`）：

| 模块 | 库 | marker | 断言 |
|:--|:--|:--|--:|
| JacksonCarpet | jackson-databind（流式 / databind / 树模型 / 注解 / 多态 / 特性） | `JACKSON_DONE` | 169 |
| GuavaCarpet | Guava（不可变集合 / Multimap / BiMap / Table / RangeSet / 哈希 / 缓存 …） | `GUAVA_DONE` | 366 |
| Lang3Carpet | commons-lang3（StringUtils / ArrayUtils / NumberUtils / builders / tuple / 反射 …） | `LANG3_DONE` | 359 |
| H2Carpet | H2 JDBC（DDL/DML/DQL/JOIN/窗口/事务/类型/约束/序列）+ `org.h2.tools.*` 命令行工具 | `H2_DONE` | 276 |
| LogCarpet | slf4j + logback（级别 / 参数化 / MDC / 编程式 appender / pattern / 过滤，断言格式化输出） | `LOG_DONE` | 189 |
| SqliteJdbcCarpet | xerial sqlite-jdbc（完整 JDBC + PRAGMA / 类型亲和 / 外键 / 触发器 / CTE / UPSERT / json1） | `SQLITEJDBC_DONE` | 295 |
| LombokCarpet | lombok 全注解（@Data/@Builder/@Value/@With/@NonNull/@SneakyThrows/@Cleanup/@Slf4j …） | `LOMBOK_DONE` | 115 |

JSE 标准库（`jse-suite/`）：Algo / Concurrency / ConcurrencyDeep / Crypto / Extra / File /
Jvm / LangUtil / Net / NioChannel / Process / Stdlib / Syntax / Time / Xml，各覆盖对应
`java.*` 包，约 3580 条断言。

## 构建与运行

依赖来源与构建命令见 `SOURCES.md`。`build-jse-jars.sh` 从 `../dod-frameworks/jars/` 的库
fat jar + lombok 处理器重建 `assets/` 下的测试 jar；`apps-starry/` 是上游 StarryOS app 的
配置（4 架构 `build-*.toml` / `qemu-*.toml` / `prebuild.sh` / `run-jse.sh`），运行命令见
`apps-starry/README.md`。
