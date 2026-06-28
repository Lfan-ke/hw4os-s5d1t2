# monitor/ — 软件包来源 (provenance)

## glances（版本核实自 monitor-apks/SOURCES.md,2026-05-24 核对 Alpine APKINDEX）
- **glances 4.4.1-r1**(v3.23/**community**)+ **py3-psutil 7.1.3-r0**(v3.23/**main**,native `_psutil_linux.abi3.so` 4 arch musl)+ CPython 3.12.13 闭包:**Alpine Linux v3.23** musl-native apk,四架构(x86_64/aarch64/riscv64/loongarch64)。
  - CDN:`https://dl-cdn.alpinelinux.org/alpine/v3.23/community/<arch>/`(glances)、`.../main/<arch>/`(python3/psutil)
  - 零源码编译;经 `prep-glances-rootfs.sh` 注入 `rootfs-<arch>-python.img` base。
  - 权威逐包 URL/版本/sha256:`<本机下载缓存目录>/monitor-apks/SOURCES.md`。
- 依赖内核修:procfs `/proc/[pid]/status` Threads + `statm`(见仓库内核修说明)。

> 注:若后续加入 prometheus/grafana 等监控包,prometheus 3.11.3 来自 GitHub release(amd64/arm64/riscv64 官方 + loong64 自编),grafana 13.0.1 来自官方 tarball(loong64 自编后端+官方前端)——大二进制不入 git,见各 prep 脚本头注释 + `<本机下载缓存目录>/monitor-bins/`。
