# htop — 交互式进程查看器 TUI(monitor;四架构 测+交付)

**htop 3.4.1-r1**(C / ncursesw)在 StarryOS 四架构(x86_64/aarch64/riscv64/loongarch64)单核 qemu-10 上:真 curses TUI **显示正确 + 键盘控制响应正确**。htop 无 headless/批处理模式,故用 **pyte(参考 VT102 模拟器)离线渲染串口流**验证。

## DoD 验证(qemu-10 单核 starry,2026-06-06)

| arch | 显示 | 控制 |
|------|------|------|
| x86_64 | √ CPU 条/Mem/Swp/Tasks/进程列/F键栏 | √ F2 Setup 弹出、F6 SortBy 菜单、F5 Tree |
| aarch64 | √ CPU 条/Mem 512M/Tasks/进程列/F键栏 | √ F5/F2 |
| riscv64 | √ CPU 100%/Mem 510M/进程列/F键栏 | √ |
| loongarch64 | √ CPU 条/Mem 8.00G/进程列/F键栏 | √ |

pyte 渲染显示 htop 完整界面:CPU meter 条、Mem/Swp、`Tasks: N`、`PID USER PRI NI VIRT RES SHR S CPU% MEM% TIME+ Command` 进程表、`F1Help F2Setup ... F10Quit` 功能键栏;F2 进入 Setup(Categories/Display/Header/Meters/Screens/Colors),F6 排序菜单 —— **操作后无涂抹、持续正确**。

注:`Load average: nan` —— starry `/proc/loadavg` 暂缺(待补,不影响 htop 其余功能)。aarch64 Mem 上限 512M(aarch64 RAM 上限为已知限制)。

依赖 TUI 相关内核修:`/proc/<pid>/status` 补 ctxt_switches、FIOCLEX/FIONCLEX ioctl、ioctl NotATty 探测降噪(否则全屏 TUI 在串口被内核 warn 污染)。

## 复现

```bash
# 1) htop apk(见 SOURCES.md / apks/)
# 2) rootfs(复用 glances rootfs base,含 ncursesw+terminfo;debugfs 注入 htop)
for a in x86_64 aarch64 riscv64 loongarch64; do bash prep-htop-rootfs.sh $a; done
# 3) 交互跑(直起 qemu,boot 到 starry shell 后):
#    export TERM=linux; htop      → 看实时进程 TUI;F2 设置 / F5 树 / q 退出
# 4) 无人值守验显示(pyte):喂命令+按键给 qemu serial,抓 stdout,pyte-render-frames.py 渲染最终屏。
```

## 文件
- `apks/htop-<arch>.apk` — 四架构 htop 3.4.1-r1
- `prep-htop-rootfs.sh` — debugfs 注入(复用 glances rootfs base)
- `pyte-render-frames.py` — pyte 多帧渲染器(验显示)
