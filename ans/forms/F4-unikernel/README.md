# 形态 F4 · 库OS / Unikernel：OS 是一个被 app 直接链接的库

> 「内核形态认知」专题 · host 软件直觉 demo（不是完整内核）。
> 一句话母题：**传统 OS 把内核摆在另一头，应用每要一次服务就得「陷入」(trap)
> 切到内核态；Unikernel 反其道——把 OS 例程当成普通库直接链接进应用，
> 同一个地址空间里，一次 `call` 就办了，根本没有「陷入」这回事。
> 产物是一份镜像 = 一个应用 + 它需要的那部分 OS。**

## 0. 这节课在讲什么

五种内核形态（宏内核 / 微内核 / 外核 / Unikernel / 多内核…，见
`notes/04-02-os-kernel-paradigms.md`）里，Unikernel 的本质权衡最干脆：

- **同地址空间、无保护边界** → 没有 user/kernel 之分，OS 函数就是库函数，
  `app → OS` 是直接函数调用，**零陷入开销**（也意味着零隔离：app 崩了 OS 也没了）。
- **单应用、编译期特化** → 一份镜像只跑一个应用，于是可以在**编译期**就把
  这个应用不用的子系统（网络栈 / 块设备 / 多进程 / VFS…）整段裁掉，镜像更小、
  攻击面更窄、启动更快（代价：通用性没了，换个应用要重新编一份镜像）。

真实代表：**MirageOS**（OCaml）、**Unikraft**（C，88 个微库可裁剪）、
**HermitOS**（Rust，自称 "library operating system / compiles to a static library"）。
思想源头是 1995 Exokernel 论文 + MIT jos 的 LibOS 设计；2020 后被 serverless / FaaS
（AWS Lambda 微型运行时、Cloudflare Workers 思路）重新带火。
参考 `notes/04-09-unikernel-libos-walkthrough.md`、`core/unikraft`、`core/libos`。

本课用最朴素的 host 软件模型把这两个权衡「演」出来——你不写真内核，
而是用普通函数调用 + 一个「陷入计数器」+ 一张「模块表」把直觉建起来。

## 1. 你要填的 6 个函数

软件在 `sw/rust/src/main.rs` 或 `sw/c/unikernel.c`，两份逻辑逐字对应。
模型里有一个 `UniKernel` 结构（console / heap_top / ticks）——它就是「被链接进
app 的那点 OS 状态」，和 app 同处一个地址空间。

| 区 | 函数 | 要求 | 判据 |
| :-- | :-- | :-- | :-- |
| OS 例程 | `uni_write` | console_len += n，返回 n（写了几字节） | `UNI_PASS` |
| OS 例程 | `uni_alloc` | bump 分配：返回旧 top，再把 top 前移 n | `UNI_PASS` |
| OS 例程 | `uni_clock` | 返回当前 tick，再自增（单调） | `UNI_PASS` |
| 直接绑定 | `dispatch_direct` | 按 `svc` 直接调对应 `uni_*`，**不碰陷入计数器** | `DIRECT_PASS` |
| 特化开关 | `is_linked` | 模块用到才链入：`(used & bit) != 0` | `SPECIALIZE_PASS` |
| 特化开关 | `image_symbols` | 累加「被链入」模块的 size = 镜像符号数 | `SPECIALIZE_PASS` |

四段皆过再打印 `ALL_PASS`。

## 2. 四个判据在演什么

1. **`UNI_PASS` — OS 例程就是库函数**：`uni_write/uni_alloc/uni_clock` 是普通函数，
   app 直接 `call`，结果立刻拿到——这就是「OS 编译进应用、同地址空间」。

2. **`DIRECT_PASS` — 直接调用替代陷入**：同一份工作负载跑两遍。
   - 传统模型 `dispatch_trap`（harness 给定）：每次系统服务都 `traps += 1`，
     模拟 user→kernel 的模式切换 / `ecall` 陷入。N 次调用 = N 次陷入。
   - Unikernel 模型 `dispatch_direct`（你填）：直接函数调用，**陷入计数恒为 0**。
   两者业务结果逐项一致——区别只在「有没有那道陷入墙」。打印
   `TRAP_COST 传统模型陷入=N unikernel 陷入=0`。

