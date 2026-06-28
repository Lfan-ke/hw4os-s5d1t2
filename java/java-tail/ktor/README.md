# java-tail/ktor/ — Ktor 2.3.x Kotlin/JVM async web（Netty 引擎）

**marker `KTOR_OK`** · server 用例（绑 `127.0.0.1:18082`）· `timeout=3000` · #764 `ktor` !

验证 Kotlin 协程运行时 + Netty NIO 事件循环 + Ktor 路由/插件管线 + kotlinx.serialization JSON 内容协商，端到端跑在 StarryOS 网络栈（[#223](https://github.com/rcore-os/tgoskits/issues/223)/[#225](https://github.com/rcore-os/tgoskits/issues/225)）的 IPv4 回环上。

## 内容

```
ktor/
├── qemu-{x86_64,aarch64,riscv64,loongarch64}.toml   harness（启 jar + busybox nc 驱动 7 断言 + gate）
└── prep-ktor-rootfs.sh                              构建 rootfs-<arch>-ktor.img（注入 ktor-demo.jar）
```

**payload**：host 编译的 fat jar `../../dod-frameworks/jars/ktor-demo.jar`（16 MB；Kotlin 在 host 编译以绕开 starry kotlinc 崩溃 #237），注入 guest `/root/ktor/ktor-demo.jar`。无需额外库（jar 已捆 Ktor+Netty+kotlinx.serialization+Kotlin stdlib/coroutines）。

> prep 脚本里 payload 路径硬编码为 `$DL/java-apps/dod/jars/ktor-demo.jar`；复用本仓库时把脚本 `DL=` 指向 `java/dod-frameworks/`（或把 jar 放回原路径）。

## 断言（gate：`SRV_READY && PASS==TOTAL && TOTAL==7`）

jar 启动后绑 :18082 打印 `KTOR_READY` 然后阻塞。harness 用 busybox `nc` 发裸 HTTP/1.0，断言精确状态行 + body + 协商出的 `application/json` content-type（真栈证明），再 `kill` JVM：

1. `GET /ping` → `200 OK` body `KTOR_PONG`
2. `GET /add/3/4` → `200 OK` body `{"a":3,"b":4,"sum":7}`
3. `GET /add/foo/4` → `400 Bad` body `BAD_INT`
4. `GET /echo?msg=hello` → `200 OK` body `{"echo":"hello","len":5}`
5. `POST /sum {"nums":[1,2,3,4]}` → `200 OK` body `{"total":10,"count":4}`
6. `GET /nope` → `404 Not Found`
7. `GET /add/3/4` content-type 必须是 `application/json`（ContentNegotiation 插件真栈证明）

## 怎么手动在 qemu v10 里跑

### 方式 A：跑整套 harness（推荐）

```
cd <tgoskits>
bash <本仓库>/java/java-tail/ktor/prep-ktor-rootfs.sh x86_64    # 产出 rootfs-x86_64-ktor.img
cargo xtask starry test qemu --arch x86_64 -g stress -c ktor-0  # 期望 KTOR_OK=1
```

### 方式 B：进 guest shell 手动起服务 + 探测

启动后落到 `root@starry:/root #`。逐步：

```sh
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk
printf '/lib\n/usr/lib\n%s/lib\n%s/lib/server\n' "$JAVA_HOME" "$JAVA_HOME" > /etc/ld-musl-x86_64.path
# 后台起服务，等 KTOR_READY
$JAVA_HOME/bin/java -Xint -Xms64m -Xmx256m -jar /root/ktor/ktor-demo.jar >/tmp/ktor.out 2>&1 &
until grep -q KTOR_READY /tmp/ktor.out; do sleep 1; done    # -Xint + 模拟架构预热，可能要 ~一两分钟
# 手动探一条路由（busybox nc，裸 HTTP/1.0）
printf 'GET /add/3/4 HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n' | nc 127.0.0.1 18082
#   期望首行 200 OK，body 行 {"a":3,"b":4,"sum":7}，Content-Type: application/json
kill %1
```
