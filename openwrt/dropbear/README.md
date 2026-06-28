# dropbear — 轻量 SSH 服务器 (#764 openwrt)

**dropbear** SSH server 在 StarryOS 四架构单核 qemu-10 上全部通过。

## DoD 结论

| arch | 结果 |
|:--:|:--:|
| x86_64 | √ `DROPBEAR_OK=1` |
| aarch64 | √ `DROPBEAR_OK=1`（`-cpu cortex-a72`）|
| riscv64 | √ `DROPBEAR_OK=1` |
| loongarch64 | √ `DROPBEAR_OK=1`（`-cpu la464 -machine virt`）|

测试内容：dropbear 生成主机密钥 + 监听 `127.0.0.1:2222`（socket/bind/listen/accept over loopback TCP）+ accept 时熵源播种 KEX；`nc` 发起连接后断言 dropbear 日志出现 `Child connection from 127.0.0.1`。验证 loopback TCP 数据通路 + 熵 + 主机密钥加载（无需 PTY/真 SSH 客户端）。

## 构建运行

```bash
bash prep-openwrt-rootfs.sh <arch>          # 注入 dropbear + openwrt 闭包
cargo xtask starry test qemu --arch <arch> -g stress -c dropbear-0
# 成功判据: ^DROPBEAR_OK=1
```
