# java/openjdk17/packages/ — OpenJDK 17 包（按架构）

实际的软件包，**按 CPU 架构分目录**。每个包的下载 URL / 镜像 / 版本 / 依赖闭包 / 校验方式见上一级 `../SOURCES.md`（权威来源文件）。大文件经 Git LFS 跟踪。

```
packages/
├── x86_64/         36 个 musl apk（openjdk17-{jdk,jre,jre-headless,jmods} + 32 包依赖闭包 + gcompat）
├── aarch64/        36 个 musl apk（同 x86_64，aarch64 二进制）
├── loongarch64/    41 个 apk（通用 openjdk17 + LoongArch 原生变体 openjdk17-loongarch-* + gcompat）
├── riscv64/        musl 依赖 apk + glibc 回退方案（见下）+ gcompat + native-cross tar
│   └── debian-glibc/   4 个 Debian Ports glibc .deb（jdk/jdk-headless/jre/jre-headless）
└── meta-apkindex/  APKINDEX 索引（离线 apk 依赖解析用）
```

## 各架构落地方式

* **x86_64 / aarch64 / loongarch64（musl）**：`apk add openjdk17-jre` 或手动 `tar xzf <apk>`（apk 即 gzip tar）解到 rootfs。依赖闭包共 32 包（含 musl libc），列表见 `../SOURCES.md` §六。
* **loongarch64**：除通用包外还有 `openjdk17-loongarch-*`（LA64 ISA 优化变体，含 JIT 端口）；二选一。
* **riscv64**：无官方 musl 包。
  - 首选：`gcompat-1.1.0-r4.apk`（+ `libucontext`/`musl-obstack`）+ `bellsoft-jdk17.0.19+11-linux-riscv64.tar.gz`（glibc）。
  - 备选：`OpenJDK17U-jdk_riscv64_linux_hotspot_17.0.19_10.tar.gz`（Adoptium glibc）、`debian-glibc/*.deb`。
  - 真 musl 原生：`openjdk17-riscv64-musl-NATIVE-cross.tar.gz`。
  - 详见 `../SOURCES.md` §三的「riscv64 三种部署路径」。

## 校验

每个 apk/tar 的版本号即文件名的一部分；与 Alpine APKINDEX（`meta-apkindex/`）/ 各 vendor 官方校验文件核对。所有 URL 与镜像（USTC/TUNA/阿里/华为/上交/网易）见 `../SOURCES.md` §一、§四。
