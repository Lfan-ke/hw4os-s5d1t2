# glances — 系统监控 (headless/TUI/C-S/web) (monitor)

**glances**(Python 系统监控，psutil 后端）在 StarryOS 四架构单核 qemu-10 上四架构全部通过。

## DoD 验证结论

| arch | 结果 |
|------|------|
| x86_64 | √ `GLANCES_OK=1` |
| aarch64 | √ `GLANCES_OK=1`（`-cpu cortex-a72`）|
| riscv64 | √ `GLANCES_OK=1` |
| loongarch64 | √ `GLANCES_OK=1`（`-cpu la464 -machine virt`）|

测试内容：glances 启动 + 单次快照采集（CPU/mem/load/proc 经 psutil 读 /proc），断言关键字段输出。依赖 starry `/proc/{stat,meminfo,loadavg,<pid>/status,<pid>/statm}` 正确上报（含 Threads + statm 修复）。

## 构建运行

```bash
bash prep-glances-rootfs.sh <arch>          # 注入 glances + psutil 闭包
cargo xtask starry test qemu --arch <arch> -g stress -c glances-0
# 成功判据: ^GLANCES_OK=1
```
