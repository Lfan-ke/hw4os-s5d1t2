# nodejs/web — Node.js Web 框架地毯测试

在 StarryOS 上用 Node.js v22.22.2（全 V8 JIT，零内核改动）对一组 Web 相关框架做工业级地毯
测试，四架构（x86_64 / aarch64 / riscv64 / loongarch64）单核 qemu-10 运行。每个模块依框架公
开 API 逐项铺满（精确值断言）；`run-nweb.sh` 跑全部 3 个模块，全部通过（PASS == TOTAL，无
skip）才输出 `TEST PASSED`，合计 **3 模块 / 291 条断言**。

上游载体：`apps/starry/node-web`，PR [rcore-os/tgoskits#1439](https://github.com/rcore-os/tgoskits/pull/1439)。

## 覆盖

| 模块 | 框架 | 维度 | marker | 断言 |
|:--|:--|:--|:--|--:|
| PugCarpet | pug 3.0.3 | 模板引擎全 API（render/renderFile/compile/compileFile/compileClient）+ 标签 / 属性 / 插值 / 转义 / 条件 / 迭代 / mixin / 继承 / include / 过滤器 / doctype，逐条精确 HTML golden | `PUG_DONE` | 120 |
| ExpressCarpet | express 4.21.2 | Web 框架经真实 IPv4 回环：路由 / 参数 / query / Router / 中间件 / body 解析 / 错误处理 / static / 完整 response 与 request API | `EXPRESS_DONE` | 102 |
| KotlinJsCarpet | Kotlin/JS（Kotlin 2.0.21 IR → commonjs） | host 预编译的 Kotlin→JS 模块在 starry-node 运行，stdout 逐字节一致 + 逐行 / 逐 token 断言 | `KOTLINJS_DONE` | 69 |

四架构单核 qemu-10 StarryOS 实测各 `NODE_WEB_OK=3/3` + `TEST PASSED`（含 x86_64，经 app-QEMU/OVMF 本地 on-target 复核）。

## 构建与运行

依赖来源与构建命令见 `SOURCES.md`。`build-nweb-deps.sh` 用 npm 从 `package.json`（pug 3.0.3 +
express 4.21.2）重建 `node_modules` 闭包，并把 `carpets/` 的 carpet 源 + 预编译的 Kotlin/JS
模块 + `apps-starry/` 配置组装成上游 `apps/starry/node-web` 的目录布局（`out/`）。`apps-starry/`
是 StarryOS app 的 4 架构 `build-*.toml` / `qemu-*.toml` / `prebuild.sh` / `run-nweb.sh` /
`README.md`，运行命令见 `apps-starry/README.md`：

```
cargo xtask starry app qemu -t node-web --arch <x86_64|aarch64|riscv64|loongarch64>
```

`prebuild.sh` 把 musl-native Node.js v22.22.2 apk 闭包注入 per-app rootfs `/usr`，把
`node_modules` 闭包 + Kotlin/JS 模块注入 `/root/nweb`，由 starry 上 node 全 V8 JIT 跑 carpet。
