# node-web — Node.js web-framework carpet

Industrial-grade, on-target test of a set of real Node.js web frameworks, run by **Node.js
v22.22.2** (full V8 JIT, no kernel change) on StarryOS across all four architectures
(x86_64 / aarch64 / riscv64 / loongarch64).

Each module is a self-contained carpet that exercises one framework's public API surface
(hundreds of exact-value assertions). Every assertion compares against an exact golden value
computed from the framework's documented behaviour on this exact node + package version. A
module prints an anchored `*_DONE` marker only when its internal fail count is zero;
`run-nweb.sh` runs every module and emits `TEST PASSED` only when all of them pass (no skip).

## Run

```
cargo xtask starry app qemu -t node-web --arch x86_64
cargo xtask starry app qemu -t node-web --arch aarch64
cargo xtask starry app qemu -t node-web --arch riscv64
cargo xtask starry app qemu -t node-web --arch loongarch64
```

`prebuild.sh` stages the musl-native Node.js v22.22.2 apk closure (the Alpine v3.22
`nodejs-22.22.2-r0` closure: node + icu + libstdc++/libgcc + openssl + c-ares + nghttp2 +
simd* + zlib/zstd/brotli + musl) into the per-app rootfs `/usr`, plus the pug/express
`node_modules` closure and the Kotlin/JS module into `/root/nweb` (image grown to 2 GiB). A
developer who already has the apk closure locally can point `NODE_DL_ROOT` at that cache.

## Coverage

| module | framework | dimension | marker |
|:--|:--|:--|:--|
| pug | pug 3.0.3 | template engine: full API (render/renderFile/compile/compileFile/compileClient) + tags / attributes / interpolation / escaping / conditionals / iteration / mixins / inheritance / includes / filters / doctype | `PUG_DONE` |
| express | express 4.21.2 | web framework over real IPv4 loopback: routing / params / query / Router / middleware / body parsers / error handling / static / full response & request API | `EXPRESS_DONE` |
| kotlin-js | Kotlin/JS (Kotlin 2.0.21 IR → commonjs) | a host-precompiled Kotlin→JS module run on node; stdout byte-identical to the golden, per-line + per-token assertions | `KOTLINJS_DONE` |

The carpet sources live in `programs/carpets/`; the pug/express libraries are provided by the
arch-independent `node_modules` closure in `assets/`, and the Kotlin/JS module
(`assets/kotlin-app.js`) is emitted on the host by the Kotlin 2.0.21 IR backend.
