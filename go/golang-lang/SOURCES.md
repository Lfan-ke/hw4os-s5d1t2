# go/golang-lang — 资产来源与构建说明

Go **1.26**（go1.26.3）语言级地毯式测试。两条腿：on-target（StarryOS 真跑的静态二进制）+ host-exclusive（宿主独占工具 CLI 地毯）。

## on-target：`golang-lang`（StarryOS 上真跑）

- 源码：`go/`（22 个 `*.go` + `go.mod`/`go.sum`，`package main`）。**2018 条确定化断言**（语言全语法 + 并发 + ~40 标准库包 + Go 1.26 新特性 + 框架运行时）。
- 构建：官方 **go1.26.3** 工具链，`CGO_ENABLED=0 GOOS=linux GOARCH=<target>` 交叉编译为**全静态 ELF**（无 libc/解释器）→ `/usr/local/bin/golang-lang`。
  - 工具链：`https://go.dev/dl/go1.26.3.linux-<host-arch>.tar.gz`（`prebuild.sh` 自动拉取并缓存到 `${GO_CARPET_CACHE:-$HOME/.cache/starry-go-carpet}`）。
  - 框架依赖（按 `go.sum` 固定版本）：gin v1.12.0 · google.golang.org/grpc v1.81.1 · go-zero v1.10.2 · gorm v1.31.1 · `glebarez/sqlite` v1.11.0 → `modernc.org/sqlite` v1.52.0（纯 Go·CGO=0；`modernc.org/libc` v1.73.4 含 **loong64** 支持，故四架构均纯 Go 静态可编译）。
- 产物按需构建：`prebuild.sh` 在 `cargo xtask starry app qemu -t go-lang --arch <a>` 时自动交叉编译并注入 rootfs（不预存二进制——含 `modernc.org/sqlite` 的静态二进制约 100 MB/架构，故只交付源码 + 构建脚本，由维护者按需复现）。
- golden：`golden.txt`（host go1.26.3 生成，2074 行 / `GOLANG count=2018`），on-target 输出逐字节比对。
- 门控：`go/run-go.sh` 跑二进制 → 过滤 go-zero 在无 cgroup 内核下的 `@timestamp` JSON 诊断 → `grep GO_LANG_OK` + `cmp golden` → 仅在两者皆成立时打印 `TEST PASSED`。

## host-exclusive：`host-carpets/`（仅在宿主跑，不在 StarryOS 上跑）

这些是 Go 开发期的**宿主独占工具**（代码生成器 / 工具链 CLI），它们在宿主运行、产物方可在 target 编译执行，故不属于 on-target app：

- `go-cli-carpet.sh` —— `go`/`gofmt`/`go vet`/`go test` 工具链 CLI 地毯（host golden）。
- `goctl-carpet.sh` —— go-zero `goctl 1.10.1` 代码生成器地毯，`GOCTL_COUNT=59`，含两条真 E2E：① `api go` 生成 → `go mod tidy` → `go build` → 启动 HTTP 服务 → curl 断言；② `rpc protoc` 生成 → `go mod tidy` → `go build` → 启动 grpc 服务 → 客户端回显。其余生成器/选项逐项执行；destructive/network/interactive 子命令 `--help` 验证 + SKIP 注明原因。
  - **可移植性**：脚本顶部的 `GOROOT`/`GOPATH`/`GOMODCACHE`/`GOCACHE`/`PATH` 由 `GO_TOOLCHAIN_ROOT` 决定，默认为 `<本机 go 工具链目录>`（须含 go1.26.3 与 goctl 1.10.1）；在其它机器上跑请设 `export GO_TOOLCHAIN_ROOT=<本机 go 工具链目录>`，或 `export GOROOT=... PATH=$GOROOT/bin:$PATH` 后运行。

## 四架构验证（qemu-10 单核 StarryOS）

aarch64 / riscv64 / loongarch64 各 `GO_LANG_OK` + `GOLANG count=2018` + golden 精确匹配 + `SUCCESS PATTERN MATCHED`；x86_64 经上游 CI（本地 app-qemu PVH `-kernel` 加载器限制）。

## 复现

```sh
cargo xtask starry app qemu -t go-lang --arch x86_64    # aarch64 / riscv64 / loongarch64
```
（把本目录作为 `apps/starry/go-lang` 放入 StarryOS 工作区即可。）
