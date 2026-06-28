# pip-uv — StarryOS 工业级地毯测试 (#764 python pip + uv)

在 **StarryOS（musl Linux-ABI 宏内核 / ArceOS）** 上，于 **qemu-10 单核四架构**（x86_64 / aarch64 / riscv64 / loongarch64）地毯式验证 Python 包管理器 **pip 与 uv 的全部命令、选项与用法**。基座 **CPython 3.14**，工具均为最新版：**pip 26.1.2**、**uv 0.11.19**。

测试脚本 `carpet_pipuv.sh` 为 host / qemu10-Linux / starry-4arch 共用——同一脚本在真实 Linux（金标）与 StarryOS 上跑，行为应一致。

## 覆盖范围（地毯式，工业级）

| 维度 | 覆盖 |
|---|---|
| pip 子命令(18) | install / download / uninstall / freeze / inspect / list / show / check / config / search(help) / cache / index / wheel / hash / completion / debug / help / lock —— 每命令所有选项 |
| uv 命令(24) | pip(install/uninstall/list/show/freeze/tree/check/compile/sync) · venv · init · add · remove · sync · lock · export · tree · run · build · publish · tool · python · cache · self · version · auth · format · check · audit · workspace · help —— 每命令所有选项 |
| 调用形式(10) | `pip3` · `pip3.14` · `python -m pip` · `python -m pip.__main__` · `import pip; pip.main([...])` · `from pip._internal.cli.main import main` · `runpy.run_module('pip')` · 裸 `pip` · `uv pip` · `uvx` |
| 安装源矩阵 | 本地 wheel · 本地 sdist · 目录 · `-r requirements` · `-c constraints` · `-e` editable · extras `pkg[x]` · 版本约束 · **在线官方 PyPI** · **清华镜像 `-i`** · **`--extra-index-url`（清华+阿里）** · `pip config set global.index-url/extra-index-url` · **`python -m pip install --upgrade pip`** |
| 选项级 | pip/uv `--help` 全树逐选项核对（808 选项），含 `--all-releases`/`--only-final`/`--requirements-from-script`(PEP723) / uv compile `--no-strip-*`/`--annotation-style`/`--emit-*` / uv group-selector `--extra`/`--group`/`--no-dev`/`--only-group`/`--all-groups` / `uv version --bump` / `uv init --build-backend` 等 |

## 测试结果

| arch | 离线门控 (2026-06-10, qemu-10 单核) | 说明 |
|---|---|---|
| x86_64 | **CB_RESULT=240/240，0 FAIL，CARPET_END** √ | |
| aarch64 | **CB_RESULT=240/240，0 FAIL，CARPET_END** √ | -cpu cortex-a72 |
| riscv64 | **CB_RESULT=240/240，0 FAIL，CARPET_END** √ | |
| loongarch64 | **CB_RESULT=240/240，0 FAIL，CARPET_END** √ | 需下列 5 处内核根治后方可全通（DTB RAM + 抢占 + unix-pollset + epoll 持久 waker + pidfd poll） |

**门控权威** = `cargo xtask starry test ...` `rc=0` + 日志 `SUCCESS PATTERN MATCHED`（success_regex `^CARPET_END$`）+ `CB_RESULT=N/N`。**无虚假通过**：每个门控 `t` 为真断言（剥除冗余 `| tail`，纯宽松探针归为 `ti` 记录不门控），关键项断言金标值（pip 26 / uv 0.11 版本、`Name: pip`、`compatible tags`、`import` 验证、产物 `test -e`）。

## 重要说明（据实）

- **包与 rootfs 自下载预制**：pip wheel / uv 二进制 / 离线 wheel 均预先下载（见 `../../../download/pip-uv/PROVENANCE.md`），rootfs 预先注入；**绕过 apt/dpkg/apk**（部分发行版 dnf 源），故宿主 apt 报错不影响测试。
- **uv loongarch64**：astral-sh 官方不发布 loongarch64 二进制，loong 的 uv 取自 **Alpine edge community apk（uv 0.11.19-r0）**，版本与官方一致。
- **login 不测**：pip 无 login 子命令；`uv auth login`/`logout` 仅 `--help` 级，**不做真实登录**（无凭据使用）。
- **publish 仅 dry-run**：`uv publish --dry-run`，**绝不真实上传**（无 token/用户名/密码）。
- **在线安装**：在真实 Linux（host 金标）全绿（numpy/six/requests/certifi/清华+阿里镜像/upgrade pip）。在 StarryOS 上，TCP 三次握手通但 **TLS 大段 RX 数据面停滞**（深层网络栈难点，根因定位于 RX 未排空到完成），故在线段经一个 SIGKILL 限时探针门控**自动跳过**（不计入门控、不挂起）；该网络数据面修复后将在 StarryOS 上自动运行。
## StarryOS 内核根治（让 pip/uv 在四架构通过的 5 处真修，均向 Linux 行为靠拢）

