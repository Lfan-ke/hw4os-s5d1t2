# gateway/ — 软件包来源 (provenance)

## nginx（版本核实自 gateway-apks 的 SOURCES.md）
- **nginx 1.28.3-r2**(v3.23/**main**,4 arch 版本号逐字相同,无任一架构缺)+ pcre2/zlib/openssl 闭包:**Alpine Linux v3.23** musl-native apk,四架构(x86_64/aarch64/riscv64/loongarch64)。
  - CDN:`https://dl-cdn.alpinelinux.org/alpine/v3.23/main/<arch>/`
  - 零源码编译;经 `prep-gateway-rootfs.sh` 注入 base rootfs。
  - 权威逐包 URL/版本/sha256:`<本机下载缓存目录>/gateway-apks/SOURCES.md`。

## angie（nginx 兼容 fork，gateway/angie，4/4 交付）
- **angie 1.11.5**，C 语言 nginx 兼容 HTTP 服务器/反向代理。StarryOS 四架构单核 qemu-10 全绿（详见 `angie/README.md`）。
  - **x86_64 / aarch64**：官方 Alpine musl apk `angie-1.11.5-r0`，来自 `https://download.angie.software/angie/alpine/v3.23/main/<arch>/`；依赖 pcre2 10.47 / zlib 1.3.2 / openssl 3.5.6（Alpine v3.23 main）。
  - **riscv64 / loongarch64**：angie 官方 Alpine 仓库**只发 x86_64+aarch64**（rv/loong APKINDEX 404）→ 从源码 `https://download.angie.software/files/angie-1.11.5.tar.gz` 用 musl 交叉工具链交叉编译：
    - riscv64：musl-gcc 11.2.1，SHA256 `14597807ee474d84984473e89c863800a8f4f0a8d16dd602364e0393a44ac5e6`
    - loongarch64：musl-gcc 13.2.0，SHA256 `1df8885a4a83530c5ff117659f46e3d4083ace2cb4b871bd6dce2845b9da5d3b`
  - 完整 provenance（configure 行、patchelf、闭包）：`<本机下载缓存目录>/gateway-bins/SOURCES.md` §4。
