# node-lib — Node.js library carpet

Industrial-grade, on-target test of a set of common Node.js libraries, run by **Node.js
v22.22.2** (full V8 JIT, no kernel change) on StarryOS across all four architectures
(x86_64 / aarch64 / riscv64 / loongarch64).

Each module is a self-contained carpet that exercises one library's public API surface
(hundreds of exact-value assertions; CSS preprocessors and JS transforms are compared against
exact golden output computed from the library's documented behaviour on this exact version). A
module prints an anchored `*_DONE` marker only when its internal fail count is zero;
`run-nlib.sh` runs every module and emits `TEST PASSED` only when all of them pass (no skip).

## Run

```
cargo xtask starry app qemu -t node-lib --arch x86_64
cargo xtask starry app qemu -t node-lib --arch aarch64
cargo xtask starry app qemu -t node-lib --arch riscv64
cargo xtask starry app qemu -t node-lib --arch loongarch64
```

`prebuild.sh` stages the musl-native Node.js v22.22.2 apk closure into the per-app rootfs
`/usr` and the library `node_modules` closure + carpet sources into `/root/nlib` (image grown
to 2 GiB). A developer who already has the apk closure locally can point `NODE_DL_ROOT` at it.

## Coverage

| module | library | dimension | marker |
|:--|:--|:--|:--|
| less | less 4.2.2 | render API + variables / arithmetic / nesting / mixins / guards / extend / maps / @import / functions / plugins → byte-exact CSS | `LESS_DONE` |
| stylus | stylus 0.64.0 | render + JS `define`/`set` API / mixins / conditionals / iteration / hashes / built-ins / @extend → byte-exact CSS | `STYLUS_DONE` |
| scss | sass 1.83.4 (Dart Sass) | compileString/compile + @mixin/@function / control flow / `sass:*` modules / @use / @extend / maps → exact CSS | `SCSS_DONE` |
| babel | @babel/core 7.26.0 | transformSync + preset-typescript (TS strip) + preset-react (JSX) + custom plugins / parse / AST round-trip | `BABEL_DONE` |
| terser | terser 5.37.0 | minify + mangle / compress / format / sourceMap / nameCache options → exact minified output | `TERSER_DONE` |
| eslint | eslint 9.18.0 | `Linter`/`ESLint` flat-config lint: exact diagnostics (ruleId / messageId / line / col / severity) + `verifyAndFix` | `ESLINT_DONE` |
| cjsesm | Node ESM/CJS | CommonJS↔ESM interop: require / dynamic import / data: URL / createRequire / circular resolution → exact values | `CJSESM_DONE` |

The carpet sources live in `programs/carpets/`; the libraries are provided by the
arch-independent `node_modules` closure in `assets/`.
