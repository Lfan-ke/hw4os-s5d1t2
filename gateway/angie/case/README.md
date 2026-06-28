# angie-0 — StarryOS gateway stress case (#764)

`angie` is an **nginx-compatible fork** (C web server / reverse proxy, maintained by
webserver-llc; nginx config syntax + extensions). This case exercises StarryOS by running
a real musl-native angie binary as a foreground HTTP server and probing it over the
in-guest loopback interface. It mirrors the sibling `gateway-nginx-0` case exactly,
swapping `nginx` -> `angie`.

Tracking issue: rcore-os/tgoskits#764 (gateway sub-task). Binary sources, SHA256, and the
dependency closure are documented in `<download-cache>/gateway-bins/SOURCES.md`.

## What the case does (methodology)

Each `angie-0/qemu-<arch>.toml` boots StarryOS with the angie rootfs image
(`rootfs-<arch>-angie.img`) and runs a `shell_init_cmd` that:

1. `angie -v` — assert the running binary is exactly **`Angie/1.11.5`** (`VER_OK`); a wrong
   version fails the case.
2. `angie -t -c /etc/angie/angie.conf` — config syntax check (surfaces the first missing
   syscall early).
3. Launch angie in the **foreground / single-process** mode (`daemon off; master_process
   off; error_log stderr;`) bound to `127.0.0.1:8080`. The minimal `/etc/angie/angie.conf`
   serves a fixed in-config body via `location / { return 200 "ANGIE_OK_BODY"; }` — no
   static-file open, no sendfile path.
4. Issue a real loopback HTTP request with busybox `wget http://127.0.0.1:8080/` (retry up
   to 40x) and assert the body is exactly `ANGIE_OK_BODY` (`SERVE_OK`).
5. Gate: emit `ANGIE_OK=1` **only** when `VER_OK=1 && SERVE_OK=1`. The success token is
   produced by a single trailing `printf` so no echo/comment/wrap can produce a false
   positive. On failure the angie stderr is dumped (look for socket/bind/epoll/eventfd/
   sendfile errors).

QEMU pass criteria:
- `success_regex = ["(?m)^ANGIE_OK=1"]`
- `fail_regex = ['(?i)\bpanic(?:ked)?\b']`
- `timeout = 1800`

QEMU knobs per the case spec: `-smp 1` (single CPU) on every arch; aarch64 uses
`-cpu cortex-a72`; loongarch64 uses `-machine virt -cpu la464` + `to_bin=true`; drive is
`rootfs-<arch>-angie.img` via virtio-blk-pci; a virtio-net-pci/user netdev is attached
(loopback works in-guest without it, kept for parity with nginx).

Kernel stress surface (same class as nginx): `socket/bind/listen/accept4`, `epoll`,
`sendfile`, `fork` (suppressed here via single-process), `setuid` (suppressed — runs as
root, no `user` line).

## Files

| file | purpose |
|------|---------|
| `build-x86_64-unknown-none.toml` | StarryOS build config, x86_64 |
| `build-aarch64-unknown-none-softfloat.toml` | StarryOS build config, aarch64 |
| `build-riscv64gc-unknown-none-elf.toml` | StarryOS build config, riscv64 |
| `build-loongarch64-unknown-none-softfloat.toml` | StarryOS build config, loongarch64 |
| `angie-0/qemu-x86_64.toml` | QEMU run + DoD probe, x86_64 |
| `angie-0/qemu-aarch64.toml` | QEMU run + DoD probe, aarch64 |
| `angie-0/qemu-riscv64.toml` | QEMU run + DoD probe, riscv64 |
| `angie-0/qemu-loongarch64.toml` | QEMU run + DoD probe, loongarch64 |
| `prep-angie-rootfs.sh` | builds `rootfs-<arch>-angie.img` from the Alpine base image |

## Per-arch binary availability

angie's **official Alpine (musl) apk repo** (`download.angie.software/angie/alpine/v3.23/main`)
ships **only x86_64 and aarch64** (riscv64 / loongarch64 APKINDEX are 404). The latter two
are therefore **SOURCE-CROSS-BUILT** from the angie 1.11.5 tarball with the musl cross
toolchain (full configure line + SHA256 in `<download-cache>/gateway-bins/SOURCES.md` §4).

| arch | angie 1.11.5 binary | source | rootfs buildable now? |
|------|--------------------|--------|------------------------|
| x86_64 | YES (`gateway-bins/angie/x86_64/`, 7 apks) | official Alpine apk | YES |
| aarch64 | YES (`gateway-bins/angie/aarch64/`, 7 apks) | official Alpine apk | YES |
| riscv64 | YES (`gateway-bins/angie/riscv64/payload/`) | **source-cross-built** (musl-gcc 11.2.1) | YES |
| loongarch64 | YES (`gateway-bins/angie/loongarch64/payload/`) | **source-cross-built** (musl-gcc 13.2.0) | YES |

The source build is a **minimal module set**: http core + rewrite (`return`) + headers
(`add_header`), with gzip / proxy / fastcgi / uwsgi / scgi / grpc / memcached / ssi / mail /
http-cache dropped. It links **only `libpcre2-8.so.0` (10.47)** + musl libc (no openssl/zlib
— those modules are not built). The interpreter is set to the Alpine musl loader
(`/lib/ld-musl-<arch>.so.1`) and the `libc` NEEDED soname patched to `libc.musl-<arch>.so.1`,
so it loads against the byte-identical Alpine v3.23 base image. Consequently the foreground
`angie.conf` for these two arches omits the `proxy/fastcgi/uwsgi/scgi` `*_temp_path`
directives (they would be unknown directives on the minimal build); the served
`location / { return 200 "ANGIE_OK_BODY"; }` is identical to the apk arches.
`prep-angie-rootfs.sh` extracts the `payload/` tree (instead of apks) for these arches and
writes the matching minimal config. Under qemu-user, `angie -v` -> `Angie/1.11.5` and
`angie -t` -> "test is successful".

## Building the rootfs

```bash
# from the angie assets dir; set TGOSKITS_ROOT to your tgoskits checkout first
bash prep-angie-rootfs.sh x86_64        # -> tmp/axbuild/rootfs/rootfs-x86_64-angie.img       (apk)
bash prep-angie-rootfs.sh aarch64       # -> tmp/axbuild/rootfs/rootfs-aarch64-angie.img      (apk)
bash prep-angie-rootfs.sh riscv64       # -> tmp/axbuild/rootfs/rootfs-riscv64-angie.img      (source payload)
bash prep-angie-rootfs.sh loongarch64   # -> tmp/axbuild/rootfs/rootfs-loongarch64-angie.img  (source payload)
```

The script starts from the Alpine v3.23 musl base image (`rootfs-<arch>-alpine.img`, same
branch as the angie apks/deps, so the musl/openssl/pcre/zlib ABI matches byte-for-byte). For
**x86_64/aarch64** it extracts the angie apk + its dependency closure (pcre2 / zlib / libssl3
/ libcrypto3 / openssl). For **riscv64/loongarch64** it copies the source-built `payload/`
tree (`usr/sbin/angie[-nodebug]` + `libpcre2-8.so.0`; musl libc already in the base) and
writes the minimal-module config (no proxy/fastcgi/uwsgi/scgi temp paths). It then writes the
foreground `/etc/angie/angie.conf` and injects everything via `debugfs -w` into the
**unmounted** ext4 image (no mount, no `sync` — avoids the WSL2 D-state deadlock). It
recreates the `/usr/sbin/angie -> angie-nodebug` symlink and verifies the binary, libs, and
conf landed.
