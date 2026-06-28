# OpenJDK 17 APK 包下载来源清单

适配目标：类 Alpine Linux OS (musl libc) × 4 个 CPU 架构

| 架构 | 状态 | openjdk17 主源 | 文件数 |
|------|------|----------------|--------|
| `x86_64`      | √ 完整 (APK) + gcompat | Alpine v3.22 community | 36 |
| `aarch64`     | √ 完整 (APK) + gcompat | Alpine v3.22 community | 36 |
| `loongarch64` | √ 完整 (APK) + LoongArch 原生变体 + gcompat | Alpine edge community | 41 |
| `riscv64`     | ! 依赖完整 (APK)；openjdk17 用 glibc tar.gz/deb 替代 + gcompat 兼容；附源码编译工具链 | Adoptium / BellSoft / Debian / 官方 backport 源码 | 35 + 4 deb + 6 编译资源 |

---

## 一、Alpine 官方 CDN 基础 URL

```
https://dl-cdn.alpinelinux.org/alpine/<branch>/<repo>/<arch>/<filename>.apk
```

- branch：`v3.22` (稳定) / `edge` (滚动)
- repo：`main` / `community`
- arch：`x86_64` / `aarch64` / `riscv64` / `loongarch64`

国内镜像（同结构）：
- USTC：`https://mirrors.ustc.edu.cn/alpine/...`
- 清华：`https://mirrors.tuna.tsinghua.edu.cn/alpine/...`
- 阿里：`https://mirrors.aliyun.com/alpine/...`

---

## 二、各架构包来源详表

### x86_64 / aarch64 / loongarch64 共用包（Alpine APK）

> 说明：以下 18 个公共包在四个架构里使用**相同文件名**，仅二进制不同。URL 中 `<ARCH>` 替换为 `x86_64`、`aarch64`、`riscv64`、`loongarch64` 即可。

| 包名 | 版本 | 来源分支 / repo | URL 模板 |
|------|------|------------------|----------|
| `alsa-lib` | `1.2.14-r2` | edge / main (历史快照) | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/alsa-lib-1.2.14-r2.apk` |
| `brotli-libs` | `1.2.0-r0` | edge / main (历史快照) | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/brotli-libs-1.2.0-r0.apk` |
| `ca-certificates` | `20260413-r0` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/ca-certificates-20260413-r0.apk` |
| `ca-certificates-bundle` | `20260413-r0` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/ca-certificates-bundle-20260413-r0.apk` |
| `freetype` | `2.14.3-r0` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/freetype-2.14.3-r0.apk` |
| `giflib` | `5.2.2-r1` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/giflib-5.2.2-r1.apk` |
| `lcms2` | `2.19-r0` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/lcms2-2.19-r0.apk` |
| `libbsd` | `0.12.2-r0` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/libbsd-0.12.2-r0.apk` |
| `libbz2` | `1.0.8-r6` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/libbz2-1.0.8-r6.apk` |
| `libcrypto3` | `3.5.6-r0` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/libcrypto3-3.5.6-r0.apk` |
| `libffi` | `3.5.2-r1` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/libffi-3.5.2-r1.apk` |
| `libmd` | `1.2.0-r0` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/libmd-1.2.0-r0.apk` |
| `libpng` | `1.6.58-r1` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/libpng-1.6.58-r1.apk` |
| `libtasn1` | `4.21.0-r0` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/libtasn1-4.21.0-r0.apk` |
| `libxau` | `1.0.12-r0` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/libxau-1.0.12-r0.apk` |
| `libxcb` | `1.17.0-r2` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/libxcb-1.17.0-r2.apk` |
| `libxdmcp` | `1.1.5-r1` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/libxdmcp-1.1.5-r1.apk` |
| `libxi` | `1.8.2-r0` | edge / main (历史快照) | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/libxi-1.8.2-r0.apk` *（当前 edge 已更新至 1.8.3-r0）* |
| `libxrender` | `0.9.12-r0` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/libxrender-0.9.12-r0.apk` |
| `libxtst` | `1.2.5-r0` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/libxtst-1.2.5-r0.apk` |
| `musl` | `1.2.6-r2` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/musl-1.2.6-r2.apk` |
| `p11-kit` | `0.25.5-r2` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/p11-kit-0.25.5-r2.apk` |
| `p11-kit-trust` | `0.25.5-r2` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/p11-kit-trust-0.25.5-r2.apk` |
| `zlib` | `1.3.2-r0` | edge / main | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/zlib-1.3.2-r0.apk` |
| `java-cacerts` | `1.1-r0` | edge / community | `https://dl-cdn.alpinelinux.org/alpine/edge/community/<ARCH>/java-cacerts-1.1-r0.apk` |
| `java-common` | `1.0-r1` | edge / community (历史快照) | `https://dl-cdn.alpinelinux.org/alpine/edge/community/<ARCH>/java-common-1.0-r1.apk` *（当前 edge 已 1.0-r2）* |

