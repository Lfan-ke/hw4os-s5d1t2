# java/java-web — JEE 框架矩阵地毯测试

在 StarryOS 上用 OpenJDK 17 对一组 JEE/JVM 框架做工业级地毯测试，四架构
（x86_64 / aarch64 / riscv64 / loongarch64）单核 qemu-10 运行。每个模块依框架公开
API 逐项铺满（精确值断言）：HTTP server 走真实 IPv4 回环（`HttpURLConnection` 抓状态行 /
响应体 / content-type），ORM 跑内存数据库，编解码用 Netty `EmbeddedChannel` 做确定性单元。
每个模块仅当其内部 fail 计数为 0 时打印锚定的 `*_DONE` marker；`run-jweb.sh` 跑全部 6 个
模块，全部通过（PASS == TOTAL，无 skip）才输出 `TEST PASSED`，合计 **6 模块 / 589 条断言**。

上游载体：`apps/starry/java-web`，PR [rcore-os/tgoskits#1438](https://github.com/rcore-os/tgoskits/pull/1438)。

## 覆盖

| 模块 | 框架 | 维度 | marker | 断言 |
|:--|:--|:--|:--|--:|
| JettyCarpet | Eclipse Jetty | 内嵌 HTTP server：handler / 路由 / 方法 / 状态行-body-header 断言 over 回环 | `JETTY_DONE` | 92 |
| NettyCarpet | Netty 4.x | ByteBuf + `EmbeddedChannel` 编解码 / handler 单元 + 真实回环 TCP echo + HTTP-codec server | `NETTY_DONE` | 60 |
| MyBatisCarpet | MyBatis | `SqlSessionFactory` / mapper / 注解 / 动态 SQL / 批 / 事务 over 内存 DB | `MYBATIS_DONE` | 80 |
| HibernateCarpet | Hibernate / JPA | `SessionFactory` / 实体 / CRUD / HQL-JPQL / Criteria / 关系 / 分页 over 内存 DB | `HIBERNATE_DONE` | 134 |
| R2dbcCarpet | R2DBC | 反应式 `ConnectionFactory` / `Statement` / `Result` + 确定性订阅 + 事务 | `R2DBC_DONE` | 123 |
| WarCarpet | Jakarta Servlet | 真实 `.war`（servlet + `web.xml`）部署进内嵌 Jetty `WebAppContext`，over 回环 HTTP | `WAR_DONE` | 100 |

四架构单核 qemu-10 StarryOS 实测各 `AGGREGATE PASS=6/6` + `JAVA_WEB_OK=6/6` + `TEST PASSED`。

## 构建与运行

依赖来源与构建命令见 `SOURCES.md`。`build-jweb-jars.sh` 从 `../dod-frameworks/jars/` 的框架
fat jar + `carpets/` 下的 carpet 源码（`--release 17` 编译，保证 JDK17 兼容字节码）重建
`assets/` 下的测试 jar 与 `assets/native/` 的 sqlite musl JNI；`apps-starry/` 是上游 StarryOS
app 的配置（4 架构 `build-*.toml` / `qemu-*.toml` / `prebuild.sh` / `run-jweb.sh`），运行命令见
`apps-starry/README.md`：

```
cargo xtask starry app qemu -t java-web --arch <x86_64|aarch64|riscv64|loongarch64>
```

MyBatis / Hibernate 用 sqlite-jdbc 3.46.1.3：x86_64 / aarch64 用 jar 内置的 musl native，
riscv64 / loongarch64 用交叉编译的 musl JNI `.so`（上游 jar 不含这两个架构的 musl 原生库），由
`prebuild.sh` 在 rv/loong 上 stage 到 `/root/jweb/native/libsqlitejdbc.so`，`run-jweb.sh` 用
`org.sqlite.lib.path` 指向它。
