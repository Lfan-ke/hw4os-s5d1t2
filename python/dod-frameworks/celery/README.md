# celery (+ flower) — Python 分布式任务队列 (#764)

Apache 风格分布式任务队列 **celery 5.5.3** + 监控面板 **flower 2.0.1**，在 StarryOS 四架构单核 qemu-10 上**全部通过**。

## DoD 结论

| arch | 结果 | 说明 |
|------|------|------|
| x86_64 | √ `CELERY_OK=1` | booted, 真任务执行, 0 panic |
| aarch64 | √ `CELERY_OK=1` | `-cpu cortex-a72`, 同上 |
| riscv64 | √ `CELERY_OK=1` | 纯 python 不受 TCG 重型 server 拖累 |
| loongarch64 | √ `CELERY_OK=1` | `-cpu la464 -machine virt`, `to_bin=true` |

**测试内容**(非 exit-0)：跨进程 celery worker 注册 `@app.task add/mul`，经 `memory://` broker + `cache+memory://` backend 执行 `add(2,3)` 与 `mul(4,5)`，断言结果 `==5` 且 `==20`（覆盖 celery 任务注册 / kombu 序列化 / result backend / worker 执行路径），并验证 flower 监控组件可加载。`memory://` broker 零外部进程、零网络，headless 确定性。

## 构建与运行

```bash
# 1) 构建 4 个 celery rootfs (纯 python wheel 闭包经 debugfs 注入, 无 mount/sync, WSL2 安全)
bash prep-celery-rootfs.sh <arch> <starryos_workspace_root>
#    arch ∈ x86_64|aarch64|riscv64|loongarch64
#    base = rootfs-<arch>-python.img (Alpine musl python 3.12 + pip)

# 2) 单核四架构跑测 (qemu-10)
cargo xtask starry test qemu --arch <arch> -g stress -c celery-0
#    成功判据: 串口出现 ^CELERY_OK=1 (success_regex)
```

## 闭包来源

- celery 5.5.3 + flower 2.0.1 + kombu/billiard/amqp/vine/click*/prometheus-client/prompt-toolkit/python-dateutil/six/tzdata/pytz/humanize/packaging/wcwidth/redis-client = 20 个 `py3-none-any` 纯 python wheel，4 架构通用。
- tornado 6.5.5(flower web）：x86/aarch64 用 musllinux 原生 wheel；riscv64/loongarch64 用 sdist 纯 python 树（无 C speedups，tornado 自动回退）。
- 详见 `prep-celery-rootfs.sh` 头注释 + SOURCES。
