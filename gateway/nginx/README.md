# nginx — 网关/反向代理 (gateway)

**nginx** 在 StarryOS 四架构单核 qemu-10 上全绿。

## DoD 结论

| arch | 结果 |
|------|------|
| x86_64 | √ `GATEWAY_OK=1` |
| aarch64 | √ `GATEWAY_OK=1`（`-cpu cortex-a72`）|
| riscv64 | √ `GATEWAY_OK=1` |
| loongarch64 | √ `GATEWAY_OK=1`（`-cpu la464 -machine virt`）|

测试内容：nginx 启动 + 加载配置 + 监听 + 经 loopback HTTP 取静态响应（断言 HTTP 200 + 响应体）。验证 nginx master/worker 进程模型 + epoll/事件循环 + loopback TCP/HTTP 通路。

## 构建运行

```bash
bash prep-gateway-rootfs.sh <arch>          # 注入 nginx + 网关闭包
cargo xtask starry test qemu --arch <arch> -g stress -c gateway-0
# 成功判据: ^GATEWAY_OK=1
```
