# textual 来源 / provenance

- 包: textual 8.2.7 + 依赖(rich 15.0.0, markdown-it-py 4.2.0, mdit-py-plugins 0.6.1, mdurl 0.1.2, pygments 2.20.0, typing-extensions 4.15.0, platformdirs 4.10.0, linkify-it-py 2.1.0, uc-micro-py 2.0.0)
- 全部 `py3-none-any`(noarch)wheel,PyPI;架构无关,四架构由 base musl CPython 3.12 覆盖。
- 取材:`pip download --only-binary=:all: --python-version 312 textual`(wheels/ 已附)。
- base 镜像:`rootfs-<arch>-python.img`(python-apks/prep-python-rootfs.sh,CPython 3.12.13 musl)。
