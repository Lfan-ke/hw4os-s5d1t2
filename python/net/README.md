# python/net — Python Web 框架地毯测试

在 StarryOS 上用 musl-native CPython 3.14 对一组 Python Web / ASGI / GraphQL 框架做工业级地毯
测试，四架构（x86_64 / aarch64 / riscv64 / loongarch64）单核 qemu-10 运行。`run_all.py` 跑全部
3 个模块，全部通过（PASS == TOTAL，无 skip）才输出 `TEST PASSED`，合计 **3 模块 / 395 条断言**。

上游载体：`apps/starry/python-net`，PR [rcore-os/tgoskits#1441](https://github.com/rcore-os/tgoskits/pull/1441)。

## 覆盖

| 模块 | 框架 | 维度 | marker | 断言 |
|:--|:--|:--|:--|--:|
| DjangoCarpet | Django 4.2.30+ | 经 `test.Client` 驱动内存 sqlite：路由/转换器/视图/响应/模板/ORM(CRUD·filter·aggregate·F·Q·FK)/表单/中间件/signing/cache | `DJANGO_DONE` | 157 |
| FastapiCarpet | FastAPI 0.121+ + uvicorn 0.38+ + Pydantic 2 | 路由/路径-query-body 参数类型转换/422 校验/依赖注入/中间件/OpenAPI + **真实 uvicorn ASGI server over IPv4 回环** | `FASTAPI_DONE` | 132 |
| StrawberryCarpet | strawberry-graphql 0.316.0 | schema/类型/查询/mutation/enum/interface/union/自定义 scalar/异步 resolver/变量/内省/SDL | `STRAWBERRY_DONE` | 106 |

四架构单核 qemu-10 StarryOS 实测各 `PYTHON_NET_OK=3/3` + `TEST PASSED`（含 x86_64 本地 on-target 复核）。

## 构建与运行

`apps-starry/` 是上游 StarryOS app 配置（4 架构 `build-*.toml` / `qemu-*.toml` / `prebuild.sh` /
`README.md`），carpet 源在 `carpets/`，strawberry-graphql + cross-web 的 py3-none-any wheel 在
`assets/`（来源见 `SOURCES.md`）。`prebuild.sh` 经 qemu-user-static 把 CPython 3.14 与各框架 apk
装进 base-rootfs staging（原生 pydantic-core 按 target 架构解析），解压 assets 下的 wheel 进
site-packages，再注入 per-app overlay。运行：

```
cargo xtask starry app qemu -t python-net --arch <x86_64|aarch64|riscv64|loongarch64>
```
