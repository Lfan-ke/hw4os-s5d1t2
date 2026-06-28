# tinygo — 来源 / 构建 provenance

为 [#764](https://github.com/rcore-os/tgoskits/issues/764)「`tinygo`」项。TinyGo(基于 LLVM 的 Go 编译器)。

## 真实 4 架构覆盖度(据实)

| 架构 | 状态 | 说明 |
|:--:|:--:|:--:|
| **x86_64** | √ **starry 绿** | tinygo 编出 **musl 动态**二进制(interp `/lib/ld-musl-x86_64.so.1`,alpine rootfs 自带);starry 上 `TINYGO_OK_GATE=1`,stdout 精确匹配 host 黄金 |
| **aarch64** | √ **starry 绿** | `GOARCH=arm64` 交叉,musl **静态**;starry 上 `TINYGO_OK_GATE=1`,精确匹配黄金 |
| **riscv64** | × **上游未支持** | `GOARCH=riscv64` → tinygo `unknown GOARCH`(tinygo 仅 linux x86/arm64,其余为 MCU 板);非本地交叉器能补 |
| **loongarch64** | × **上游未支持** | `GOARCH=loong64` → tinygo `unknown GOARCH` |

> tinygo 的 riscv64/loong64 是**上游语言项目未移植该架构后端 + Go runtime port**(target 定义 + `task_stack_<arch>.S` 协程切换汇编 + syscall ABI,1–3 千行含汇编,3–6 周/架构),既非本地 musl-cross 能补、也非本地构建能变出。**据实交付口径:2/4 真做 + 2/4 上游缺口据实记录**(非假绿);逐架构覆盖度见同级 `LANG-4ARCH-ANALYSIS.md`。

## 工具链
- **TinyGo 0.40.0**(github tinygo-org/tinygo,内置 go1.22.2 + LLVM 20.1.1 + 自带 musl)。
- `GOOS=linux GOARCH=arm64 tinygo build`(aarch64 静态);x86_64 默认 musl 动态(依赖落 alpine rootfs)。

## 测试 (`main.go`)
综合 TinyGo-supported Go,确定性输出:goroutine+channel+WaitGroup(并发求和)、sync/atomic、泛型(`Sum[T]`/`Map`)、select、interface 动态分发、map+sort、closure、defer LIFO。golden 11 行结尾 `TINYGO_OK`。

## 结论 (2026-05-30)
- host(tinygo 0.40)跑 main.go → 黄金 `golden.txt`;
- x86_64(musl 动态)+ aarch64(musl 静态)交叉 → **starry 2/4 绿**(`TINYGO_OK_GATE=1`,qemu-user md5 全等黄金);
- riscv64/loong64 = 上游 `unknown GOARCH`。

## 文件
- `main.go` · `golden.txt` · `prep-tinygo-rootfs.sh` · `testbin/tinygo-{x86_64,aarch64}` · `build-*.toml` + `qemu-*.toml`(x86_64+aarch64)。
- 对应 case `test-suit/starryos/stress/tinygo-0/`。aarch64 toml 须 `-cpu cortex-a72`。
