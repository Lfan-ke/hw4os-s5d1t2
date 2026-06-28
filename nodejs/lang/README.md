# nodejs/lang — Node.js v22.22.2 language-level carpet (StarryOS)

Node.js **v22.22.2** (V8) language-level delivery for StarryOS. The node binary is a
musl-native Alpine closure that runs **full V8 JIT on all four architectures with
zero kernel changes** — Node is one of the few runtimes that needs no kernel
adaptation to run JIT (contrast: Java needs `-Xint`).

## Cases (under `cases/`)

| case | gate | covers |
|------|------|--------|
| `nodejs-0` | `NODEJS_ALL_OK` | runtime + `node --version`, fs/crypto (sha256/hmac/randomBytes), loopback HTTP, modern JS (BigInt / optional chaining / `Array.at` / private fields `#x` / `structuredClone` / TextEncoder-Decoder) + **worker_threads** (2nd V8 isolate) |
| `nodejs-lite-0` | `NODEJS_LITE_OK` | preprocessors (less / stylus / scss / pug via Node API, byte-identical) + **ESM/CJS interop** + **Kotlin-JS** (run host-emitted `app.js`) |
| `nodejs-tools-0` | `NODEJS_TOOLS_OK` | eslint (flat config) + `@babel/cli` transpile (TypeScript path, byte-identical) + terser minify (byte-identical) + express (loopback HTTP) |
| `nodejs-pm-0` | `NODEJS_PM_OK` | yarn offline install (gating); npm is a non-gating control (kernel-blocked, see below) |

Each case's test logic is embedded in its `qemu-<arch>.toml` `shell_init_cmd`; assertions
are byte-identical-to-host-golden or exact-token. Single-core (`features=["qemu"]`,
`-smp 1`). A V8 JIT probe falls back to `--jitless` only if JIT is unavailable (it is
available on all four arches).

## Verified (qemu-10 single-core, current kernel HEAD)

- **aarch64 / riscv64 / loongarch64**: `nodejs-0`, `nodejs-lite-0`, `nodejs-tools-0`
  all `*_OK=1` + `SUCCESS PATTERN MATCHED` (9/9); `nodejs-pm-0` (yarn) likewise.
- **x86_64**: via CI (local app-qemu `-kernel` PVH loader limit).

## Building the rootfs

The node runtime closure is bundled in `apks/<arch>/` (20 Alpine musl apks, see
SOURCES.md). The case-specific test `node_modules` (preprocessors / eslint / babel /
terser / express / yarn) are host-vendored and injected by the `prep-nodejs-*.sh`
scripts.

The prep scripts read a few env vars instead of any hardcoded machine paths:
`TGOSKITS_ROOT` (your tgoskits checkout; rootfs imgs land under `tmp/axbuild/rootfs/`),
`SUDO_PW` (host sudo password for the loop-mount steps), `NODEJS_FW` (the host-vendored
test material dir for the lite/tools/pm cases), and `NODE_HOME` (a host Node v22.22.2
install, only for the pm case's bundled npm). `NODEJS_APKDIR` overrides the default
`apks/<arch>/` apk location. Then:

```sh
bash prep-nodejs-rootfs.sh <arch>        # rootfs-<arch>-nodejs.img  (nodejs-0)
bash prep-nodejs-lite-rootfs.sh <arch>   # rootfs-<arch>-nodejs-lite.img
bash prep-nodejs-tools-rootfs.sh <arch>  # rootfs-<arch>-nodejs-tools.img
bash prep-nodejs-pm-rootfs.sh <arch>     # rootfs-<arch>-nodejs-pm.img
```

Then `cargo xtask starry test qemu --arch <arch> -g stress -c <case>`.

## Scope note (npm / astro / heavy frameworks)

The Node **language + core ecosystem** above is delivered and 4-arch verified. The
**heavy-V8-build group — npm, astro, and full multi-framework builds (vite/vue/react
at scale)** — is **kernel-gated**: it stresses V8 heavy-load mmap / page-table pressure
and stack-on-demand growth (deep libuv/threadpool behavior). These are pending kernel
features, not language-level issues; `react+vite` is verified via an x86 framework case.
The integral nodejs group (with npm/astro) is delivered together once the kernel work
lands.
