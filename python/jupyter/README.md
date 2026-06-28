# JupyterLab — StarryOS 四架构单核交付 (#764 jupyter)

**JupyterLab 4.5.7**(jupyter-server + ipykernel + pyzmq/libzmq + 257-asset 前端 bundle)在 StarryOS 四架构(x86_64/aarch64/riscv64/loongarch64)单核 qemu-10 上验证:**真实 `jupyter lab` headless 服务器正确启动 + URL 可达 + REST API 应答**,4/4 全部通过。

## DoD 结论(qemu-10 单核 starry,2026-06-06)

| arch | 结果 | 备注 |
|------|------|------|
| x86_64 | √ `JUPYTER_OK=1` (up/lab/api/status=1) | 官方 amd64;default cpu |
| aarch64 | √ `JUPYTER_OK=1` | `-cpu cortex-a72`(必须,否则 virt 默认 AArch32→内核不启动) |
| riscv64 | √ `JUPYTER_OK=1` | `-cpu rv64`;前端从 py3-none-any wheel 装(无 riscv apk) |
| loongarch64 | √ `JUPYTER_OK=1` | `-machine virt -cpu la464`;前端同 wheel 路径 |

判据权威:xtask `rc=0` + 日志 `SUCCESS PATTERN MATCHED` + `1/1 case(s) passed` + 锚定 `^JUPYTER_OK=1`,四架构各复核一致。

## 测试内容(JupyterLab PRODUCT smoke)

门控 `JUPYTER_OK = UP && LAB_OK && API_OK && STATUS_OK`(非 exit-0,真打 HTTP 响应):
1. **import sanity**:`jupyterlab / jupyter_server / zmq(pyzmq native + libzmq) / ipykernel / tornado` 全 import,打印各版本(jupyterlab 4.5.7)。
2. **launch**:`jupyter lab --allow-root --no-browser --ip=127.0.0.1 --port=8888`(无 token/无 xsrf)后台起,轮询 `/api` 直到应答(UP)。
3. **`/lab`**(LAB_OK):取前端 HTML,断言 HTTP 200 + 含 `JupyterLab` + `jupyter-config-data`(= 257-asset 前端真正 served,非 "assets not found" 错误页)。
4. **`/api`**(API_OK):REST 根返回含 `version` 的 JSON。
5. **`/api/status`**(STATUS_OK):server status JSON 含 `last_activity` + `kernels`。

验证内核面:Python3.14 运行时、**pyzmq native(libzmq 在 starry 的 epoll/eventfd/AF_UNIX 之上)**、tornado AF_INET loopback listen/accept/recv(`/api` HTTP 路径)。

## 关键适配点(本交付坐实的内核/打包修)

1. **qemu cpu/machine**:aa 必须 `-cpu cortex-a72`、loong 必须 `-machine virt -cpu la464`(否则 64 位内核根本不 boot = 无串口 = 假超时)。见上表。
2. **前端资源路径**(rv/loong wheel 路径专属):`jupyterlab` apk 仅 x86/aarch64 有;rv/loong 用 py3-none-any wheel。pip `--target` 把 wheel 的 `.data/data/share/jupyter/lab`(257 静态资源)落进 site-packages/share,而 JupyterLab 在 `/usr/share/jupyter/lab` 找前端 → 否则 `/lab` 返回 329B "assets not found" 错误页。prep 脚本已把 share/bin 迁回 `/usr/share`、`/usr/bin`(x86/aa apk 路径本就正确)。
3. **httpx 闭包**:jupyterlab pypi 扩展依赖 `httpx`(连同 `h11`/`httpcore`),已加入 apk-closure 根,四架构 rootfs 自带。

## 来源(provenance)

- **JupyterLab 4.5.7 stack**:Alpine edge musl apks(py3.14,全预编译,无在 guest 编译)= py3-pyzmq(cython `_zmq.so`)+ libzmq + libsodium + ipykernel/ipython/jupyter_client/jupyter_core/jupyter-server + nbconvert/nbformat/nbclient/tornado/traitlets/jinja2 + httpx/h11/httpcore 等。
- **前端**(全 4 arch 统一):`jupyterlab==4.5.7` + `jupyterlab-server>=2.28.0` + `jupyter-lsp>=2.0.0` 的 **py3-none-any** PyPI wheel(arch 无关,bundle 257-asset 前端),避免 riscv/loong 无 jupyterlab apk 的缺口。
- 详见 `../core/apks/SOURCES-python-apks.md §9` + `prep-jupyter-rootfs.sh`。

## 构建运行

```bash
bash prep-jupyter-rootfs.sh <arch>          # 解析+下载闭包, debugfs 注入 rootfs-<arch>-jupyter.img(4G)
source <仓库根>/.starry-env.sh              # 第一原则: qemu-10
cargo xtask starry test qemu --arch <arch> -g stress -c jupyter-lab-0
# 成功判据: rc=0 + SUCCESS PATTERN MATCHED + ^JUPYTER_OK=1
```

注:`timeout = 2400/4000`(x86 / aa·rv·loong;TCG 下 jupyter-server 启动 + 257-asset 前端 + pyzmq 较重,非真 hang)。