### 各架构差异（libjpeg-turbo / libx11 / libxext）

| 包名 | x86_64 | aarch64 | loongarch64 | riscv64 |
|------|--------|---------|--------------|---------|
| `libjpeg-turbo` | `3.1.2-r0` (edge 历史) | `3.1.3-r0` (edge 当前) | `3.1.3-r0` (edge 当前) | `3.1.3-r0` (edge 当前) |
| `libx11` | `1.8.12-r1` (edge 历史) | `1.8.13-r0` (edge 当前) | `1.8.13-r0` (edge 当前) | `1.8.13-r0` (edge 当前) |
| `libxext` | `1.3.6-r2` (edge 历史) | `1.3.6-r2` (edge 历史) | `1.3.6-r2` (edge 历史) | `1.3.7-r0` (edge 当前) |

URL 同样替换 `<ARCH>` 和版本号即可。

---

## 三、openjdk17 核心包来源（按架构）

### x86_64

| 包 | 版本 | 来源 URL |
|----|------|----------|
| `openjdk17-jdk` | `17.0.18_p8-r0` | `https://dl-cdn.alpinelinux.org/alpine/v3.22/community/x86_64/openjdk17-jdk-17.0.18_p8-r0.apk` |
| `openjdk17-jre` | `17.0.18_p8-r0` | `https://dl-cdn.alpinelinux.org/alpine/v3.22/community/x86_64/openjdk17-jre-17.0.18_p8-r0.apk` |
| `openjdk17-jre-headless` | `17.0.18_p8-r0` | `https://dl-cdn.alpinelinux.org/alpine/v3.22/community/x86_64/openjdk17-jre-headless-17.0.18_p8-r0.apk` |
| `openjdk17-jmods` | `17.0.18_p8-r0` | `https://dl-cdn.alpinelinux.org/alpine/v3.22/community/x86_64/openjdk17-jmods-17.0.18_p8-r0.apk` |

升级版（当前 edge）：把 `v3.22` 改成 `edge`，版本号改成 `17.0.19_p10-r0`。

### aarch64

同 x86_64，URL 中 `x86_64` → `aarch64`，文件同名同版本：
- `https://dl-cdn.alpinelinux.org/alpine/v3.22/community/aarch64/openjdk17-jdk-17.0.18_p8-r0.apk`
- `…/openjdk17-jre-17.0.18_p8-r0.apk`
- `…/openjdk17-jre-headless-17.0.18_p8-r0.apk`
- `…/openjdk17-jmods-17.0.18_p8-r0.apk`

### loongarch64

**两套包都已下载**：

1. 通用 openjdk17（同 x86_64 版本）：
   - `https://dl-cdn.alpinelinux.org/alpine/v3.22/community/loongarch64/openjdk17-jdk-17.0.18_p8-r0.apk`
   - `…/openjdk17-jre-17.0.18_p8-r0.apk`
   - `…/openjdk17-jre-headless-17.0.18_p8-r0.apk`
   - `…/openjdk17-jmods-17.0.18_p8-r0.apk`

2. **LoongArch 原生优化变体 `openjdk17-loongarch`**（仅 edge/community 提供，含 LA64 ISA 指令集补丁）：
   - `https://dl-cdn.alpinelinux.org/alpine/edge/community/loongarch64/openjdk17-loongarch-17.0.17_p10-r0.apk`
   - `…/openjdk17-loongarch-jdk-17.0.17_p10-r0.apk`
   - `…/openjdk17-loongarch-jre-17.0.17_p10-r0.apk`
   - `…/openjdk17-loongarch-jre-headless-17.0.17_p10-r0.apk`
   - `…/openjdk17-loongarch-jmods-17.0.17_p10-r0.apk`

### riscv64 ! 特殊情况

