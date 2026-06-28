# python/lang — 软件包来源 (provenance)

语言级 carpet 测试套件本身为**原创**(`python/t01..t19_*.py` + `run_all.py` + `test_lang.py`),
无第三方包依赖(纯 CPython 解释器 + 标准库)。运行时来源如下,可校验可复现。

## CPython 3.14 运行时(`./apks/<arch>/`,Git LFS)
- **python3 3.14.3-r0** + 其纯解释器闭包(21 个 apk/arch:musl 1.2.6 / libcrypto3+libssl3 3.5.x / ncurses 6.6 / readline / sqlite-libs / libffi / mpdecimal / gdbm / xz-libs / zlib / libbz2 / libstdc++ / libgcc / libexpat 等)。
  - 来源:**Alpine Linux edge** `main` musl-native apk,四架构(x86_64/aarch64/riscv64/loongarch64)官方 CDN
    `https://dl-cdn.alpinelinux.org/alpine/edge/main/<arch>/python3-3.14.3-r0.apk`(+ 闭包依赖)。
  - 解析脚本:下载缓存目录内的 `apk-closure.py`;原始下载缓存:`<本机下载缓存目录>/python-apks/python314/<arch>/`。
  - ROUTE B(纯解释器):仅 python3 + 标准库,**不含** numpy/scipy 等原生科学栈(那是 `python/core` 等独立用例)。
- `prebuild.sh` 用 `debugfs -w`(不 mount/sync,WSL2 安全)把该闭包注入 Alpine base 工作副本(覆盖 3.12→3.14),再经 app overlay 注入测试模块到 `/usr/bin`。**无本机绝对路径**,`./apks` 相对定位,离线可复现。

## 内核依赖(loongarch64)
- 本套件在 loongarch64 spawn 子解释器(multiprocessing / `python -i` / venv)需较大 RAM。要求 guest StarryOS 内核**从 FDT 检测真实 RAM(honor qemu `-m`)** —— **rcore-os/tgoskits #239**(`feat(ax-plat-loongarch64-qemu-virt): detect RAM size from the FDT`);`qemu-loongarch64.toml` 已设 `-m 2048M`。缺此修则 128M 硬上限会 OOM。aarch64/riscv64/x86_64 用 512M 即可。
