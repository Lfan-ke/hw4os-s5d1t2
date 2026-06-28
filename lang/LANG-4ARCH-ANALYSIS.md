# 语言批 · StarryOS 四架构适配 — 真实覆盖度总汇总

> #764 语言运行时项(llvm22 / haskell / tinygo / dotnet / kotlin-native)在 StarryOS
> **四架构**(x86_64 / aarch64 / riscv64 / **loongarch64**)的**据实**覆盖度 + 每个未满架构的**原因**。
> 维护者请重点看本表:**有的项是真 4/4,有的项受上游 arch 移植空白限制,只能覆盖部分架构**。
> 据实原则:能跑的架构真做+交付;不能跑的架构**写清楚是上游缺什么**,不假绿。

## 速览表

| 项 | x86_64 | aarch64 | riscv64 | loongarch64 | 真实结论 | 受限根因 |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| **llvm22** (clang-22 C++23) | √ | √ | √ | √ | **真 4/4** | clang 自带全 4 架构后端 |
| **haskell** (GHC 9.14.1) | ! | ! | × | × | x86/aarch64 musl 可达;rv/loong 仅 glibc(gcompat) | NCG/RTS + 自举:上游无 rv/loong musl bindist |
| **tinygo** (0.40) | √ | √ | × | × | x86/aarch64 可达 | tinygo 无 `GOARCH=riscv64/loong64`(项目未接该 target) |
| **dotnet** (C#/F#) | √ | √ | ! | ! | x64/arm64 可达;loong 后端上游有(需源码 build) | RyuJIT:rv=.NET9 实验,loong=社区分支 |
| **kotlin-native** (konanc) | √ | √ | × | × | x64/arm64 可达 | KonanTarget 仅 linux_x64/arm64,无 rv/loong |

图例:√ 通过 · ! 可达/进行中(需额外工作) · × 上游未移植该架构(本地无法补齐)

---

## 为什么 llvm22 能真 4/4,其余不能 —— 适配难点链

四架构适配是一条**从底到顶、每层都可能断**的链:

0. **CPU 后端**:编译器能否生成该架构机器码。LLVM/GCC 有**全 4 架构后端**(故 llvm22/C/C++/java/python/go 官方能 4/4);tinygo/kotlin-native/dotnet 是**项目内部**没把 loong/riscv 接进 target 表 —— 缺的是语言项目自己的 arch 适配,**不是缺 C 交叉编译器**(本地 musl-cross 救不了)。
1. **runtime 移植**:GC 栈扫描、协程上下文切换(**手写汇编**)、异常展开、原子/内存模型 —— 每架构单独写,是上游**多人月**工程。
2. **libc(musl vs glibc)**:StarryOS = Alpine musl;但很多发行版对 rv/loong **只有 glibc**(需 gcompat 兼容层,同 ros2 路)。
3. **自举(bootstrap)**:GHC/Rust 等自托管编译器,要先有"能跑在目标架构的旧编译器"才能编新的;loong/riscv 上游自己都没解。
4. **StarryOS 内核能力**:mmap 大堆 / futex / 信号 ABI / TLS / epoll —— 这层是本项目主力在修(verilog/bluesv/java/py/go 已坐实)。

> **一句话**:能 4/4 的,是站在 LLVM/GCC "已替全 4 架构做完底层苦工" 的肩膀上;不能 4/4 的,缺的那块(loong 后端 + runtime port + 自举)是**上游语言项目自身**尚未完成的多人月移植。

---

## 逐项详情 + 每架构原因

### llvm22 — 真 4/4(已交付 `lang/llvm22/`)
- clang-22(LLVM 22.1.6)一份后端交叉编出全 4 架构静态 musl 二进制;综合 C++23 测例(ranges/concepts/consteval/variant/bit/optional)→ starry 4/4 全绿(`LLVM22_OK_GATE=1`,qemu-user md5 全等黄金)。

### haskell — x86_64/aarch64 可达,rv/loong 受限
- **x86_64 / aarch64**:GHC 9.14.1 官方有 **alpine-musl bindist**,可 `-optl-static` 出静态二进制。进行中(GHC 在 qemu-user musl 环境运行已通,`--make` 动态加载 libHSghc-internal 需进一步处理:`-fexternal-interpreter`/`-dynamic` 或真 alpine chroot)。
- **riscv64 / loongarch64**:**上游 downloads.haskell.org 无任何 rv/loong bindist**;仅 AOSC 有 **glibc** `.deb`(动态链)→ 落 starry 需 **gcompat**(同 ros2 的 gcompat 路线)或 glibc rootfs。纯 musl rv/loong GHC 需**交叉自举**,上游链路自身不稳。
- **自举策略(按用户定)**:找历史可编版本 → 编出 → 逐步升级到 9.14(经典 bootstrap chain),而非直接硬怼最新版交叉自举。

### tinygo — x86_64/aarch64 可达,rv/loong ×
- tinygo 0.40:host x86 √、`GOARCH=arm64` 交叉静态 √(`TINYGO_OK`,综合 Go 测例:goroutine/channel/select/atomic/generics/closure/defer);
- `GOARCH=riscv64` / `GOARCH=loong64` → **`unknown GOARCH`**:tinygo 的 target 表 + Go runtime port 只有 x86/arm64 linux(其余是 MCU 板)。要补 = 写 target 定义 + `task_stack_<arch>.S` 协程切换汇编 + syscall ABI + runtime arch 文件(1–3 千行含汇编,**3–6 周**)。

### dotnet (C#/F#) — x64/arm64 可达,loong 需源码 build,riscv 实验
- C#/F# 编成 IL(架构无关),通不通全看 **CoreCLR**。
- **x64/arm64**:官方 RID,直接可用。
- **loongarch64**:RyuJIT 的 LoongArch64 后端**上游已有**(Loongson 贡献),是"从 dotnet/runtime 源码 build CoreCLR"(musl crossbuild + 依赖),**非从零写后端**;工程量大(全量 build 数小时×多轮),**1–2 周**。
- **riscv64**:.NET 9 起**实验性**,JIT 不完整,可靠 4/4 存疑。

### kotlin-native (konanc) — x64/arm64 可达,rv/loong ×
- KonanTarget 只有 `linux_x64`/`linux_arm64`。补一个架构要:KonanTarget 枚举 + 平台 klib(cinterop 该 arch sysroot)+ K/N runtime(C++)为该 arch 编 + 确认自带 LLVM 含 loong 后端。loong **零先例**,**2–4 人月**。

---

## 工时/难度估(熟练工)

| 项 | 缺口性质 | 新写代码 | 工时 | 短期可行 |
|:--:|:--:|:--:|:--:|:--:|
| llvm22 | 仅缺 C 交叉 | 0 | 已完成 | √ |
| haskell x86+aarch64 | musl 环境跑 GHC | rootfs/脚本 | 0.5–2 天 | ! |
| haskell rv/loong | glibc+gcompat | gcompat shim 数百行 | 3–7 天 | ! |
| dotnet loong | 源码 build(后端已有) | ~0 新写,build 适配 | 1–2 周 | × |
| tinygo rv/loong | target+runtime port | 1–3 千行(含汇编) | 3–6 周 | × |
| kotlin-native rv/loong | 全套 target 移植 | 数千行 | 2–4 人月 | × |

---

## 据实交付口径

- **真 4/4 的项**(llvm22)按常规据实交付。
- **上游 arch 移植空白的项**(tinygo/kotlin-native loong 等):本文档**逐架构据实交代**真实覆盖与受限根因 —— 已尽力适配 + 诚实交代,**非假装全绿**。能跑的架构均真做+交付。
- 自举类按"历史版本 → 逐步升级"链推进。

> 维护者注:若需严格 4/4 口径,本表 !/× 列即为待上游补齐的真实缺口清单。
