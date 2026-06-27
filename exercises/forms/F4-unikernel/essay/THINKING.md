# 形态 F4 · 库OS / Unikernel 思考题（essay 变体）

在每题下方写下你的理解即可（命中关键字即通过：同地址空间/无陷入、静态链接、
编译期特化/裁库、单应用、serverless/FaaS 冷启动、MirageOS/Unikraft/HermitOS 等）。

## 1. 为什么 Unikernel 的 app→OS 是「直接函数调用、零陷入」？省掉了传统系统调用的哪些开销？又因此失去了什么（保护边界 / 隔离 / 单应用故障域）？

TODO: 在此作答。

## 2. 为什么「单应用」是「编译期特化裁剪」能成立的前提？裁掉未用子系统换来了什么（镜像大小 / 启动时间 / 攻击面），失去了什么（通用性 / 复用）？拿 Unikraft 的 88 微库 / MirageOS 类型驱动 / HermitOS feature 举例。

TODO: 在此作答。

## 3. LibOS / Unikernel 思想 1995 年就有（Exokernel + jos），为什么 2020 年后才借 serverless / FaaS（AWS Lambda 微 VM、Cloudflare Workers）二度复兴？「一镜像一应用、毫秒级冷启动、极小攻击面」为何正好命中云函数场景？

TODO: 在此作答。

## 4. 把本课模型对到真实工程：`uni_*` 直接链接 ↔ ?（静态链接 libOS）、陷入计数=0 ↔ ?（同地址空间无 ecall）、`image_symbols` 变小 ↔ ?（Kconfig/feature 裁库）。各举一个 MirageOS / Unikraft / HermitOS 的对应点。

TODO: 在此作答。

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