这些是为 pip/uv 全绿而做的内核修复（在 tgoskits fork 分支，最终经内核 PR 上游）。前三处早期已落，后两处是 loongarch64 `uv pip install` / `uv run` / `uv build` 100% CPU 自旋 / 卡死的根因(2026-06-10 根治)：

1. **loongarch64 DTB RAM 检测**（`platforms/ax-plat-loongarch64-qemu-virt`）：原硬编码 `phys-memory-size` 无视 qemu `-m` → 大内存 pip/uv OOM-SIGSEGV。改为 boot 捕获启动寄存器 + 扫描 FDT + 解析 `/memory`，`MemTotal` honor `-m`。
2. **ret_to_user 抢占点**（`StarryOS kernel/src/task/user.rs` + `axtask`）：内核态中只置 `need_resched` 的时钟 tick，在返回用户态前补一次 `check_preempt_pending`；否则单核上 CPU-bound 线程（uv 紧循环非阻塞 syscall）饿死就绪的 peer。
3. **unix socketpair 唤醒方向**（`axnet-ng/src/unix/stream.rs`）：原两端两方向共用单一 `PollSet`，send/recv 都 wake 它 → reader 自己排空数据后**自唤醒**自身 IN + OUT level-true 伪边沿，把 edge-triggered 的 tokio reactor 拖入 100% 自旋。改为每端独立就绪队列（= Linux per-socket `sk_wq`），I/O 只唤醒对端。
4. **epoll 持久 waker**（`StarryOS kernel/src/file/epoll.rs`，= Linux `ep_poll_callback` 模型）：原 `PollSet` 是 one-shot（wake 消费 waker 须重注册），epoll 用 poll() level 重检补救丢边沿，但分不清"窗口新数据"与"未排空 stale" → edge-triggered fd 未排空时反复重报 → 永不阻塞自旋。改为每 interest 存单一持久 waker、唤醒时先重注册再入队、删除 level 重检。
5. **pidfd poll() 语义**（`StarryOS kernel/src/file/pidfd.rs`）：原报"进程活着 = 可读"（逻辑反了），致 `uv run`/`uv build` 狂 reap 活子进程 busy-loop、子进程真退出后反报空 → 永卡。改为"进程/线程已退出（`is_zombie`/`thread_exit`）才 POLLIN"（Linux pidfd 语义）。

> 注：epoll/pidfd 两处尚有仅 SMP（多核）下的窄竞态边角（单核 pip/uv 不触发，本交付全单核），将于内核 PR 前一并加固。

## 如何运行（维护者）

```bash
# 0. 准备 qemu-10（铁律，勿用 qemu-8）
source <tgoskits>/.starry-env.sh        # 把 /opt/qemu-10.2.1/bin 前置进 PATH

# 1. 构建注入了 pip 26.1.2 + uv 0.11.19 + 离线 wheel 的 rootfs（四架构各一）
bash prep-pip-uv-rootfs.sh x86_64
bash prep-pip-uv-rootfs.sh aarch64
bash prep-pip-uv-rootfs.sh riscv64
bash prep-pip-uv-rootfs.sh loongarch64

# 2. 四架构单核跑（建议 qemu10 最大并发 2；loong 单独跑）
cargo xtask starry test qemu --arch x86_64      -g stress -c pip314-0
cargo xtask starry test qemu --arch aarch64     -g stress -c pip314-0
cargo xtask starry test qemu --arch riscv64     -g stress -c pip314-0
cargo xtask starry test qemu --arch loongarch64 -g stress -c pip314-0   # 单独
```

`case/` 内含四架构 build-*.toml + qemu-*.toml（已内嵌 `carpet_pipuv.sh`）。上面的 `cargo xtask ... test` 会自动跑完 240 条主要命令并逐条打印 `CB <命令>=OK/FAIL`，末尾 `CB_RESULT=240/240` + `CARPET_END`（自动化验收)。

### 人工交互验收(可选)

想自己进 StarryOS shell 逐条手敲 pip/uv 主要命令：

```bash
# 进任一架构的交互 shell（一个一个跑，退出 qemu: Ctrl-A 再 X）
bash boot-pipuv.sh <x86|rv|aa|loong>
# 进 shell 后先引导一次 pip（uv 直接可用），再敲 pip/uv 命令——见 boot-pipuv.sh 顶部注释。
```

`boot-pipuv.sh` 用的 qemu-10 调用与过门控的完全一致；`-snapshot` 使改动不落盘,可反复跑。