3. **`SPECIALIZE_PASS` — 编译期特化裁剪**：镜像 = 一组模块
   （console / alloc / clock / net / blk / fs，各有 size = 符号数）。app 只声明用到
   `console/alloc/clock`（`APP_USES`）。`is_linked` 决定每个模块是否进镜像，
   `image_symbols` 把进镜像的累加。结论：特化镜像 30 < 全量 152，
   且 net/blk/fs 被裁掉、用到的仍在——「镜像变小」用符号计数模拟出来。

4. **`IMAGE_PASS` — capstone：一镜像 = app + OS**：构造 `APP_USES` 的特化镜像，
   把 app `main` 跑起来——全程经 `dispatch_direct` 直接调 OS，banner 正确、
   两次 alloc 区间不重叠、clock 单调、**全程 0 陷入**、且未用模块不在镜像里。
   这就是「单应用 + 单地址空间 + 一份可启动镜像」的缩影。

```
labctl run forms/F4-unikernel       # 跑 rust / c 两条路径
labctl watch                        # 边改边自动判定
labctl hint forms/F4-unikernel      # 卡住看提示
```

## 3. 完成标准 (DoD)

- [ ] 至少一条变体跑出 `UNI_PASS`/`DIRECT_PASS`/`SPECIALIZE_PASS`/`IMAGE_PASS`/`ALL_PASS`，无任何 `*_FAIL`（必修）。
- [ ] 能口述：为什么 Unikernel 的 `app→OS` 是 0 陷入？代价是什么（隔离）？
- [ ] 能口述：为什么「单应用」是「编译期特化裁剪」的前提？裁掉换来什么、失去什么？
- [ ] essay 答出「静态链接、特化、cloud/FaaS 复兴、单应用」与三个真实工程的对应。

## 4. 关键约定（判题用）

- `uni_alloc` 是 **bump 分配**：返回的是分配前的 `heap_top`，随后 `heap_top += n`；
  连续两次 `alloc(32)`/`alloc(16)` 的偏移是 `0` 和 `32`（区间不重叠）。
- `uni_clock` 先返回**当前** tick 再自增：连续三次得 `0,1,2`。
- `dispatch_direct` 与 harness 的 `dispatch_trap` 调用**同一批** `uni_*`——
  唯一差别是后者多了一次 `traps += 1`。这正是「内核代码相同，差别只在那道陷入墙」。
- 特化：`is_linked(used,bit) = (used & bit)!=0`；`image_symbols(used)` 只累加链入模块。
  `APP_USES = console|alloc|clock`，故 `image_symbols(APP_USES)=30 < image_symbols(ALL)=152`。
- 失败会打印含 `FAIL` 的诊断行（如 `TRAP_LEAK_FAIL` 直接调用却产生陷入、
  `SPECIALIZE_BLOAT_FAIL` 特化没让镜像变小）。`TRAP_COST`/`IMAGE_SIZE` 是信息行，非判据。

## 5. 思考题（`essay/THINKING.md` 作答即可通过）

1. Unikernel 的 `app→OS` 为什么是「直接函数调用、零陷入」？这省掉了传统系统调用的哪些开销？又因此**失去**了什么（提示：保护边界 / 隔离 / 单应用故障域）？
2. 为什么「单应用」是「编译期特化裁剪」能成立的前提？拿 Unikraft 的 88 个微库 / MirageOS 的类型驱动裁剪举例：裁掉未用子系统换来了什么（镜像大小 / 启动时间 / 攻击面），失去了什么（通用性 / 复用）？
3. LibOS / Unikernel 思想 1995 年就有（Exokernel + jos），为什么 2020 年后才借 **serverless / FaaS**（AWS Lambda 微 VM、Cloudflare Workers）二度复兴？「一镜像一应用、毫秒级冷启动、极小攻击面」为何正好命中云函数场景？
4. 把本课模型对到真实工程：`uni_write` 直接链接 ↔ ?（静态链接 libOS）、陷入计数=0 ↔ ?（同地址空间无 `ecall`）、`image_symbols` 变小 ↔ ?（Kconfig/feature 裁库）。各举一个 MirageOS / Unikraft / HermitOS 的对应点。
