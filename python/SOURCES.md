# python/ — 软件包来源 (provenance)

所有包均来自官方渠道,可校验可复现。详细 apk/wheel 清单与 sha256 见 `core/apks/SOURCES-python-apks.md` 及各子目录 `SOURCES.md` / 下载缓存内的 `PROVENANCE.md`。

## 基础运行时(core / frameworks / uv-venv 共用 base)
- **CPython 3.12.13** + **py3-numpy / py3-scikit-learn / py3-opencv / py3-scipy / py3-pandas** 等：**Alpine Linux v3.23**(main/community)musl-native apk,**四架构全有**(x86_64/aarch64/riscv64/loongarch64),零源码编译。
  - CDN:`https://dl-cdn.alpinelinux.org/alpine/v3.23/{main,community}/<arch>/`
  - ROUTE A(默认)= v3.23 = python 3.12.13,与 base 镜像同 branch,无 ABI 风险。
- **django / fastapi / uvicorn / strawberry / uv** 等纯 python:PyPI `py3-none-any` wheel(架构无关)。

## data/pyarrow
- **py3-pyarrow 21.0.0** + libarrow/libparquet/libthrift/libprotobuf 闭包:Alpine v3.23 community musl apk(四架构)。x86 另需内核 AVX/XCR0 修(见 README)。

## kconfiglib
- **kconfiglib.py v14.1.0**(单文件纯 python):来自 opensbi 源码树内置的 `scripts/Kconfiglib/kconfiglib.py`,架构无关。

## dod-frameworks/celery
- **celery 5.5.3 + flower 2.0.1 + kombu/billiard/amqp/...**:PyPI `py3-none-any` wheel(20 个,四架构通用)。tornado:x86/aa musllinux wheel,riscv/loong sdist 纯 python 回退。详见 `celery/prep-celery-rootfs.sh` 头注释。

## 获取方式
各 `prep-*.sh` 脚本从上述 Alpine CDN / PyPI 拉取并经 `debugfs -w` 注入 rootfs。大文件经 Git LFS;apk/wheel 可按上述 URL 复现下载。
