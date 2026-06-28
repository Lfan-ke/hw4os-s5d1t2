# java/complex-demo/ — 自写复杂 Java 17 程序

一个**自己编写**的复杂 Java 17 程序，用来验证 StarryOS 上的 JVM 能跑现代 Java 语言特性 + 标准库，并且能被 maven 和 gradle **离线**构建（无任何外部依赖）。它是 `../openjdk17/` 对应用例（`openjdk17-0`）里 `CDEMO` 段和 `MVN`/`GRADLE` 段的被测对象。

来源：**本地编写**（非第三方下载）。下载/编写日期见上级 `../toolchain/SOURCES.md`。

## 内容

```
complex-demo/
├── src/main/java/demo/
│   ├── App.java        主程序：sealed+streams / concurrent / reflection / regex+time / NIO 往返
│   └── Shapes.java     sealed interface + record 实现（Circle/Square）
├── pom.xml             maven 构建（产出 complex-demo.jar，Main-Class=demo.App）
├── build.gradle        gradle 构建（同上）
├── settings.gradle
└── complex-demo.jar    预构建产物（host 编译；也可在 starry 上现场重建）
```

覆盖的语言/库特性：records · sealed 接口与 permitted 实现 · pattern matching · Stream · 并发（线程 + AtomicLong + 合并）· 反射 · 正则 + `java.time` · NIO 文件往返。

## 标记 / 期望输出

程序首行打印 `CDEMO_START java=<ver> arch=<arch>`，各阶段打印 `CDEMO_AREA/CDEMO_CONC/CDEMO_REFLECT/CDEMO_REGEX_TIME/CDEMO_NIO`，全部成功后打印 **`CDEMO_DONE`**。用例 gate 用 `grep -q CDEMO_DONE` 判定。

## 怎么用

**host ground-truth**（构建机上）：

```sh
javac --release 17 -d out src/main/java/demo/*.java
java -cp out demo.App        # 期望末行 CDEMO_DONE
```

**在 StarryOS guest 里**（jar 已注入 `/root/complex-demo.jar`，源码树在 `/root/complex-demo/`）：

```sh
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk
# 1) 直接跑预构建 jar
$JAVA_HOME/bin/java -Xint -Xmx256m -Xms32m -jar /root/complex-demo.jar      # 期望 CDEMO_DONE
# 2) 或现场用 maven / gradle 离线重建（见 ../toolchain/README.md），再跑产出的 jar
```