**没有 Alpine 官方 APK**。经全面搜索（含 GitHub 代码搜索、Docker Hub、各 JDK 厂商 API）：

| 来源 | 是否有 musl + riscv64 + JDK 17 |
|------|-------------------------------|
| Alpine edge community / main / testing | × 仅 `openjdk21` |
| Alpine v3.20–v3.24 | × 仅 `openjdk21` |
| Alpine 官方 APKBUILD | × 明确禁用：`arch="all !x86 !armhf !armv7 !riscv64"` |
| Wolfi (Chainguard) | × riscv64 仓库 404 |
| Chimera Linux | × 只有 `openjdk21-bootstrap` |
| postmarketOS / Adelie | × |
| BellSoft Liberica musl | × musl 仅 arm/x86 |
| SapMachine | × riscv64 完全没有；musl 仅 x64 |
| Eclipse Temurin (Adoptium) | × 只有 glibc |
| Amazon Corretto 17 | × 无 riscv64 |
| Microsoft Build of OpenJDK | × 无 riscv64 |
| Tencent Kona 17 | × 无 riscv64 |
| Alibaba Dragonwell 17 | × 无 riscv64 |
| 华为 BiSheng JDK 17 | × 仓库无 release |
| JetBrains Runtime | × 无 riscv64 |
| Eclipse OpenJ9 | × 无 riscv64 |
| Debian / Ubuntu Ports | ! 有 .deb 但是 **glibc** |
| stagex/Docker Hub | ! 仅 musl x86_64 |

**根本原因**：OpenJDK 上游的 RISC-V port 在 **JDK 21 才合并**，JDK 17 没有官方 RISC-V 支持。只有 `openjdk/riscv-port-jdk17u` 官方 backport 源码项目，**无任何 vendor 发布预编译产物**。

#### 已下载的 glibc 替代方案

| 包 | 版本 | libc | 来源 |
|----|------|------|------|
| `OpenJDK17U-jdk_riscv64_linux_hotspot_17.0.19_10.tar.gz` (190MB) | 17.0.19+10 | **glibc** | `https://github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.19%2B10/OpenJDK17U-jdk_riscv64_linux_hotspot_17.0.19_10.tar.gz` |
| `bellsoft-jdk17.0.19+11-linux-riscv64.tar.gz` (181MB) | 17.0.19+11 | **glibc** | `https://github.com/bell-sw/Liberica/releases/download/17.0.19+11/bellsoft-jdk17.0.19+11-linux-riscv64.tar.gz` |
| `bellsoft-jre17.0.19+11-linux-riscv64.tar.gz` (38MB) | 17.0.19+11 | **glibc** | `https://github.com/bell-sw/Liberica/releases/download/17.0.19+11/bellsoft-jre17.0.19+11-linux-riscv64.tar.gz` |
| `debian-glibc/openjdk-17-jdk_17.0.19~9ea-1_riscv64.deb` | 17.0.19~9ea-1 | **glibc** | `http://deb.debian.org/debian/pool/main/o/openjdk-17/openjdk-17-jdk_17.0.19~9ea-1_riscv64.deb` |
| `debian-glibc/openjdk-17-jdk-headless_17.0.19~9ea-1_riscv64.deb` (68MB) | 17.0.19~9ea-1 | **glibc** | 同上目录 |
| `debian-glibc/openjdk-17-jre_17.0.19~9ea-1_riscv64.deb` | 同 | **glibc** | 同上 |
| `debian-glibc/openjdk-17-jre-headless_17.0.19~9ea-1_riscv64.deb` (42MB) | 同 | **glibc** | 同上 |

#### gcompat 兼容层（让 glibc 二进制在 musl 上跑）

已为所有 4 个架构下载（用于 riscv64 主要场景）：

| 包 | 版本 | 来源 |
|----|------|------|
| `gcompat-1.1.0-r4.apk` | 1.1.0-r4 | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/gcompat-1.1.0-r4.apk` |
| `libucontext-1.5.1-r0.apk` (gcompat 依赖) | 1.5.1-r0 | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/libucontext-1.5.1-r0.apk` |
| `musl-obstack-1.2.3-r2.apk` (gcompat 依赖) | 1.2.3-r2 | `https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/musl-obstack-1.2.3-r2.apk` |

用法：`apk add gcompat`，然后 Adoptium / BellSoft / Debian 的 glibc 二进制可直接跑（部分 JNI/线程功能可能受限）。

