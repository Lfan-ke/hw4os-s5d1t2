# casca — Python TUI 框架 (#764 python "casca")

**casca 1.0.4**("Native Python CLI UI library with CSS-like styling",纯 Python TUI 框架)在 StarryOS 四架构(x86_64/aarch64/riscv64/loongarch64)单核 qemu-10 上运行:headless 运行,构建真实 widget 树、驱动 Redux 式 Store(reducer+dispatch)状态机、经 `App.handle_input` 处理键事件,**显示 + 控制(交互/状态)全部通过**,4/4。

## DoD 结论(qemu-10 单核 starry,2026-06-06)

| arch | 结果 |
|------|------|
| x86_64 | √ `CASCA_RESULT pass=9 total=9` |
| aarch64 | √ 9/9(`-cpu cortex-a72`) |
| riscv64 | √ 9/9(`-cpu rv64`) |
| loongarch64 | √ 9/9(`-machine virt -cpu la464`) |

判据:`CASCA_RESULT pass=N total=N` 且 N≥9 → shell gate 打 `CASCA_OK=1`(success_regex 锚定)。四架构 starry 均为真实运行(非仅安装包而不运行)。

## 测试内容(9 断言,headless)

1. import casca 1.0.4
2. **Label 内容** round-trip(`STARRY_CASCA`)+ `set_text`
3. **Button** text
4. **Container** 持 2 children(widget 树)
5. **App(build_ui)** 实例化
6. **Store reducer + dispatch** 状态转移(count 0→2,reactive/状态核心)
7. **handle_input(KeyEvent)** 输入/控制路径处理
8. **Keys** 表(控制键常量)

→ 覆盖 **显示**(Label/Button 内容、Container 树)+ **控制/状态**(Store dispatch 状态机、handle_input 键事件)。

## 纯 Python 说明

casca 是单个 `py3-none-any`(noarch)wheel、无额外依赖 → **架构无关**,四架构覆盖由 base musl CPython 3.12 保证。`prep-casca-rootfs.sh` 把 wheel 装进**独立 `/opt/pytui`**(不污染 base site-packages,避免版本叠加冲突),测例 `PYTHONPATH=/opt/pytui` 优先。这是"直接安装预编 wheel、跳过包管理器以直接测试应用"的设计;包管理器(uv/pip)本身为独立课题。

## 复现(qemu-10 四架构 starry)

```bash
# 1) wheels 已附 wheels/(或 pip download --only-binary=:all: casca)
# 2) 组装 rootfs(debugfs 直写, 无 mount/sync, /opt/pytui 隔离)
for a in x86_64 aarch64 riscv64 loongarch64; do bash prep-casca-rootfs.sh $a; done
# 3) case/ 放进 tgoskits/test-suit/starryos/stress/casca-0/, 跑:
source <仓库根>/.starry-env.sh
for a in x86_64 aarch64 riscv64 loongarch64; do
  cargo xtask starry test qemu --arch $a -g stress -c casca-0
done
```

## 文件
- `case/build-<arch>.toml` / `case/casca-0/qemu-<arch>.toml` — 四架构 build/run
- `prep-casca-rootfs.sh` — debugfs rootfs 组装(/opt/pytui)
- `casca_smoke.py` — headless 显示+控制 smoke
- `wheels/` — casca 单 noarch wheel(无额外依赖)
