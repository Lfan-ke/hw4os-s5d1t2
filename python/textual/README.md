# textual — Python TUI 框架 (#764 python "textual")

**Textual 8.2.7**(基于 Rich 的现代 Python TUI 框架)在 StarryOS 四架构(x86_64/aarch64/riscv64/loongarch64)单核 qemu-10 上运行:headless 测试驱动(`App.run_test()` Pilot)运行,组合真实 widget 树、解析 CSS、运行 compositor + reactive 引擎 + 异步消息泵,**显示 + 控制(交互)全部通过**,4/4。

## DoD 结论(qemu-10 单核 starry,2026-06-06)

| arch | 结果 |
|------|------|
| x86_64 | √ `TEXTUAL_RESULT pass=9 total=9` |
| aarch64 | √ 9/9(`-cpu cortex-a72`) |
| riscv64 | √ 9/9(`-cpu rv64`) |
| loongarch64 | √ 9/9(`-machine virt -cpu la464`) |

判据:`TEXTUAL_RESULT pass=N total=N` 且 N≥9 → shell gate 打 `TEXTUAL_OK=1`(success_regex 锚定)。四架构 starry 均为真实运行(非仅安装包而不运行)。

## 测试内容(9 断言,headless `App.run_test()` Pilot)

1. import textual 8.2.7 + import rich
2. **Static 内容**渲染 round-trip(`STARRY_TEXTUAL`)
3. **Button** label
4. **CSS** 应用(compositor 解析样式表 → `#msg` color)
5. screen 组合 ≥4 widgets(widget 树)
6. **reactive + 控制**:`pilot.click("#btn")` → `Button.Pressed` → reactive count 自增
7. **Label reactive 更新** round-trip(count: 1)
8. **compositor** 帧(显示管线)

→ 覆盖 **显示**(widget 树/CSS/compositor 渲染)+ **控制**(pilot 模拟点击 → 事件 → reactive 更新)。

## 纯 Python 说明

textual + rich + 8 个依赖全是 `py3-none-any`(noarch)wheel → **架构无关**,四架构覆盖由 base musl CPython 3.12 保证。`prep-textual-rootfs.sh` 把 wheels 装进**独立 `/opt/pytui`**(不污染 base site-packages,避免版本叠加冲突),测例 `PYTHONPATH=/opt/pytui` 优先。这是"直接安装预编 wheel、跳过包管理器以直接测试应用"的设计;包管理器(uv/pip)本身为独立课题。

## 复现(qemu-10 四架构 starry)

```bash
# 1) wheels 已附 wheels/(或 pip download --only-binary=:all: textual)
# 2) 组装 rootfs(debugfs 直写, 无 mount/sync, /opt/pytui 隔离)
for a in x86_64 aarch64 riscv64 loongarch64; do bash prep-textual-rootfs.sh $a; done
# 3) case/ 放进 tgoskits/test-suit/starryos/stress/textual-0/, 跑:
source <仓库根>/.starry-env.sh
for a in x86_64 aarch64 riscv64 loongarch64; do
  cargo xtask starry test qemu --arch $a -g stress -c textual-0
done
```

## 文件
- `case/build-<arch>.toml` / `case/textual-0/qemu-<arch>.toml` — 四架构 build/run
- `prep-textual-rootfs.sh` — debugfs rootfs 组装(/opt/pytui)
- `textual_smoke.py` — headless Pilot 显示+控制 smoke
- `wheels/` — textual + 9 依赖 noarch wheel
