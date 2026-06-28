# jdk-multi/case/ — openjdk-multi-0 用例文件

`openjdk-multi-0` 用例的 harness + 构建脚本 + host 参考输出。这些文件（连同 `../programs/`）已在上游 tgoskits 的 `test-suit/starryos/stress/openjdk-multi-0/`；此处保留备查/复现。

```
case/
├── qemu-{x86_64,aarch64,riscv64,loongarch64}.toml   4 架构 harness（统一 shell_init_cmd，per-arch machine/mem）
├── prep-jdk-multi-rootfs.sh                          构建 rootfs-<arch>-jdk-multi.img（debugfs -w，装 4 JDK + 切换状态）
└── host-ref/                                          host 期望输出 + host 仿真
    ├── Jdk{17,21,23,25}Features.out                  各版本程序的 host 期望输出（末尾 JDKxx_OK）
    ├── Jdk25Features.compact.out                     compact-object-headers 二跑期望输出
    ├── harness-host-sim.sh / .out                    整个 toml gate 的 host 仿真脚本 + 输出
    └── toml-body-host.out                            toml shell_init_cmd body 在 host 上得到 JDK_MULTI_OK=1 的证明
```

用法见 `../README.md`（构建 + 4 架构跑 + 手动验证单版本）。host_ref 的 `*.out` 是手动跑时的对照基准。
