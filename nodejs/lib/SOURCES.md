# nodejs/lib — 来源与构建说明

Node.js 常用类库地毯测试。上游载体为 `apps/starry/node-lib`
（PR [rcore-os/tgoskits#1440](https://github.com/rcore-os/tgoskits/pull/1440)）。本目录是 CI-like
可构建交付：提交 carpet 源码 + 构建脚本 + 运行配置，依赖按下表获取后由 `build-nlib-deps.sh`
重建，不随仓库 bundle 大体积 `node_modules` / node 二进制。

## 被测类库及其来源（`package.json` 锁定版本，npm registry）

| 类库 | 版本 |
|:--|:--|
| less | 4.2.2 |
| stylus | 0.64.0 |
| sass（Dart Sass） | 1.83.4 |
| @babel/core | 7.26.0 |
| @babel/preset-typescript | 7.26.0 |
| @babel/preset-react | 7.26.3 |
| terser | 5.37.0 |
| eslint | 9.18.0 |

Node.js 运行时（各架构 musl）v22.22.2：Alpine v3.22 `nodejs-22.22.2-r0` apk 闭包（20 apk/arch），
由 `apps-starry/prebuild.sh` 从官方源获取，开发者本地有缓存可设 `NODE_DL_ROOT` 指向 `nodejs-apks/<arch>/`。

## 构建

```
bash build-nlib-deps.sh
```

产出 `out/`（`assets/node_modules` 约 145 包 / 36 MiB + `programs/carpets/*.js` +
`programs/run-nlib.sh` + `prebuild.sh` + `*.toml` + `README.md`），即上游 `apps/starry/node-lib`
的目录布局；置于 tgoskits `apps/starry/node-lib/` 后运行。构建脚本会瘦身闭包（删 docs /
sourcemap / 类型定义 / test 目录，运行时不读）。

## 运行

```
cargo xtask starry app qemu -t node-lib --arch <x86_64|aarch64|riscv64|loongarch64>
```

四架构单核实测 `NODE_LIB_OK=7/7` + `TEST PASSED`（961 条断言）。
