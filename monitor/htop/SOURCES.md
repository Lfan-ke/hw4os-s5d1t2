# htop 来源 / provenance

- htop **3.4.1-r1**,Alpine v3.23 **main**(`dl-cdn.alpinelinux.org/alpine/v3.23/main/<arch>/htop-3.4.1-r1.apk`)。
- 四架构 apk 各 ~130KB(apks/htop-<arch>.apk)。纯 C / ncursesw,依赖 musl + libncursesw + terminfo —— 均已在 base 镜像(rootfs-<arch>-glances.img,Alpine python 基)。
- 取材:`wget $M/main/<arch>/htop-3.4.1-r1.apk`(M=Alpine v3.23 镜像)。
