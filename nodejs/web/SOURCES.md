# nodejs/web — 来源与构建说明

Node.js Web 框架地毯测试。上游载体为 `apps/starry/node-web`
（PR [rcore-os/tgoskits#1439](https://github.com/rcore-os/tgoskits/pull/1439)）。本目录是 CI-like
可构建交付：提交 carpet 源码 + 构建脚本 + 运行配置 + 预编译 Kotlin/JS 产物，依赖按下表获取后由
`build-nweb-deps.sh` 重建，不随仓库 bundle 大体积 `node_modules` / node 二进制。

## 被测框架及其来源

| 框架 | 版本 | 来源 |
|:--|:--|:--|
| express | 4.21.2 | npm registry `express@4.21.2`，由 `build-nweb-deps.sh` 据 `package.json` 安装 |
| pug | 3.0.3 | npm registry `pug@3.0.3`，同上（`build-nweb-deps.sh` 产出 `out/assets/node_modules` 闭包，约 103 包 / 14 MiB） |
| Kotlin/JS 模块 | Kotlin 2.0.21 IR → commonjs | host 预编译产物 `carpets/kotlin-app.js`（+ golden `carpets/kotlin-REF.out`）随仓库提交；源自 Kotlin 2.0.21 IR backend（`-module-kind commonjs`），重建需 JDK17 + kotlinc-js |
| Node.js 运行时（各架构 musl） | v22.22.2 | Alpine v3.22 `nodejs-22.22.2-r0` apk 闭包（node + icu + libstdc++/libgcc + openssl + c-ares + nghttp2 + simd\* + zlib/zstd/brotli + musl，20 apk/arch）；由 `apps-starry/prebuild.sh` 从官方源获取，开发者本地有缓存可设 `NODE_DL_ROOT` 指向 `nodejs-apks/<arch>/` |

## 构建

```
bash build-nweb-deps.sh
```

产出 `out/`（`assets/node_modules` + `assets/kotlin-app.js` + `programs/carpets/*.js` +
`programs/run-nweb.sh` + `prebuild.sh` + `*.toml` + `README.md`），即上游 `apps/starry/node-web`
的目录布局；置于 tgoskits `apps/starry/node-web/` 后运行。`KotlinJsCarpet` 经 `__dirname` 解析
与其同目录的 `kotlin-app.js`，故 `prebuild.sh` 把 Kotlin/JS 模块 stage 到 carpet 同目录。

## 运行

按 `apps-starry/README.md`：

```
cargo xtask starry app qemu -t node-web --arch <x86_64|aarch64|riscv64|loongarch64>
```

四架构单核实测 `NODE_WEB_OK=3/3` + `TEST PASSED`（291 条断言）。
