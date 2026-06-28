# java/bigapps/ — 大数据 / EE 大应用（neo4j / iceberg / paimon / trino / ozone / wildfly / quarkus）

#764「linux 大应用选取」的 Java/JVM/Hadoop 子集。全部**架构无关**（一份 tar/jar 四架构通用，运行时依赖 `../openjdk17/` 或 `../jdk-multi/` 的 musl JRE）。所有来源 / 版本 / sha256 / 适配风险见 **`SOURCES.md`**（权威来源说明，已逐字与上游官方校验文件核对）。

## 这里放了什么 / 引用了什么

为控制 staging 仓库体积，**小的 runtime jar 入库，巨型 server tar 以 `SOURCES.md` 引用为主**（含完整 URL + sha256，可按需重新下载；大文件经 Git LFS）：

| 应用 | 类型 | 版本 | 大小 | 本仓库状态 |
|------|------|------|------|-----------|
| **iceberg** | spark runtime fat jar | 1.11.0 | 48 MB | √ 入库 `iceberg/iceberg-spark-runtime-3.5_2.12-1.11.0.jar` |
| **paimon** | flink bundle jar | 1.4.1 | 55 MB | √ 入库 `paimon/paimon-flink-1.20-1.4.1.jar` |
| **quarkus** | CLI | 3.35.4 | 20 MB | 入库于 `../java-tail/quarkus/package/`（CLI 用例 payload） |
| **wildfly** | EE app server tar | 40.0.0.Final | 262 MB | 入库于 `../java-tail/wildfly/package/`（wildfly 用例 payload） |
| **neo4j** | 图数据库 server tar | community 2026.04.0 | 235 MB | 引用（URL+sha256 见 `SOURCES.md`） |
| **trino** | 分布式 SQL server + cli | server 476 + cli 481 | 821 + 19 MB | 引用（见 `SOURCES.md`；进程内 demo 在 `../dod-frameworks/src-modules/trino/`） |
| **ozone** | Apache 对象存储(Hadoop) | 2.1.0 | 499 MB | 引用（见 `SOURCES.md`） |

> 引用项需要时按 `SOURCES.md` 里的 `curl -L <URL>` 重新拉取并核对 sha256。

## 适配提示 + 风险（findings 方向，详见 `SOURCES.md` §3）

* **JDK 版本错配**：`../openjdk17/` 是 JDK 17。trino 476 要 JDK 23、ozone 2.1.0 与 neo4j 2026.x 多要 JDK 21+。→ 跑这些新版需更高 JDK（用 `../jdk-multi/` 的 JDK21/23/25），或选各应用支持 JDK17 的旧版本（neo4j 5.26 LTS、wildfly 40、iceberg/paimon jar 通常 11/17 即可）。**先以 wildfly/quarkus/iceberg/paimon/neo4j-LTS（JDK17 友好）建案**，trino/ozone 标注「需更高 JDK，后置」。
* **trino 的现实路径**：完整 server（~1 GB，多 GB heap，Discovery 集群）在 starry **不可行**；务实路径是 `../dod-frameworks/src-modules/trino/` 的进程内 `LocalQueryRunner` + TPCH（x86_64 优先，stretch）。**Trino 435 是最后一个 Java-17 版本**（436+ 要 Java 22）。
* **架构覆盖**：架构无关是这批最大优点 —— 无 Go/C 那种 riscv/loong 缺预编译问题；4 架构由 4 架构 musl JRE 一次性覆盖。

## 怎么用

这些是「大应用选取」的候选资料，不是当前已打勾的用例（已绿的框架在 `../dod-frameworks/`，收官中的在 `../java-tail/`）。建案时：解包对应 tar/jar 进 rootfs，用合适版本的 JRE（17 或 21+）跑其 `bin/<app>` 启动脚本（多为调 `$JAVA_HOME/bin/java` 的 shell 包装）；务实走轻量路径（如 `ozone version` / 单进程 `ozone freon`、neo4j 单实例、trino 进程内引擎），而非全守护进程集群。