#### 源码编译资源（已放在 `riscv64/source-build/`）

| 文件 | 说明 |
|------|------|
| `APKBUILD.alpine-openjdk17` | Alpine 官方 openjdk17 APKBUILD（编译模板，需移除 `!riscv64` 排除并加 musl flag） |
| `ppc64le.patch` | Alpine 用的 ppc64le 兼容补丁（参考其风格写 riscv64 补丁） |
| `openjdk21-jdk-21.0.11_p10-r0.apk` | bootstrap JDK 21（musl/riscv64 原生，用于自举编译 JDK 17） |
| `openjdk21-jre-headless-21.0.11_p10-r0.apk` | 同上配套 |
| `openjdk21-jmods-21.0.11_p10-r0.apk` | 同上配套 |
| `BUILD_FROM_SOURCE.md` | 完整编译指南（克隆、configure 命令、make、打包） |

源码（**未下载**，体积 ~100MB，建议在编译机直接 clone）：

```
# 官方 OpenJDK RISC-V port 项目（JDK 17u backport）
https://github.com/openjdk/riscv-port-jdk17u            (项目页 https://openjdk.org/projects/riscv-port)
  └─ 默认分支 riscv-port
  └─ 最新 stable tag: jdk-17+35
  └─ 源码下载：https://github.com/openjdk/riscv-port-jdk17u/archive/refs/tags/jdk-17+35.tar.gz
  └─ 克隆：git clone --depth=1 --branch riscv-port https://github.com/openjdk/riscv-port-jdk17u.git
```

#### riscv64 三种部署路径（按推荐度排序）

| # | 方案 | 优点 | 缺点 |
|---|------|------|------|
| 1 | **gcompat + Adoptium/BellSoft glibc tar.gz** | 立即可用，已全部下载 | 依赖 gcompat 兼容，部分 JNI/native 行为可能异常 |
| 2 | **直接用 openjdk21 musl APK**（Alpine 官方） | 原生 musl，无兼容层 | 是 JDK 21 不是 JDK 17（如必须 17 不可用） |
| 3 | **源码编译 musl 版 openjdk17-riscv64**（最末选择） | 真正 musl + JDK 17 + 原生 riscv64 | 需要 ~6h 编译 + 编写 musl patch；详见 `riscv64/source-build/BUILD_FROM_SOURCE.md` |

---

## 四、备用镜像源

如 dl-cdn 限速或不可达，所有 URL 中的 `https://dl-cdn.alpinelinux.org/alpine/` 可替换为：

| 镜像 | URL 前缀 |
|------|---------|
| 阿里云 | `https://mirrors.aliyun.com/alpine/` |
| 中科大 USTC | `https://mirrors.ustc.edu.cn/alpine/` |
| 清华 TUNA | `https://mirrors.tuna.tsinghua.edu.cn/alpine/` |
| 华为云 | `https://mirrors.huaweicloud.com/alpine/` |
| 上海交大 | `https://mirror.sjtu.edu.cn/alpine/` |
| 网易 | `https://mirrors.163.com/alpine/` |
| 官方主站 | `https://dl-cdn.alpinelinux.org/alpine/` |

历史快照（含 edge 历史版本）：`https://archive.alpinelinux.org/alpine/`

---

## 五、APKINDEX 索引（依赖解析必备）

```
https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/APKINDEX.tar.gz
https://dl-cdn.alpinelinux.org/alpine/edge/community/<ARCH>/APKINDEX.tar.gz
```

用于离线 apk install / `apk fetch -R` 解析。

---

## 六、依赖闭包（用于校验）

`openjdk17-jdk` + `openjdk17-jre` 的完整传递依赖（共 32 包，含 musl libc）：

```
alsa-lib brotli-libs ca-certificates ca-certificates-bundle freetype giflib
java-cacerts java-common lcms2 libbsd libbz2 libcrypto3 libffi libjpeg-turbo
libmd libpng libtasn1 libx11 libxau libxcb libxdmcp libxext libxi libxrender
libxtst musl openjdk17-jdk openjdk17-jmods openjdk17-jre openjdk17-jre-headless
p11-kit p11-kit-trust zlib
```

> 注：解析方式 = 抓取每个 .apk 的 `.PKGINFO`，对 `depend = so:libXXX.so.N` 等通过 APKINDEX 的 `p:` (provides) 字段反查所属包，递归闭包。
