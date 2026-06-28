# nodejs/lang — provenance

## Node runtime closure (`apks/<arch>/`)

The Node.js v22.22.2 musl-native runtime + its 20-apk dependency closure, identical
version across all four architectures, from Alpine Linux:

```
https://dl-cdn.alpinelinux.org/alpine/edge/{main,community}/<arch>/<pkg>-<ver>.apk
```

Key member: `nodejs-22.22.2-r0.apk` (per arch). Closure also includes `icu-data-full`,
`libstdc++`, `simdjson`, `simdutf`, `c-ares`, `nghttp2`, `sqlite-libs`, `brotli`, etc.
(the full APKINDEX dependency chain for `node`). Each arch's closure is 0-missing and
version-identical (verified via APKINDEX). apk = gzip tar; extracted into the rootfs
under `/usr` (apk metadata members stripped).

## Test `node_modules`

The case-specific test packages are host-vendored (prepared with `npm`/`yarn` on the
host, captured verbatim) and injected by the `prep-nodejs-*.sh` scripts:

- `nodejs-lite-0`: less / stylus / sass(scss) / pug + a CJS↔ESM interop fixture + a
  Kotlin/JS-emitted `app.js`.
- `nodejs-tools-0`: eslint (flat config) + @babel/cli + @babel/preset-typescript +
  terser + express.
- `nodejs-pm-0`: yarn (offline install fixture).

These are the same package versions resolvable from the npm registry at delivery time;
the prep scripts document the exact set.

## Notes

- Node runs full V8 JIT on all four arches with no kernel changes / no workarounds.
- The heavy-V8-build group (npm, astro, full multi-framework builds) is kernel-gated
  (V8 heavy-load mmap/page-table pressure + stack-on-demand growth); delivered with the
  integral nodejs group once the kernel work lands.
