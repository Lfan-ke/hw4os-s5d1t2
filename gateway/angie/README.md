# angie — nginx 兼容网关/反向代理 (gateway/angie)

**angie**（webserver-llc 维护的 nginx 兼容 fork，C 语言 HTTP 服务器/反向代理）在 StarryOS 四架构单核 qemu-10 上**全绿 4/4**。

## DoD 结论（qemu-10 单核 starry，2026-06-04）

| arch | 结果 | 备注 |
|------|------|------|
| x86_64 | √ `ANGIE_OK=1` | apk 二进制 |
| aarch64 | √ `ANGIE_OK=1` | `-cpu cortex-a72`；apk 二进制 |
| riscv64 | √ `ANGIE_OK=1` | `-cpu rv64`；**源码交叉编译**（上游 Alpine 无 riscv64 apk）|
| loongarch64 | √ `ANGIE_OK=1` | `-cpu la464 -machine virt`；**源码交叉编译**（上游 Alpine 无 loong apk）|

判据权威：xtask `rc=0` + 日志 `SUCCESS PATTERN MATCHED` + `1/1 case(s) passed` + 锚定 `^ANGIE_OK=1`，四架构各复核一致，`getpwnam` 错误 0。

测试内容（与 sibling `gateway-nginx` 同款）：`angie -v` 断言版本 `Angie/1.11.5` → `angie -t` 配置语法检查 → 前台单进程（`master_process off`）监听 `127.0.0.1:8080`、`location / { return 200 "ANGIE_OK_BODY"; }` → busybox `wget` 经 loopback 取响应、断言响应体精确等于 `ANGIE_OK_BODY`。验证 angie 进程模型 + epoll 事件循环 + loopback TCP/HTTP 通路。

## qemu-10 真 Linux 4-arch 参照（测例可行性基线）

同测例在真 Linux 内核（Alpine linux-lts/virt 6.18.33 @qemu-10）上 x86_64/aarch64/riscv64 均 `ANGIE_OK=1`；loongarch64 二进制+逻辑经 qemu-user 验证 `ANGIE_OK=1`（其 system-boot 受 qemu loong 内核 i8042 driver 限制，与 angie 无关）。因此测例逻辑在真 Linux 四架构均成立，starry 全绿可信。

## 来源（provenance，每个二进制据实注明）

- **x86_64 / aarch64**：angie 官方 Alpine musl apk `angie-1.11.5-r0`（来自 `download.angie.software/angie/alpine/v3.23/main/<arch>`）+ 依赖闭包 pcre2 10.47 / zlib 1.3.2 / openssl 3.5.6（Alpine v3.23 main）。
- **riscv64 / loongarch64**：angie 官方 Alpine 仓库**只发 x86_64+aarch64**（rv/loong APKINDEX 404）→ 从 angie 1.11.5 源码（`download.angie.software/files/angie-1.11.5.tar.gz`）用 musl 交叉工具链交叉编译：
  - riscv64：`riscv64-linux-musl-gcc` 11.2.1，`Angie/1.11.5`，SHA256 `14597807ee474d84984473e89c863800a8f4f0a8d16dd602364e0393a44ac5e6`
  - loongarch64：`loongarch64-linux-musl-gcc` 13.2.0，`Angie/1.11.5`，SHA256 `1df8885a4a83530c5ff117659f46e3d4083ace2cb4b871bd6dce2845b9da5d3b`
  - 链接 pcre2 10.47 / zlib / openssl（从 Alpine v3.23 nginx 同版 apk 提取）；最小模块集（http core + rewrite + headers，去 proxy/fastcgi/uwsgi/scgi/gzip）。
- 完整 provenance（configure 行、post-link patchelf、依赖闭包）见 `<本机下载缓存目录>/gateway-bins/SOURCES.md` §4。

## 构建运行

```bash
bash case/prep-angie-rootfs.sh <arch>        # debugfs 注入 angie 二进制 + libpcre2 闭包 + 最小 angie.conf
cargo xtask starry test qemu --arch <arch> -g stress -c angie-0   # source .starry-env.sh 用 qemu-10
# 成功判据: ^ANGIE_OK=1
```

注：`prep-angie-rootfs.sh` 对 x86_64/aarch64 用 apk 闭包，对 riscv64/loongarch64 用 `download/gateway-bins/angie/<arch>/payload/` 的源码构建产物；conf 显式 `user root;`（apk angie 编译期默认 `--user=angie`，rootfs 无该账户会 `getpwnam` `[emerg]` 退出），rv/loong minimal build 不发 proxy/fastcgi 等 `*_temp_path` 指令。
