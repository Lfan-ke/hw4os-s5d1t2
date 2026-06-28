# 16c · 核内中断 思考题（在每题下作答即可）

> 判据：答案非空且命中关键字（如 `core-local` / `核内` / `核外` / `PLIC` / `mtimecmp` /
> `msip` / `IPI` / `mtip` / `claim` / `仲裁` / `volatile` / `fence`）即过。用自己的话写清楚即可。

## 题：核内（core-local）中断 vs 核外（platform-level）中断；为什么 timer 属核内？

围绕 CLINT（Core-Local Interruptor）回答三件事：

1. 「核内」与「核外」中断各指什么？分界线在哪（私有/共享、要不要路由与 claim/complete 仲裁）？
2. 为什么 **timer**（mtime/mtimecmp）与**软件中断**（msip/IPI）天然属于核内？
   - 提示：每 hart 各一套 `mtimecmp`/`msip`、与本 hart 调度时钟绑定、无需仲裁；
   - 软件中断为何核内私有、却又能通过「写别核的 msip」成为核间中断（IPI）？
3. 为什么 `mtimecmp`/`msip` 这类寄存器必须用 `volatile` 写 + `fence`（回链 16a/16b）？
   它和核外共享外设 IRQ 要走的 PLIC（priority/threshold/claim/complete）有什么本质区别？

## 答

（在此作答）

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
