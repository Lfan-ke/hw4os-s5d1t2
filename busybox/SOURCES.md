# busybox/ — 软件包来源 (provenance)

## busybox applets（版本核实自下载缓存的 openwrt-apks/SOURCES.md）
- **busybox 1.37.0-r30** + **busybox-binsh 1.37.0-r30** + **rootfs-<arch>-alpine.img**(Alpine v3.23.4 musl base,已含 busybox):**Alpine Linux v3.23** **main** musl-native apk,四架构(x86_64/aarch64/riscv64/loongarch64)。
  - CDN:`https://dl-cdn.alpinelinux.org/alpine/v3.23/main/<arch>/busybox-1.37.0-r30.apk`
  - 覆盖的 6 个 #764 子命令(resize/remove-shell/rdev/setlogcons/killall5/fdflush)是 busybox 内置 applet,随主包提供;对应 starry 内核依赖见 README「关联内核改」表(bb-* 分支)。

## 权威来源记录
busybox + alpine base 逐包 URL/版本/sha256 见 `<本机下载缓存目录>/openwrt-apks/SOURCES.md`(busybox 1.37.0-r30 条目)+ Alpine CDN v3.23/main。
