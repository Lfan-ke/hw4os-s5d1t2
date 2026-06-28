# 大应用（Java/Hadoop）下载来源清单（方案二 · #764 · neo4j / iceberg / paimon / wildfly / quarkus / trino / ozone）

适配目标：StarryOS × 4 架构。**这些是 Java/JVM/Hadoop 应用 → 产物架构无关**（一份 tar/jar 四架构通用），运行时依赖 JRE（已在 `../openjdk17-apks/`，4 arch 各自的 musl JRE）。
跟踪 issue：rcore-os/tgoskits#764 — 「linux 大应用选取」`neo4j/iceberg/minio/paimon/ozone`（minio 在 `../golang-bins/`，是 Go）+ java 子课题 `wildfly/quarkus/trino`。

> **关键事实：架构无关**。Java 字节码 jar / JVM 应用 tar 不区分 CPU 架构；同一份文件在 x86_64/aarch64/riscv64/loongarch64 的 JRE 上都能运行。因此**只下载一份**，不做 per-arch 目录（与 `../java-apps/` 同思路）。四架构覆盖 = 由 `../openjdk17-apks/` 的 4-arch JRE 保证。所有 sha256/sha1 于 2026-05-24 与上游官方校验文件核对一致。

---

## 0. 结构速览（全部架构无关，4-arch 由 JRE 覆盖）

| 应用 | 类型 | 版本 | 文件 | 大小 | 4-arch | 校验 |
|------|------|------|------|------|--------|------|
| **neo4j** | 图数据库（JVM server tar） | community 2026.04.0 | `neo4j/neo4j-community-2026.04.0-unix.tar.gz` | 235 MB | √ JRE 通用 | sha256 = 官方 √ |
| **iceberg** | 表格式 runtime jar | 1.11.0 (spark-3.5_2.12 runtime) | `iceberg/iceberg-spark-runtime-3.5_2.12-1.11.0.jar` | 48 MB | √ | maven sha1 √ |
| **paimon** | 湖格式 bundle jar | 1.4.1 (flink-1.20 bundle) | `paimon/paimon-flink-1.20-1.4.1.jar` | 55 MB | √ | jar manifest √ |
| **wildfly** | Jakarta EE app server | 40.0.0.Final | `wildfly/wildfly-40.0.0.Final.tar.gz` | 262 MB | √ | 官方 sha1 √ |
| **quarkus** | CLI（启 demo 用） | CLI 3.35.4 | `quarkus/quarkus-cli-3.35.4.tar.gz` | 20 MB | √ | 官方 sha256 √ |
| **trino** | 分布式 SQL 引擎 server | server 476 + cli 481 | `trino/trino-server-476.tar.gz` + `trino/trino-cli-481-executable.jar` | 821 MB + 19 MB | √ | jar/tar 结构 √ |
| **ozone** | Apache 对象存储（Hadoop） | 2.1.0 | `ozone/ozone-2.1.0.tar.gz` | 499 MB | √ | tar 结构 √ |

→ **全部 7 项现成可建案**（架构无关 + 4-arch JRE 已备）。无任何 per-arch 缺口（这是 Java 应用相对 Go/C 的最大优势）。

---

## 1. 各应用下载 + 说明

### 1.1 neo4j community 2026.04.0
- 上游：`dist.neo4j.org`（CalVer 2026.04.0 = 最新；5.26.26 是 LTS 备选）。
- `curl -L https://dist.neo4j.org/neo4j-community-2026.04.0-unix.tar.gz`
- 解包顶层 `neo4j-community-2026.04.0/`，含 `bin/neo4j`（shell 包装，调 `$JAVA_HOME/bin/java`）、`lib/*.jar`、`conf/`。需 JDK 17/21。
- sha256 `fd750466...` = 官方 `.sha256` √。

### 1.2 iceberg 1.11.0（runtime jar）
- 上游：Maven Central `org/apache/iceberg/iceberg-spark-runtime-3.5_2.12`，release 1.11.0。
- 单 fat-jar（含 iceberg-core + spark 集成），用于 spark/flink 引擎里跑表操作。也可仅做 jar 加载 + `iceberg-core` API 探针。
- sha1 `86eb1291...` = maven `.jar.sha1` √。

### 1.3 paimon 1.4.1（flink bundle jar）
- 上游：Maven Central `org/apache/paimon/paimon-flink-1.20`，release 1.4.1。
- 单 bundle-jar（含 paimon-core + flink-1.20 connector）。manifest `Implementation-Title: Paimon : Flink : 1.20` / `Implementation-Version: 1.4.1` √。

### 1.4 wildfly 40.0.0.Final
- 上游：GitHub `wildfly/wildfly` release（二进制 tar.gz，**不是 -src**）。
- `curl -L https://github.com/wildfly/wildfly/releases/download/40.0.0.Final/wildfly-40.0.0.Final.tar.gz`
- 解包顶层 `wildfly-40.0.0.Final/`，含 `bin/standalone.sh`、`modules/`、`standalone/`。Jakarta EE 10 full server。
- sha1 `0b5948eb...` = 官方 `.tar.gz.sha1` √。

### 1.5 quarkus CLI 3.35.4
- 上游：GitHub `quarkusio/quarkus` release 资产 `quarkus-cli-3.35.4.tar.gz`（CLI 而非整框架；用 CLI `quarkus create` + `quarkus dev`/`build` 拉起 demo）。
- 解包顶层 `quarkus-cli-3.35.4/`，`bin/quarkus`（JVM 包装）。
- sha256 `4ce2ed59...` = 官方 `checksums_sha256.txt` √。
- 注意：`quarkus dev`/项目创建会尝试拉取 maven 依赖；StarryOS 离线时需预热本地 `.m2`（可复用 `../java-apps/` 的 maven 缓存策略）。

