# openwrt/ — 软件包来源 (provenance)

## dropbear（版本核实自下载缓存的 openwrt-apks/SOURCES.md）
- **dropbear 2025.88-r1**(v3.23/**main**,PIE musl ELF `/usr/sbin/dropbear` + `/usr/bin/dropbearkey`)+ 闭包:**Alpine Linux v3.23** musl-native apk,四架构(x86_64/aarch64/riscv64/loongarch64)。
  - CDN:`https://dl-cdn.alpinelinux.org/alpine/v3.23/main/<arch>/`
  - 零源码编译;`prep-openwrt-rootfs.sh` 产出 `rootfs-<arch>-openwrt.img`(dropbear-0 与 dnsmasq-0 共用)。

## 权威来源记录
逐包 URL / 版本 / sha256 / 获取方式见 `<本机下载缓存目录>/openwrt-apks/SOURCES.md`(下载侧权威记录,同含 dnsmasq)。
