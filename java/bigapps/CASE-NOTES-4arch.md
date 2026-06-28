# bigapps 单核四架构结论 (2026-05-30)

承接 [#764](https://github.com/rcore-os/tgoskits/issues/764) 「linux app 引导 starry」。本目录两个 Apache 大数据 jar 应用在 StarryOS 上**单核 4 架构全部通过**。

## 结论

| 应用 | jar | x86_64 | aarch64 | riscv64 | loongarch64 | DoD |
|---|---|---|---|---|---|---|
| **iceberg** | iceberg-spark-runtime-3.5_2.12-1.11.0.jar | √ | √ | √ | √ | JVM class-load + Iceberg Schema/PartitionSpec 构建 |
| **paimon** | paimon-flink-1.20-1.4.1.jar | √ | √ | √ | √ | JVM class-load + Paimon Schema/RowType 构建 |

- 全部 `*_OK=1`(printf 防假阳性门),`result: 1/1 case(s) passed`。
- 四架构覆盖由 **musl OpenJDK 运行时**保证(jar 字节码架构无关);JVM 一律 `-Xint` + 显式 `-Xms/-Xmx`(见上级 `java/README.md` 运行约定);loongarch64 另加 `-XX:-UsePerfData -XX:+ReduceSignalUsage`。

## 运行硬性要求:QEMU 10

**loongarch64 必须用 QEMU ≥ 10**。qemu-8 的 `qemu-system-loongarch64 -kernel` 会拒绝 StarryOS 镜像并报
`could not load kernel '…/starryos.bin': The image is not ELF`;qemu-10 正常加载。x86_64/aarch64/riscv64 用 qemu-8 也能跑,但统一用 qemu-10。维护者复跑前请确保 `qemu-system-loongarch64 --version` 为 10.x。

## 文件
每应用:`qemu-<arch>.toml` ×4 + `build-<target>.toml` ×4 + `prep-<app>-rootfs.sh`(debugfs 注入 jar 到 java rootfs 镜像)+ jar 包(来源见 `SOURCES.md`)。维护者把这些落到 tgoskits `test-suit/starryos/stress/<app>-0/`。
