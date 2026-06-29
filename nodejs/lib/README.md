# nodejs/lib — Node.js 常用类库地毯测试

在 StarryOS 上用 Node.js v22.22.2（全 V8 JIT，零内核改动）对一组常用 Node.js 类库做工业级
地毯测试，四架构（x86_64 / aarch64 / riscv64 / loongarch64）单核 qemu-10 运行。`run-nlib.sh`
跑全部 7 个模块，全部通过（PASS == TOTAL，无 skip）才输出 `TEST PASSED`，合计 **7 模块 /
961 条断言**。

上游载体：`apps/starry/node-lib`，PR [rcore-os/tgoskits#1440](https://github.com/rcore-os/tgoskits/pull/1440)。

## 覆盖

| 模块 | 类库 | 维度 | marker | 断言 |
|:--|:--|:--|:--|--:|
| LessCarpet | less 4.2.2 | render API + 变量 / 算术 / 嵌套 / mixin / guard / extend / map / @import / 内置函数 / plugin → 逐字节 CSS | `LESS_DONE` | 110 |
| StylusCarpet | stylus 0.64.0 | render + JS `define`/`set` API / mixin / 条件 / 迭代 / hash / 内置 / @extend → 逐字节 CSS | `STYLUS_DONE` | 117 |
| ScssCarpet | sass 1.83.4 | compileString/compile + @mixin/@function / 控制流 / `sass:*` 模块 / @use / @extend / map → 精确 CSS | `SCSS_DONE` | 98 |
| BabelCarpet | @babel/core 7.26.0 | transformSync + preset-typescript（TS 去类型）+ preset-react（JSX）+ 自定义 plugin / parse / AST 往返 | `BABEL_DONE` | 115 |
| TerserCarpet | terser 5.37.0 | minify + mangle / compress / format / sourceMap / nameCache 选项 → 精确压缩 | `TERSER_DONE` | 83 |
| EslintCarpet | eslint 9.18.0 | `Linter`/`ESLint` flat-config lint：精确诊断（ruleId/messageId/line/col/severity）+ `verifyAndFix` | `ESLINT_DONE` | 330 |
| CjsEsmCarpet | Node ESM/CJS | CommonJS↔ESM 互操作：require / 动态 import / `data:` URL / createRequire / 循环解析 | `CJSESM_DONE` | 108 |

四架构单核 qemu-10 StarryOS 实测各 `NODE_LIB_OK=7/7` + `TEST PASSED`（含 x86_64，经 app-QEMU/OVMF 本地 on-target 复核）。

## 构建与运行

依赖来源与构建命令见 `SOURCES.md`。`build-nlib-deps.sh` 用 npm 从 `package.json` 重建并瘦身
`node_modules` 闭包，并把 `carpets/` 的 carpet 源 + `apps-starry/` 配置组装成上游
`apps/starry/node-lib` 的目录布局（`out/`），置于 tgoskits `apps/starry/node-lib/` 后：

```
cargo xtask starry app qemu -t node-lib --arch <x86_64|aarch64|riscv64|loongarch64>
```

`prebuild.sh` 把 musl-native Node.js v22.22.2 apk 闭包注入 per-app rootfs `/usr`，把
`node_modules` 闭包 + carpet 源注入 `/root/nlib`，由 starry 上 node 全 V8 JIT 跑 carpet。
