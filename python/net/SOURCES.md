# python/net — 来源与构建说明

Python Web 框架地毯测试。上游载体为 `apps/starry/python-net`
（PR [rcore-os/tgoskits#1441](https://github.com/rcore-os/tgoskits/pull/1441)）。本目录是 CI-like
可构建交付：提交 carpet 源码 + 运行配置 + 纯 python wheel，运行时 CPython 与框架 apk 由 prebuild
按下表从 Alpine 仓库 fetch-on-build，不随仓库 bundle 解释器 / 大体积 native 闭包。

## 运行时与框架来源

| 组件 | 来源 |
|:--|:--|
| CPython 3.14（各架构 musl） | Alpine edge `main` 的 `python3`（3.14.x），由 `apps-starry/prebuild.sh` 经 `qemu-user-static` `apk add` 进 base-rootfs staging |
| Django / FastAPI / uvicorn / Pydantic(+原生 pydantic-core) / starlette / graphql-core / asgiref / python-dateutil / packaging | Alpine edge `community` 的 `py3-*` / `uvicorn` apk，由 prebuild `apk add`（apk 按 target 架构解析每包，含原生 pydantic-core 的 musl `.so`） |
| strawberry-graphql 0.316.0 | PyPI `py3-none-any` wheel（Alpine 无 apk），随 `assets/` 提交，prebuild 解压进 site-packages |
| cross-web 0.7.0（strawberry 运行时依赖，PyPI-only） | PyPI `py3-none-any` wheel，随 `assets/` 提交，prebuild 解压进 site-packages |

注：strawberry-graphql 的运行时依赖中 `python-dateutil` / `packaging` 走 apk，`cross-web` 无 apk
故随 wheel bundle；`graphql-core` / `typing-extensions` 由框架 apk 闭包提供。

## 运行

```
cargo xtask starry app qemu -t python-net --arch <x86_64|aarch64|riscv64|loongarch64>
```

四架构单核实测 `PYTHON_NET_OK=3/3` + `TEST PASSED`（395 条断言）。
