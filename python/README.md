# Python 生态 — StarryOS 四架构单核交付 (#764)

CPython 3 运行时 + 主流科学计算/Web/微服务库，在 StarryOS 四架构（x86_64/aarch64/riscv64/loongarch64）单核 qemu-10 上验证。

## 子项与 DoD 结论

| 子项 | 目录 | 测试内容 | 成功判据 | 4-arch |
|------|------|----------|----------|--------|
| **core** | `core/` | CPython 3 + numpy(线性代数/ndarray) + scikit-learn(模型 fit/predict) | `^PYTHON_OK=1` | √ 4/4 |
| **frameworks** | `frameworks/` | django + fastapi + langchain + strawberry(GraphQL) + uvicorn(ASGI) | `^PYTHON_FW_OK=1` | √ 4/4 |
| **uv-venv** | `uv-venv/` | uv 包管理器 + venv 虚拟环境创建/激活/装包 | `^PYTHON_UV_OK=1` | √ 4/4 |
| **celery** | `dod-frameworks/celery/` | celery 5.5.3 + flower 任务队列(memory:// broker) | `^CELERY_OK=1` | √ 4/4 |
| **pyarrow** | `data/pyarrow/` | Apache Arrow 列式内存(array/table/compute) | `^PYARROW_OK=1` | √ 4/4 |
| **jupyter** | `jupyter/` | JupyterLab 4.5.7 服务器启动 + /lab 前端 + REST API 可达 | `^JUPYTER_OK=1` | √ 4/4 |

> opencv 亦在 #764 勾选（图像 IO/矩阵运算），随 frameworks/core 闭包注入。
> 余下深度学习类（triton/jittor/keras3/automl）难度较高、属推迟批次，不在本交付（见跟踪 issue）。

## 构建运行（每子项）

```bash
bash prep-python-rootfs.sh <arch>           # 注入 CPython + 库闭包(musl wheel/apk)
cargo xtask starry test qemu --arch <arch> -g stress -c <python-0|python-fw-0|python-uv-0>
# 成功判据见上表 ^*_OK=1
```

依赖 starry: futex / mmap / /proc / signal / epoll / 线程 等已成熟支持。