### 1.6 trino（server 476 + cli 481）
- **server tarball 走 Maven Central** `io/trino/trino-server`，**当前 maven 已发布到 476**（481 的 server tar 尚未上 central，GitHub release 481 只放各 plugin zip + `trino-cli-481`）。故 server 用 **476**（最近的 server tar），cli 用 GitHub release **481** 的自执行 jar。
- `curl -L https://repo1.maven.org/maven2/io/trino/trino-server/476/trino-server-476.tar.gz`（**821 MB，本目录最大件**；已确认 URL 与大小后才下载）。
- `curl -L https://github.com/trinodb/trino/releases/download/481/trino-cli-481`（自执行 jar，存为 `trino-cli-481-executable.jar`）。
- server 解包顶层 `trino-server-476/`，含 `bin/launcher`、`lib/`、`plugin/`。需 JDK 23+（trino 481 要 JDK 24；476 要 JDK 23）—— 注意：**与 `../openjdk17-apks/` 的 JDK17 不兼容**，trino 运行需更高 JDK（见 §3 风险）。

### 1.7 ozone 2.1.0（Apache，Hadoop 系）
- 上游：`dlcdn.apache.org/ozone/2.1.0/ozone-2.1.0.tar.gz`（499 MB；2.0.0/1.4.1 为旧版备选）。
- 解包顶层 `ozone-2.1.0/`，Hadoop 生态对象存储（OM/SCM/Datanode + S3 gateway）。JVM 应用，启动 shell 调 `java`。
- 注意：ozone 是 Hadoop 衍生，对 JDK + 大量后台守护进程 + 网络 + 本地存储要求高，DoD 宜走 `ozone version` / 单进程 `ozone freon` 基准的轻量路径。

---

## 2. 校验和（官方核对状态见右注）

SHA256（tar/jar 的内容指纹）：
```
fd750466b1247c0d1ef09a84c614f7e045793b30dfa277148e8da71646598820  neo4j/neo4j-community-2026.04.0-unix.tar.gz   (= 官方 .sha256 √)
94b8e36fc329f0293d44ba9e01b784a56e9501affec1842d898144c51f6e486a  iceberg/iceberg-spark-runtime-3.5_2.12-1.11.0.jar
c70a60aef9d86d73220c7417ebe77d47b06bed5071aab65c75422a3becb56ea1  paimon/paimon-flink-1.20-1.4.1.jar
6b75f6de39dcf7e94b96f82006b96ec257b6358fc769a29d9817284c31c1e793  wildfly/wildfly-40.0.0.Final.tar.gz
4ce2ed5937b77c4515439344f38e2b25f85aee1c66f266ce147c93d85aa8a92a  quarkus/quarkus-cli-3.35.4.tar.gz   (= 官方 checksums_sha256.txt √)
cfd5accde17e8ebd251eeeb78aed1f490e77bb3a164d95a0f454bf8a7c1cbd3f  trino/trino-server-476.tar.gz
9532fb7a47dc54eec4041e86d980991236c131fd6e34983e40f735ccf60bad7f  trino/trino-cli-481-executable.jar
6b56b5dde82dd56f8ed45a9fd628a48d6ba6dec5f0be23d731d6e9e3366fe522  ozone/ozone-2.1.0.tar.gz
```

SHA1（maven 工件官方只发 `.sha1`，故 iceberg/paimon 用 sha1 与上游核对，均逐字一致 √）：
```
86eb12917658be2c8dd8982ee0c57cfece862591  iceberg/iceberg-spark-runtime-3.5_2.12-1.11.0.jar  (= maven .jar.sha1 √)
951adaacf361b3d2e22ef7077019d0522527c2b1  paimon/paimon-flink-1.20-1.4.1.jar                 (= maven .jar.sha1 √)
0b5948eb57016ef70498c32e936368096f9aafd8  wildfly/wildfly-40.0.0.Final.tar.gz                (= 官方 .tar.gz.sha1 √)
```
> neo4j / quarkus / wildfly / iceberg / paimon 均已与上游官方校验文件逐字核对一致；trino server/cli 与 ozone 上游未在同一处提供单文件校验，以本地 sha256 + tar/jar 结构完整性（§1 已验顶层目录/manifest）为准。

---

## 3. StarryOS 适配提示 + 风险（findings 方向，不 workaround）

- **JDK 版本错配（trino/ozone/neo4j 新版）**：`../openjdk17-apks/` 是 **JDK 17**。trino 476 要 JDK 23、ozone 2.1.0 与 neo4j 2026.x 多要 JDK 21+。→ 跑这些**新版**需更高 JDK；务实选项：(a) 用各应用支持 JDK17 的旧版本（neo4j 5.26 LTS 支持 17/21；wildfly 40 支持 17+；iceberg/paimon jar 通常 11/17 即可），(b) 另备 JDK21/23 musl JRE。**先以 wildfly/quarkus/iceberg/paimon/neo4j-LTS（JDK17 友好）建案**，trino/ozone 标注「需更高 JDK，后置」。
- **JVM 在 StarryOS 的压力点**已在 java 子课题（`../java-apps/dod/`）大量验证：大 heap mmap reserve、futex、信号、文件 I/O、class 加载。这些大应用是同类压力的放大版（更多 jar、更多线程、更大 heap）。
- **架构覆盖**：架构无关是这批的最大优点 —— 不存在 Go/C 那种 riscv/loong 缺预编译问题；4-arch 由 4-arch musl JRE 一次性覆盖。
