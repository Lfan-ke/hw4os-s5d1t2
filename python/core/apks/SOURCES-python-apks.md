# Python APK + wheel 下载来源清单（StarryOS app 适配）

适配目标：类 Alpine Linux OS (musl libc) × StarryOS × 4 个 CPU 架构（`x86_64` / `aarch64` / `riscv64` / `loongarch64`）。

跟踪 issue：rcore-os/tgoskits#764 — python 子课题（numpy / opencv / django / fastapi / langchain / uvicorn / strawberry / sklearn）。

> 本文件**只记录来源 + 计划 + 可行性结论**，不在此阶段下载大体积闭包（DoD loongarch QEMU 正在占用 rootfs 镜像）。实际抓取用 `prep-python-rootfs.sh`（待跑）。下面所有版本号均在 2026-05-23 核对自 Alpine 官方 APKINDEX + PyPI JSON API。

---

## 0. 结论速览（最重要）

| 维度 | 结论 |
|------|------|
| **python 3.14 是否可得** | √ **可得**：Alpine **edge** = `python3 3.14.3-r0`（4 arch 全有，2025-10 发布的 3.14 已进 edge）。稳定分支 **v3.22 = 3.12.13**、**v3.23 = 3.12.13**。 |
| **native 扩展（numpy/scipy/sklearn/opencv）是否需要源码编译** | × **完全不需要**。Alpine 为 **4 个架构全部提供 musl 原生预编译 apk**（`py3-numpy` / `py3-scipy` / `py3-scikit-learn` / `py3-opencv`），版本号 4 arch 一致。这比 java/nodejs 的 riscv 缺包局面好得多。 |
| **当前 base 镜像分支** | `rootfs-<arch>-alpine.img` 核对 = **Alpine v3.23.4**（`/etc/apk/repositories` 指向 `v3.23/main` + `v3.23/community`），无预装 python。 |
| **推荐路线** | **路线 A（默认，低风险）**：用 base 镜像自带的 **v3.23 = python 3.12.13** + v3.23 community 的 native apk + **uv 0.10.2 apk**。**路线 B（命中 #764「3.14」字面）**：edge = **python 3.14.3** + edge community native apk + **uv 0.11.16 apk**。两条都 4 arch 全闭包、零源码编译。包安装首选 **uv**（纯 python 框架 wheel：strawberry/langchain），重型 native（numpy/sklearn/opencv）走 apk（最干净）。 |
| **strawberry-graphql / langchain** | × Alpine **无 apk**（`strawberry` apk 是音乐播放器 daemon，不是 graphql；`py3-langchain` 不存在）。√ 但都是 **纯 python `py3-none-any` wheel**，pip 离线装即可。唯一注意点：langchain **1.x** 链 → `langchain-core 1.x` → `uuid-utils`（Rust，**无 riscv64/loongarch64 musllinux wheel**）。改用 **langchain 0.3.x** 即可规避（见 §5）。 |
| **langchain 真实链能否离线确定性运行** | √ **能**。`FakeListLLM`（langchain_core.language_models.fake）+ `PromptTemplate` + `RunnableSequence` 组成的链，**零网络、零 API key、输出确定性**，可与 host REF 逐字比对。真实 LLM 调用需要网络 + key，**诚实标注为不在 CI 范围**。 |
| **包管理器 = uv（推荐）+ pip** | √ **uv（Rust 单文件）4 arch 全有 Alpine apk**：v3.23 community `uv 0.10.2-r0`、edge community `uv 0.11.16-r0`，x86_64/aarch64/riscv64/**loongarch64** 全覆盖，deps 仅 `libbz2+musl+libgcc_s`（~25-28MB）。**upstream GitHub 静态二进制 / PyPI musllinux wheel 均缺 loongarch64**——故 **apk 是唯一覆盖 4 arch 的途径**。#764 注释 `python <!-- 3.14 - pyπ - uv - venv -->` 把 uv/venv 列为 python 工具角度。DoD 像 java 的 maven/gradle 一样**同时跑 uv + pip**（见 §9）。 |

---

## 1. Alpine 官方 CDN 基础 URL

```
https://dl-cdn.alpinelinux.org/alpine/<branch>/<repo>/<arch>/<filename>.apk
```

- branch：`v3.23`（base 镜像分支，python 3.12）/ `v3.22`（同 java/nodejs，python 3.12）/ `edge`（滚动，python 3.14）
- repo：`main`（python3 解释器 + 基础库）/ `community`（numpy/scipy/sklearn/opencv/django/fastapi/uvicorn/pydantic 等绝大多数包）
- arch：`x86_64` / `aarch64` / `riscv64` / `loongarch64`

APKINDEX（依赖闭包解析必备）：
```
https://dl-cdn.alpinelinux.org/alpine/<branch>/main/<arch>/APKINDEX.tar.gz
https://dl-cdn.alpinelinux.org/alpine/<branch>/community/<arch>/APKINDEX.tar.gz
```

国内镜像（同结构，注意 cernet 对 v3.23 用 302 跳转，curl 要加 `-L`）：USTC `https://mirrors.ustc.edu.cn/alpine/`、清华 `https://mirrors.tuna.tsinghua.edu.cn/alpine/`、阿里 `https://mirrors.aliyun.com/alpine/`。
历史快照（edge 历史版本固定）：`https://archive.alpinelinux.org/alpine/`。

---

## 2. Python 解释器版本矩阵（4 arch 核对，版本完全一致）

| 包 | v3.23（base 镜像） | v3.22 | edge | 备注 |
|----|--------------------|-------|------|------|
| `python3` | **3.12.13-r0** | 3.12.13-r0 | **3.14.3-r0** | edge 命中 #764「python 3.14」字面 |
| `py3-pip` | 25.1.1-r1 | 25.1.1-r0 | 26.1.1-r0 | |

> 4 个架构（x86_64/aarch64/riscv64/loongarch64）上 `python3` 版本号**逐字相同**——只是二进制不同。无任一架构缺 python。

URL（`<ARCH>` 替换为 4 个架构之一）：
```
# 路线 A（python 3.12, base 镜像同分支，默认）
https://dl-cdn.alpinelinux.org/alpine/v3.23/main/<ARCH>/python3-3.12.13-r0.apk
https://dl-cdn.alpinelinux.org/alpine/v3.23/main/<ARCH>/py3-pip-25.1.1-r1.apk
# 路线 B（python 3.14, edge）
https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/python3-3.14.3-r0.apk
https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/py3-pip-26.1.1-r0.apk
```

**路线 B 注意**：edge 与 v3.23 的 `musl` / `libstdc++` / `libgcc` 等基础库 ABI 不同（edge `musl 1.2.6-r2` vs v3.23 `musl 1.2.5`）。混装 edge python 到 v3.23 base 镜像时，**整条 native 闭包都必须取自 edge**（含 musl / libgfortran / openblas / libstdc++），否则 so 版本对不上。最干净做法：路线 B 用 edge APK 全闭包注入（musl 也覆盖），或干脆从 edge 重新做一个 base 镜像。**路线 A 与 base 镜像同分支，无此问题，故为默认。**

---

## 3. 各包 native-扩展 可得性矩阵（4 arch · 核对 · 这是本调查的核心）

> 「musl 预编译 apk」= Alpine 直接给出 musl 原生 `.so`，**无需源码编译**。所有下列包在 x86_64/aarch64/riscv64/loongarch64 上**版本号一致**（只二进制不同），即 4 arch 全覆盖。

| #764 子课题 | apk 名 | v3.23 版本 | edge 版本 | native 扩展 | 4 arch musl apk? | 源码编译? |
|-------------|--------|-----------|-----------|-------------|-------------------|-----------|
| **numpy** | `py3-numpy` | 2.3.5-r0 | 2.4.6-r0 | C + Fortran(openblas) | √ 全有 | × 不需要 |
| **sklearn** | `py3-scikit-learn` | 1.5.2-r0 | 1.5.2-r1 | C/C++/Cython + openblas | √ 全有 | × 不需要 |
| **opencv** | `py3-opencv` | 4.12.0-r3 | 4.12.0-r7 | C++ (libopencv_*) | √ 全有 | × 不需要 |
| (numpy 依赖) | `py3-scipy` | 1.16.3-r0 | 1.17.1-r1 | C/C++/Fortran | √ 全有 | × |
| (sklearn 依赖) | `py3-pandas` | 2.3.3-r0 | 3.0.3-r0 | C/Cython | √ 全有 | × |
| **django** | `py3-django` | 4.2.30-r0 | 5.2.14-r0 | 纯 python | √（noarch） | × |
| **fastapi** | `py3-fastapi` | 0.121.2-r1 | 0.136.1-r0 | 纯 python | √（noarch） | × |
| **uvicorn** | `uvicorn` 非 py3- 前缀 | 0.38.0-r0 | 0.44.0-r0 | 纯 python | √（noarch） | × |
| (fastapi/uvicorn 依赖) | `py3-pydantic` | 2.12.3-r0 | 2.12.5-r0 | 纯 python | √ | × |
| (pydantic 依赖) | `py3-pydantic-core` | 2.41.4-r0 | 2.41.5-r0 | **Rust** | √ 全有（Alpine 已编） | × |
| (strawberry 依赖) | `py3-graphql-core` | 3.2.6-r0 | 3.2.6-r1 | 纯 python | √ | × |
| **strawberry** | × 无 apk | — | — | 纯 python | → pip wheel（§5） | × |
| **langchain** | × 无 apk | — | — | 纯 python | → pip wheel（§5） | × |

**关键结论**：numpy / opencv / sklearn 三个「重型 native」子课题，**4 个架构全部有 Alpine musl 预编译 apk，零源码编译**。这是 python 子课题相对 java(riscv 缺 JDK)/nodejs 的最大优势。

**uvicorn 命名陷阱**：Alpine 把 ASGI 服务器打成 `uvicorn`（**不是** `py3-uvicorn`）。同名的 `py3-uvicorn` 不存在。`uvicorn` apk `provides: cmd:uvicorn`、`py3.14:uvicorn`，依赖 `py3-click py3-h11 python3~3.14`（edge）。**别把它和 community 里那个 `strawberry`(1.2.19, Qt6 音乐播放器 daemon) 搞混**——后者完全无关。

uvicorn 性能增强可选包（均有 4-arch apk，含 Rust 的 watchfiles）：`py3-uvloop` `py3-httptools` `py3-websockets` `py3-watchfiles`。CI 测试用 asyncio 默认 loop 即可，不强制装。

---

## 4. native 重型包的 apk 依赖闭包（核对自 edge x86_64 APKINDEX，arch 间结构一致）

> 注入 rootfs 时按这些清单 `apk fetch -R` 或直接拉每个 apk 展开到 `/usr`。pkg 数 = 传递闭包大小。

### 4.1 `py3-numpy` 闭包（22 包，最轻）
```
musl python3 py3-numpy openblas libgfortran libquadmath libgcc libstdc++
libffi libbz2 libexpat gdbm libncursesw libpanelw ncurses-terminfo-base
mpdecimal readline sqlite-libs xz-libs zlib libcrypto3 libssl3
```
核心：`openblas`(BLAS/LAPACK) + `libgfortran`(Fortran 运行时) 是 numpy 数值计算地基。

### 4.2 `py3-scikit-learn` 闭包（81 包）
在 numpy 闭包基础上 + `py3-scipy py3-pandas py3-joblib py3-threadpoolctl libgomp`（OpenMP）+ 一串 `py3-dask py3-distributed py3-aiohttp ...`（scipy/pandas/sklearn 的间接 extras）。全部 noarch 或已有 musl apk。可裁剪：纯 fit/predict 测试不需要 dask/distributed，可只装 `py3-scikit-learn py3-scipy py3-numpy py3-joblib py3-threadpoolctl` 的 so 闭包。

### 4.3 `py3-opencv` 闭包（208 包，最重）
`py3-opencv` 元包默认拉 **完整 opencv**（含 `highgui` → Qt6 + mesa + ffmpeg + gstreamer + wayland + X11，约 200 包）。
**headless 图像处理只需** `libopencv_core libopencv_imgproc libopencv_imgcodecs`（+ 它们的 so 依赖：`libjpeg-turbo libpng tiff libwebp openexr-* zlib libstdc++ openblas`）+ `py3-numpy`。
**建议**：测试用 cv2 的 `imread/imwrite/cvtColor/resize/threshold/GaussianBlur` 等纯算法 op（不开窗口），可只注入 core/imgproc/imgcodecs 的精简闭包，避开 Qt6/mesa 那 ~180 个 GUI 包。`prep` 脚本里把 opencv 闭包做成「精简（headless）」与「完整」两档。

### 4.4 framework 集（uvicorn+fastapi+django+strawberry-graphql 的 apk 部分，38 包）
```
musl python3 uvicorn py3-fastapi py3-django py3-starlette py3-anyio py3-sniffio
py3-h11 py3-click py3-pydantic py3-pydantic-core py3-typing-extensions
py3-typing-inspection py3-annotated-types py3-annotated-doc py3-asgiref
py3-sqlparse py3-graphql-core py3-idna py3-curio tzdata + (libc/lib 闭包)
```
全部 musl apk 或 noarch，4 arch 齐。strawberry-graphql 本体走 §5 wheel。

---

## 5. strawberry-graphql / langchain：pip wheel 路线（无 apk）

二者 Alpine 都无 apk，但都是 **纯 python `py3-none-any` wheel**（跨 arch 通用，无需为 4 arch 各下一份）。
统一放 `python-apks/wheels/`（arch-无关，4 arch 共用同一批 .whl）。

### 5.1 strawberry-graphql（GraphQL）
- 最新：`strawberry-graphql 0.316.0`，wheel = `strawberry_graphql-0.316.0-py3-none-any.whl`（纯 python）。
- 运行期依赖（核心，全部 apk 或 noarch wheel 可得）：`graphql-core>=3.2,<3.4`（apk `py3-graphql-core`）、`typing-extensions`（apk）、`python-dateutil`（apk `py3-dateutil`）、`packaging`（apk `py3-packaging`）。
- ASGI 集成（可选）：starlette + uvicorn（均 apk 已有）。
- PyPI URL（JSON 取精确 hash）：`https://pypi.org/pypi/strawberry-graphql/0.316.0/json` → files 里取 `...-py3-none-any.whl` 的 `url`。
- pip 离线装：`pip install --no-index --find-links wheels/ strawberry-graphql`（graphql-core/dateutil 等已由 apk 满足，wheel 只需 strawberry-graphql 本体 + 它独有的纯 python 依赖）。

### 5.2 langchain（LLM 编排）
- **版本选择关键**：langchain **1.x** → `langchain-core 1.x` → 依赖 `uuid-utils`（**Rust/maturin**，musllinux wheel **仅 x86_64/aarch64/i686**，**riscv64/loongarch64 缺**）+ `langgraph`。直接阻断 2 个架构。
- √ **改用 langchain 0.3.x**：
  - `langchain 0.3.27`（`py3-none-any`）→ `langchain-core 0.3.79`（`py3-none-any`，**不依赖 uuid-utils、不依赖 langgraph**）+ `langchain-text-splitters`（纯 python）+ `langsmith`（纯 python）+ `SQLAlchemy`（apk `py3-sqlalchemy`）+ `requests`（apk `py3-requests`）+ `PyYAML`（apk `py3-yaml`）+ `pydantic`/`pydantic-core`（apk，pydantic-core 是 Rust 但 **Alpine 已为 4 arch 编好**）+ `tenacity`/`jsonpatch`（apk `py3-tenacity`/`py3-jsonpatch`）+ `async-timeout`（apk）。
  - 即：**langchain 0.3.x 的全部「编译」依赖（pydantic-core）都由 Alpine apk 满足，4 arch 齐；其余纯 python wheel 跨 arch 通用。** langsmith 可选拉 `orjson`（Rust）但有纯 python 回退，离线/无网下不触发。
- PyPI URL：`https://pypi.org/pypi/langchain/0.3.27/json`、`.../langchain-core/0.3.79/json`、`.../langchain-text-splitters/<ver>/json`、`.../langsmith/<ver>/json` → 各取 `py3-none-any.whl` 的 url+sha256。
- **确定性离线链**：`langchain_core.language_models.fake.FakeListLLM(responses=[...])` + `langchain_core.prompts.PromptTemplate` + `RunnableSequence`（`prompt | llm | StrOutputParser`）。**零网络、零 key、输出 = 预置 responses（确定性）**，可与 host REF 逐字比对。
- **诚实标注**：真实 LLM（各云端服务商或本地部署的模型）链需要网络 + API key/本地模型权重，**不纳入 CI**；CI 只测「langchain 运行时 + 链编排 + 提示模板 + 解析器」在 StarryOS 上能跑且确定性正确。这是对 langchain「在 StarryOS 上可用」最诚实、可复现的验证边界。

### 5.3 wheel 抓取（待跑，体积小，纯 python）
`prep` 脚本用 host python 的 pip 仅下 wheel（不装）：
```
pip download --only-binary=:all: --python-version 3.12 --platform any \
  --no-deps -d wheels/ \
  strawberry-graphql==0.316.0 langchain==0.3.27 langchain-core==0.3.79 \
  langchain-text-splitters langsmith
```
（`--no-deps`：其余依赖由 apk 满足，避免把 numpy 之类又抓成 wheel。装机时 `--no-index --find-links wheels/`。）

---

## 6. apk 结构与注入（与 nodejs/java 同套路）

每个 apk 是 gzip-tar，载荷展开到 `/usr`（`usr/bin/python3`、`usr/lib/python3.12/site-packages/numpy/...`、`usr/lib/*.so`）。
注入：`mount -o loop` 挂 `tmp/axbuild/rootfs/rootfs-<arch>-python.img`（从 alpine base 复制 + resize），对每个 apk `tar xz -C <mp>`（排除 `.PKGINFO`/`.SIGN.*`/`.trigger` 等元数据成员）。
musl loader 搜索路径写 `/etc/ld-musl-<arch>.path`（含 `/lib /usr/lib`）。
wheel（strawberry/langchain）：解开 .whl（就是 zip）到 `site-packages/`，或在 host 用 **uv**（首选）/`pip` 的 `... install --target` 预装好再 `cp -a` 进镜像（更稳，省得 guest 里跑装包）。
注入脚本三档：
- `prep-python-rootfs.sh`（python-0 运行时：python3+numpy+sklearn+opencv；默认路线 A，`PYBRANCH=edge` 切路线 B）。
- `prep-python-fw-rootfs.sh`（python-fw-0 框架：django/fastapi/uvicorn/strawberry/langchain，wheel 用 **pip** `--target` 注入）。
- `prep-python-uv-rootfs.sh`（**uv 版框架镜像**，推荐：装 uv apk 进 guest + host 用 uv `pip compile`/`pip install --offline --target` 注入 wheel；详见 §9）。

---

## 7. 备用镜像源

dl-cdn 限速/不可达时，所有 `https://dl-cdn.alpinelinux.org/alpine/` 可换：阿里 `https://mirrors.aliyun.com/alpine/`、USTC `https://mirrors.ustc.edu.cn/alpine/`、清华 `https://mirrors.tuna.tsinghua.edu.cn/alpine/`、华为 `https://mirrors.huaweicloud.com/alpine/`、cernet `https://mirrors.cernet.edu.cn/alpine/`（注意 302，curl 加 `-L`）。
PyPI 镜像：清华 `https://pypi.tuna.tsinghua.edu.cn/simple`、阿里 `https://mirrors.aliyun.com/pypi/simple`。

---

## 8. 风险 / 诚实标注

1. **python 3.14 vs 3.12 的取舍**：#764 字面写「python 3.14」→ edge 有 3.14.3。但 edge 与 base 镜像（v3.23）的 musl/ABI 不同，混装需整条 native 闭包都取 edge（含 musl）。**默认先用路线 A（v3.23/python 3.12）打通全部 8 个子课题**（base 镜像零 ABI 风险），再视需要做路线 B（edge/python 3.14）验证「3.14 同样可跑」。两条路线的 apk 全部 4-arch 齐、零源码编译。
2. **opencv GUI 闭包巨大（208 包）**：headless 图像 op 只需 core/imgproc/imgcodecs 精简闭包；prep 脚本分「精简/完整」两档，CI 默认精简（避 Qt6/mesa）。
3. **langchain 真实 LLM = 网络依赖**：CI 只能测离线确定性链（FakeListLLM）。真实推理不可复现、需 key，**明确排除在 CI 之外**——这是诚实边界，不是 StarryOS 缺陷。
4. **langchain 1.x 的 uuid-utils**：Rust，riscv64/loongarch64 无 musllinux wheel。**已通过锁 langchain 0.3.x 绕开**；切勿升 1.x，否则 riscv/loongarch 直接装不上（除非源码编译 uuid-utils，重）。
5. **JIT / native so 在 StarryOS 上的运行风险**：python 解释器本身无 JIT（CPython 3.12/3.14 默认不开 JIT），比 V8/JVM 风险低；但 numpy/opencv/sklearn 的 native `.so` 大量用 `mmap(PROT_EXEC)`/SIMD/线程/openblas，**首次实际运行可能暴露 StarryOS 的 mmap/线程/信号 真因 bug**——这正是「下游 app 引导内核改进」要找的目标。测试用例需对 native 调用做结果级校验（见测试用例设计），崩则定位内核真因（不 workaround）。
6. **uv 自身在 StarryOS 上跑 = Rust 重型二进制实跑**：uv 是 25-28MB 的 Rust 单文件，内部大量 `mmap`/线程/`statx`/文件锁/`io_uring?`（取决于版本），**guest 内 `uv pip install --offline` 可能暴露内核真因 bug**（与 nodejs/JVM 同类，但纯解包+解析，无 net）。若 uv 在某 arch 崩，DoD 仍可凭 **pip 路径**判 python 包管理「可用」，并把 uv 崩溃定位成内核 issue（不 workaround）。

---

## 9. uv（推荐包管理器）：来源 · 4-arch 可得性 · 离线注入设计

> 全部核对自 2026-05-23（Alpine community APKINDEX + astral-sh/uv GitHub release `0.11.16` + PyPI `uv` JSON）。脚本：`prep-python-uv-rootfs.sh`。

### 9.1 为什么是 uv，以及怎么拿到它（4-arch 可得性矩阵）

#764 python 行的注释 `python <!-- 3.14 - pyπ - uv - venv -->` 把 **uv** 列为推荐 python 工具（对标 java=maven/gradle、nodejs=npm/yarn）。uv 是 astral-sh 出的 **Rust 单文件**包管理器（极快，pip 兼容子命令 `uv pip ...`）。三条获取途径，**只有 Alpine apk 覆盖全部 4 arch**：

| 途径 | x86_64 | aarch64 | riscv64 | **loongarch64** | 结论 |
|------|--------|---------|---------|------------------|------|
| **Alpine apk `uv`（community）** | √ | √ | √ | √ | **唯一 4-arch 全覆盖 → 采用** |
| upstream GitHub 静态二进制（`uv-<triple>-unknown-linux-musl.tar.gz`） | √ | √ | √ (`riscv64gc`) | × **无 loong 资产** | 缺 loongarch |
| PyPI `pip install uv`（musllinux wheel） | √ | √ | √ | × **无 loong wheel** | 缺 loongarch |

apk 版本（4 arch 版本号逐字一致，仅二进制不同）：

| 分支 | uv 版本 | 安装大小 | 依赖闭包 |
|------|---------|----------|----------|
| **v3.23 community**（路线 A，base 镜像同分支） | `uv 0.10.2-r0` | ~25 MB | `so:libbz2.so.1` + `so:libc.musl-<arch>.so.1` + `so:libgcc_s.so.1`（**仅 3 个 so，近乎静态**） |
| **edge community**（路线 B，python 3.14） | `uv 0.11.16-r0` | ~28 MB | 同上 |

URL（`<ARCH>` ∈ {x86_64,aarch64,riscv64,loongarch64}，4 个均核对 HTTP-206 可达）：
```
# 路线 A（v3.23，uv 0.10.2，与 base 镜像同 ABI，默认）
https://dl-cdn.alpinelinux.org/alpine/v3.23/community/<ARCH>/uv-0.10.2-r0.apk
# 路线 B（edge，uv 0.11.16，配 python 3.14；整条 native 闭包须同取 edge，见 §2）
https://dl-cdn.alpinelinux.org/alpine/edge/community/<ARCH>/uv-0.11.16-r0.apk
```
PyPI 备查（venv/独立场景）：`https://pypi.org/pypi/uv/json`（musllinux wheel 仅 x86_64/aarch64/riscv64/i686/armv7，**无 loongarch64**）。

### 9.2 分工：哪些走 uv、哪些走 apk

| 类别 | 包 | 装法 | 理由 |
|------|----|----|------|
| 重型 native（musl 已编） | numpy / scipy / sklearn / opencv / pydantic-core / graphql-core | **apk**（`prep-python-rootfs.sh` + fw apk 闭包） | Alpine 4-arch 已编好 musl `.so`，apk 最干净、零编译、ABI 与 base 镜像一致；让 uv/pip 去编这些反而引入 riscv/loongarch 编译风险 |
| 框架 / 纯 python apk | django / fastapi / uvicorn / starlette / anyio / pydantic | **apk**（noarch） | Alpine 有 noarch apk，最稳 |
| **无 apk 的纯 python wheel** | **strawberry-graphql / langchain 0.3.x / langchain-core 0.3.x / langchain-text-splitters / langsmith** | **uv（推荐）/ pip** 离线 `--target` 注入 | Alpine 无 apk；都是 `py3-none-any`（跨 arch 通用，4 arch 共用同一批 .whl） |

即 **uv 负责「Alpine 没有 apk 的那几个纯 python 包」**，重型 native 仍交给 apk（与 SOURCES.md §3/§5 的既定结论一致，uv 不改变 native 分工，只替换 wheel 注入器）。

### 9.3 离线注入流程（host-resolve-then-bundle，guest 零网络）

`prep-python-uv-rootfs.sh <arch>` 在 `prep-python-rootfs.sh` 产出的 `rootfs-<arch>-python.img`（已含 numpy/sklearn/opencv）之上：

1. **apk 层**：`apk fetch -R` 拉 `uv` + 框架闭包（django/fastapi/uvicorn/pydantic/...）→ 解包进 `/usr`。**uv 二进制随之落到 guest `/usr/bin/uv`**（供 §9.4 的 in-guest DoD）。
2. **uv 锁 + vendor（host，arch 无关，一次产出 4 arch 共用）**：
   ```
   uv pip compile --generate-hashes --no-build req.in -o requirements-uv.txt   # 钉死+hash 的 lockfile
   pip download --only-binary=:all: --no-deps -d wheels/  <pure-python 包>      # vendor .whl（uv 无 download 子命令，用 pip 取原始 wheel）
   ```
3. **uv 离线装进 guest site-packages**：
   ```
   uv pip install --offline --no-index --no-deps --find-links wheels/ \
       --target <guest>/usr/lib/python<X>/site-packages  strawberry-graphql langchain ...
   ```
   `--offline` 断网、`--no-index`+`--find-links` 只用 vendored wheel、`--no-deps`（传递依赖已由 apk 满足）、`--target` 直接落进 guest 树。host 无 uv 时自动回退 `pip install --target`（语义一致）。
4. **in-guest 复跑物料**：把 `requirements-uv.txt` + `wheels/*.whl` 复制进 guest `/opt/uv/`，使 DoD 能在 StarryOS 内重跑 `uv pip install --offline`（证明 uv 在 OS 上能跑，而非仅 host 预铺）。

> **决策：host-resolve-then-bundle（不在 guest 联网装）。** 与 java/nodejs 一致——guest 内无网络、无 DNS，所有解析/下载在 host 完成，guest 只做离线安装/校验。uv 的 `--offline` + vendored `--find-links` 正是为此设计。

### 9.4 DoD：包管理器覆盖 = uv（推荐）+ pip（对标 java maven/gradle）

#764 给 python 打勾前，DoD 应像 java 同时验 maven+gradle 那样，**同时验 uv + pip 两条包管理路径**：

1. **uv 路径**（推荐）：guest 内 `uv --version` 正常；`uv pip install --offline --no-index --find-links /opt/uv/wheels -r /opt/uv/requirements-uv.txt --target /tmp/uvsite` 成功；`PYTHONPATH=/tmp/uvsite python3 -c "import strawberry, langchain; print('uv-ok')"` 输出确定值。
2. **pip 路径**（对照）：guest 内 `python3 -m pip install --no-index --no-deps --find-links /opt/uv/wheels --target /tmp/pipsite strawberry-graphql langchain` 成功；同样 import 校验。
3. （可选）**venv**：`python3 -m venv /tmp/venv && /tmp/venv/bin/python -c "import sys; print(sys.prefix)"`——命中注释里的 `venv`。

任一 arch 上 uv 崩 → 仍可凭 pip 路径判 python 包管理「可用」，并把 uv 崩溃**定位为内核真因 issue**（不 workaround，见 §8.6）。三路全绿才据实给 #764 python 勾打勾。

---

## 10. pyarrow（Apache Parquet I/O）addendum — `python-arrow-0` 用例（核对 2026-05-24）

> #764 python 行注释 `pyarrow <!-- parquet -->` → 验证 pyarrow 读写 Apache Parquet。脚本：`prep-python-arrow-rootfs.sh`。

### 10.1 4-arch 可得性（py3-pyarrow = Alpine v3.23 community，零源码编译）

| 包 | v3.23 版本 | x86_64 | aarch64 | riscv64 | loongarch64 | 备注 |
|----|-----------|--------|---------|---------|-------------|------|
| `py3-pyarrow` | **21.0.0-r4** | √ | √ | √ | √ | 4 arch 版本号逐字一致，仅二进制不同；musl-native，**无源码编译** |
| `libarrow` / `libarrow_acero` / `libarrow_compute` / `libarrow_dataset` / `libarrow_flight` | 21.0.0-r4 | √ | √ | √ | √ | Arrow C++ 核心库 |
| `libparquet` | 21.0.0-r4 | √ | √ | √ | √ | Parquet 读写（依赖 libthrift + libcrypto） |
| `apache-arrow` | 21.0.0-r4 | √ | √ | √ | √ | 数据文件元包 |

**无 musl-incompatible wheel 阻塞**：不走 PyPI wheel，直接用 Alpine 4-arch musl apk，与 numpy/sklearn 同套路。loongarch64 community APKINDEX 在 dl-cdn 偶发拉取失败（504/超时），用清华/USTC 镜像即可（apk-closure.py 已内置 fallback 镜像列表）。

### 10.2 依赖闭包（py3-cffi + py3-pyarrow + py3-numpy，x86_64 核对 132 apk，0 download fail，0 unsatisfied token）

py3-pyarrow 的 `D:` 硬依赖：`python3 py3-cffi py3-numpy so:libarrow.so.2100 so:libarrow_acero.so.2100 so:libarrow_compute.so.2100 so:libarrow_dataset.so.2100 so:libarrow_flight.so.2100 so:libparquet.so.2100 so:libstdc++.so.6 so:libgcc_s.so.1 so:libc.musl-<arch>.so.1`。

传递闭包把整条 Arrow C++ 运行时拉进来（全部 musl apk，4-arch 齐）：`libarrow*`/`libparquet` + `libprotobuf`(31) + `libthrift`(0.22) + `libgrpc`(51)/`grpc-cpp` + `glog`/`gflags` + `re2` + **abseil-cpp**(66 个 libabsl_*.so.2508.0.0) + `utf8proc` + 压缩编解码 `snappy`/`zstd-libs`/`lz4-libs`/`brotli-libs` + `boost1.84-filesystem` + `icu-libs`/`icu-data-en` + `c-ares` + `libcrypto3`/`libssl3` + base(`musl`/`libstdc++`/`libgcc`/`openblas`/`libgfortran`/`python3`/`py3-numpy`)。

**SONAME 注意**：abseil 各 `.so` 的 DT_SONAME 即完整 `libabsl_base.so.2508.0.0`（apk 不带 `.2508` 短链，无需补）；`libarrow_python{,_flight,_parquet_encryption}.so.2100` 由 py3-pyarrow **自带在 site-packages/pyarrow/ 内**，ext 模块用 `RUNPATH=$ORIGIN` 就近解析。全闭包 DT_NEEDED 核对对 image lib 集 0 缺失（3 个 `libarrow_python*` 经 $ORIGIN 满足）。

### 10.3 镜像 + 用例

- 镜像：`tmp/axbuild/rootfs/rootfs-<arch>-python-arrow.img` = `rootfs-<arch>-python.img`(已含 python3+numpy+libstdc+++openblas) 之上注入 pyarrow 闭包，resize 到 **4G**（Arrow C++ 库重）。WSL2 用 mount+tar+**直接 umount**（不 bare `sync`）。
- 用例：`test-suit/starryos/stress/python-arrow-0/`（build-*.toml 抄自 python-fw-0 + `python-arrow-0/qemu-<arch>.toml`）。DoD：构造 Arrow Table（int32/int64/float64/string 5 列 5 行）→ `pq.write_table` 写 `/tmp/t.parquet` → `pq.read_table` 读回 → 断言 schema + 全值 EXACT 相等（`table.equals` + 逐列 `to_pylist` 比对）→ `PYARROW_OK=1`；`echo PYARROW_DONE`。`success_regex=["(?m)^PYARROW_OK=1"]`、`fail_regex='(?i)\bpanic(?:ked)?\b'`。
- 运行（orchestrator）：`cargo xtask starry test qemu --arch x86_64 -g stress -c python-arrow-0`。

---

## 11. python 3.14 ROUTE B（PURE-interpreter feature case）— `python-314-0`（下载 + 决策 2026-05-24）

> #764 `python <!-- 3.14 ... -->` 要 **3.14**；python-0 跑的是 3.12，其 T5（3.14 特性）被 `compile()` 门控 → 3.12 走 `PY314_SKIP`，**3.14 新语法/新 stdlib 从未真跑**。本节落地 ROUTE B。

### 11.1 实情订正（重要）：3.14 此前**未**下载
`python-apks/<arch>/` 既有的 `python3-3.12.13-r0.apk` 是 **v3.23（ROUTE A）**；SOURCES.md §8.1 把 ROUTE B（edge/3.14）标为「待跑」，磁盘上**没有任何 3.14 apk**（此前记录「3.14.3 已下载」与文件系统不符）。本次（2026-05-24）下载 edge python 3.14 闭包到 **`python-apks/python314/<arch>/`**（4 arch 各 18 apk，0 fail，gzip 全校验通过）。

### 11.2 关键 ABI 调查结论（edge 的 C 扩展是 cp314）
edge APKINDEX 核对：`python3 3.14.3-r0`（4 arch）；`py3-numpy 2.4.6-r0`、`py3-scikit-learn 1.5.2-r1`、`py3-opencv 4.12.0-r7` 均 `D:python3~3.14` 且 `p:py3.14:numpy=...` → **edge 的 native 扩展确是 cp314 ABI（不是 cp312）**，理论上可做完整 3.14 native 案。

### 11.3 决策：走 **ROUTE B-core（纯 3.14 解释器 + 纯 stdlib，不碰 numpy/sklearn/opencv）**
理由（自行据实判断）：
1. **3.14 LANGUAGE 特性只需解释器 + 纯 stdlib**：PEP750 t-string / PEP758 parenless-except / PEP734 concurrent.interpreters / PEP784 compression / PEP779 GIL 自省 / PEP649-749 annotationlib —— 全不依赖 numpy/openblas/libgfortran。
2. **零 ABI 混装风险**：edge 的 native 重型闭包（numpy/openblas/libgfortran/libstdc++）若混进 v3.23 base，会引入 edge-musl-1.2.6 vs base-musl-1.2.5 的 ABI 风险，**且对 3.14 特性覆盖零增益**。纯解释器闭包（18 apk，**含 musl 1.2.6**）覆盖 python 仅有的 so 依赖 → 整条 3.14 运行时自洽，无残留库错配。
3. **native 扩展已在 python-0/3.12 4-arch 收官**，无需重复 —— 把 native 留 3.12，3.14 专测语言/stdlib，分工清晰。

### 11.4 闭包（edge/main，18 apk，apk-closure.py 解析，0 unsatisfied）
`python3-3.14.3-r0` + `musl-1.2.6-r2` libbz2 libcrypto3 libexpat libffi libgcc libncursesw libpanelw libssl3 libstdc++ mpdecimal ncurses-terminfo-base readline sqlite-libs xz-libs zlib gdbm。
URL：`https://dl-cdn.alpinelinux.org/alpine/edge/main/<ARCH>/python3-3.14.3-r0.apk`（+闭包各 apk 同目录）。脚本：`prep-python314-rootfs.sh <arch>`（**debugfs -w 直写未挂载 ext4，禁 sync**，绕 WSL2 D-state 死锁；resize 2G）。镜像：`tmp/axbuild/rootfs/rootfs-<arch>-python314.img`。

### 11.5 Alpine 未编 `_zstd`（PEP784 zstd capability-gated）
4 arch 的 `python3` apk 均**不含** stdlib `_zstd*.so`（`compression/zstd/*.py` 在，但 C 后端缺）；edge 无任何包提供 stdlib `_zstd`（community `py3-zstd 1.5.7.3-r2` 是无关的第三方 `python-zstd`，非 stdlib 后端）。→ 测例对 `compression.zstd` 做**能力探测**：有 `_zstd` 则断言压缩往返；无则报 `compression.zstd UNAVAILABLE` 且**不失败**（这是发行版 build 取舍，非 StarryOS 缺陷）。PEP784 命名空间本身仍以 `compression.zlib`/`compression.bz2` 真往返断言（这俩 C 后端 `_bz2`/`zlib` 在）。注：Alpine apk 是**默认 GIL build**（abi `cpython-314`，`Py_GIL_DISABLED=0`）→ `sys._is_gil_enabled()` 返回 True（仅自省断言 API 存在+返 bool，不以值定对错）。

### 11.6 用例 = `test-suit/starryos/stress/python-314-0/`
build-*.toml 抄自 python-0；`python-314-0/qemu-<arch>.toml` 用 `rootfs-<arch>-python314.img`，跑 `/tmp/t_py314.py`（3.14 特性穷尽，exact 断言）。gate：`printf 'PYTHON314_OK=%s'`（防 success_regex 假阳性）+ `echo PYTHON314_DONE`。`success_regex=["(?m)^PYTHON314_OK=1"]`、`fail_regex='(?i)\bpanic(?:ked)?\b'`、timeout 1800。host 已验：8 toml tomllib 解析 OK、4 shell `sh -n` OK、3.14-only 语法在 host py3.12 如期 SyntaxError（证真 3.14 专属）、其余 body 中和后 host parse OK。**3.14 active 逻辑靠 starry/真 3.14 跑**（host 无 3.14）。运行：`cargo xtask starry test qemu --arch <arch> -g stress -c python-314-0`。
