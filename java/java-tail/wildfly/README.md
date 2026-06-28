# java-tail/wildfly/ — WildFly 40.0.0.Final（完整 Jakarta EE 10 app server）

**marker `WILDFLY_OK`** · server 用例（绑 `127.0.0.1:8080`，**本组最重**）· `timeout=4500` · #764 `wildfly` !

启动**真正的** app server：`bin/standalone.sh` → `jboss-modules.jar` → `org.jboss.as.standalone`。验证模块化 classloader + Undertow web 子系统 + 多分钟 -Xint 启动。

## 内容

```
wildfly/
├── qemu-{x86_64,aarch64,riscv64,loongarch64}.toml   harness（后台启 server + 等 banner + 4 断言 + kill）
├── prep-wildfly-rootfs.sh                            构建 rootfs-<arch>-wildfly.img（resize 5G）
└── package/wildfly-40.0.0.Final.tar.gz               官方二进制发行包（251 MB）
```

**payload**：官方 `wildfly-40.0.0.Final.tar.gz` 解到 `/opt/wildfly-40.0.0.Final`。`bin/standalone.sh` + `bin/common.sh` 是 POSIX `#!/bin/sh`（已验无 `[[`/`local`/数组/`function` 关键字）→ 在基座的 **busybox ash 下直接跑，无需 bash**。WildFly 40 支持 JDK 17+。来源 + sha1（`0b5948eb...`，与官方 `.tar.gz.sha1` 一致）见 `../../bigapps/SOURCES.md`。

## 断言（gate：`SRV_READY && PASS==TOTAL && TOTAL==4`）

后台 `standalone.sh -b 127.0.0.1`，等 `WFLYSRV0025`「started in」banner（完整 EE 启动在 -Xint + 模拟架构下慢，≤600s 轮询），再 `nc` 探 :8080：

1. HTTP `200 OK` 状态行（Undertow web 子系统）
2. welcome body 含 `Welcome to WildFly`
3. welcome body 含 `Your WildFly instance is running.`
4. 启动日志含 `40.0.0.Final`（in-gate 版本断言）

**关停 = `kill $PID`**（关键设计决定）：**不用** `jboss-cli.sh :shutdown`（它 fork 第二个完整 JVM，翻倍 -Xint 启动开销，且管理端口 SSL/SASL 握手在模拟单核下超时）。`pkill -f org.jboss.as.standalone` 作双保险。

## 风险（诚实）

最重用例：真 app-server 进程 + 模块化 classloader + 多分钟 -Xint 启动。某架构若 `timeout=4500` 内启动不完，会报 `WILDFLY_NOT_READY` + 启动日志尾巴（**诚实失败，不发 `WILDFLY_OK=1`**）。先盯 loongarch/riscv64 启动时间；调 timeout 或 toml `-m` 是杠杆（loongarch toml 给 8G RAM 留了余量）。

## 怎么手动在 qemu v10 里跑

```
cd <tgoskits>
bash <本仓库>/java/java-tail/wildfly/prep-wildfly-rootfs.sh x86_64    # 产出 rootfs-x86_64-wildfly.img（5G）
cargo xtask starry test qemu --arch x86_64 -g stress -c wildfly-0    # 期望 WILDFLY_OK=1（可能要数分钟）
```

进 guest shell 手动起 + 探：

```sh
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk
printf '/lib\n/usr/lib\n%s/lib\n%s/lib/server\n' "$JAVA_HOME" "$JAVA_HOME" > /etc/ld-musl-x86_64.path
export JAVA_OPTS="-Xint -Djava.net.preferIPv4Stack=true"
WF=/opt/wildfly-40.0.0.Final
sh $WF/bin/standalone.sh -b 127.0.0.1 >/tmp/wf.out 2>&1 &
until grep -aq WFLYSRV0025 /tmp/wf.out; do sleep 2; done        # 等 "started in" banner（慢）
printf 'GET / HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n' | nc 127.0.0.1 8080   # 期望 200 OK + Welcome to WildFly
grep -a 40.0.0.Final /tmp/wf.out                                # 版本断言
kill %1; pkill -f org.jboss.as.standalone
```
