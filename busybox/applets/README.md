# busybox applets — 子命令兼容性套件 (#764 busybox)

busybox applet 兼容性测试套件，在 StarryOS 四架构单核（`normal/qemu-smp1` 组）qemu-10 上跑，覆盖 ~100 个 applet。

## #764 勾选子命令（本套件覆盖）

`resize` · `remove-shell` · `rdev` · `setlogcons` · `killall5` · `fdflush` 六个 #764 busybox section 子命令，均在本套件内验证。其余覆盖：ash/awk/bzip2/chgrp/chmod/chroot/cmp/cp/dd/delgroup/deluser/diff/… 等核心 applet。

## DoD

- 测试脚本：`case/sh/busybox-tests.sh`（1044 行，每 applet 一条 PASS/FAIL 断言）。
- 成功判据：`^PASS: \d+  FAIL: 0`（任一 FAIL 即整体失败，无静默放过）。
- rootfs：`rootfs-<arch>-alpine.img`（Alpine musl + busybox）。
- 单核 `qemu-smp1`，4 架构 qemu toml 在 `case/`。

### 四架构结果（真 4/4）

| arch | 结果 | 备注 |
|:--:|:--:|:--:|
| x86_64 | √ `PASS: 313  FAIL: 0` | |
| aarch64 | √ `PASS: 313  FAIL: 0` | `-cpu cortex-a72` |
| riscv64 | √ `PASS: 313  FAIL: 0` | |
| loongarch64 | √ `PASS: 313  FAIL: 0` | TCG 下 313 条 applet 测试耗时长，`timeout` 需 ≥ 3000s（本仓库 loong toml 已设 3000）；非功能性失败 |

四架构均 `SUCCESS PATTERN MATCHED` + `Test run completed`，无 panic。

## 关联内核改（fork 分支，PR 至 dev）

| 子命令 | starry 内核依赖 |
|--------|------------------|
| rdev | pseudofs 暴露根块设备为 `/dev/vda`（bb-rdev）|
| killall5 | job-control SIGSTOP/SIGCONT + `kill(-1)` 广播（bb-killall5）|
| resize | TIOCGWINSZ/TIOCSWINSZ 终端尺寸 ioctl（bb-resize）|
| setlogcons | TIOCLINUX 控制台重定向（bb-setlogcons）|
| fdflush/remove-shell | applet 路径 + `/etc/shells` 行删除（bb-fdflush/bb-remove-shell）|

详见 `busybox-compatibility-report.zh.md`。

## 运行

```bash
# normal/qemu-smp1 组单核四架构
cargo xtask starry test qemu --arch <arch> -g normal -c busybox   # 调用法以仓库 xtask 为准
# 成功判据: ^PASS: \d+  FAIL: 0
```
