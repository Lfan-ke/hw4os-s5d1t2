# F6-hybrid 思考题（essay 变体）

在每题下方写下你的理解即可（命中关键字即通过：Executive / Mach+BSD+IOKit、ring0/用户态、
IPC 开销 / 故障隔离、名实不符 / 两头不靠、工程折中 vs 架构纯粹 等）。

## 1. Windows NT 与 macOS XNU 各把哪些服务放内核态、哪些放用户态？为什么算「混合」而非纯微/纯宏？

（提示：NT 的 Microkernel layer / Executive(一堆 Manager) / subsystem；XNU 的 Mach + BSD + IOKit。）

TODO: 在此作答。

## 2. 混合内核常被批评「名实不符 / 两头不靠」。用本 demo 的三条断言（mono<hybrid<micro、隔离服务数居中、kcalls<umsgs）解释：它在哪些维度上既不如宏内核、又不如微内核？

TODO: 在此作答。

## 3. 既然混合「两头不靠」，为什么 Windows + macOS 反而占了 90%+ 桌面市场？工程上的「最优折中」与架构上的「纯粹性」为何不是一回事？

TODO: 在此作答。

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
