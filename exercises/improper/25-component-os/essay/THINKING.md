# 25 · 组件化 OS 思考题（essay 变体）

在每题下方写下你的理解即可（命中关键字即通过：统一接口/可替换、cargo features 组装、
按需裁剪、同源组件不同拼法、方法论 vs 形态、unikernel 是一种组装结果、接口契约不变换实现、
arceos/axalloc/axtask/StarryOS 等）。

## 1. 「内核组件化 + cargo features 组装」相比「从头手搓一个内核」，省了什么、换来了什么？为什么同一套组件能拼出 unikernel / 宏内核 / hypervisor 三种形态？

TODO: 在此作答。

## 2. 组件化是方法论，forms（架构形态）是结果。请说明「unikernel 只是一种组装结果」：对照 forms-F4，本课的 `UNI = {最小组件 + 无 syscall 边界}` 与 F4 的「同地址空间、零陷入」是不是一回事？

TODO: 在此作答。

## 3. 「可热替换」为什么重要？拿 arceos 的 `axalloc`（换 bump / slab / buddy）、`axtask`（换 fifo / cfs 调度）举例：组件契约（接口 + 不变量）不变时，换实现为何不影响其余系统？这和本课 `SWAP`（换 freelist+rr）是同一回事吗？

TODO: 在此作答。

## 4. 把本课模型对到真实工程：`make_allocator(kind)` ↔ ?（cargo feature / Kconfig 选实现）、`build_kernel(cfg)` ↔ ?（按 feature 组装依赖图）、`SWAP` ↔ ?（替换一个 mod 实现）。再说说 StarryOS 怎样「复用同一套组件、换个拼法」拼出 Linux 兼容宏内核。

TODO: 在此作答。

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
