# 16e · 中断聚合 · 多核仲裁 思考题（在每题下作答即可）

> 判据：答案非空且命中关键字（如 `claim` / `complete` / `hart` / `barrier` / `gateway` / `in-flight` /
> `release` / `acquire`）即过。用自己的话写清楚即可。

## 题：读这段缩减版真内核源码，回答四问

下面是 Linux `drivers/irqchip/irq-sifive-plic.c` 与 RISC-V 跨核 IPI 的**缩减**片段
（去掉锁/容错/边界，只留主干）。`__iomem` 指 MMIO，`readl/writel` 即 `lw/sw`。

```c
/* 每个 hart 有自己的一份 context：claim/complete 共用同一个寄存器 */
struct plic_handler {
    void __iomem *hart_base;      /* 本 hart-context 的寄存器窗口 */
};
static DEFINE_PER_CPU(struct plic_handler, plic_handlers);

/* 外设中断到达：本 hart 在自己的 context 上 claim → 处理 → complete */
static void plic_handle_irq(struct irq_desc *desc)
{
    struct plic_handler *h = this_cpu_ptr(&plic_handlers);
    void __iomem *claim = h->hart_base + CONTEXT_CLAIM;
    irq_hw_number_t hwirq;

    while ((hwirq = readl(claim))) {                 /* (A) claim：读到非 0 = 本 hart 抢到该源 */
        generic_handle_domain_irq(h->priv->irqdomain, hwirq);  /* 跑设备处理函数 */
        writel(hwirq, claim);                        /* (B) complete：写回同号，通知 PLIC 处理完 */
    }
}

/* 跨核：把一个调用请求发给目标 hart（典型如 smp_call_function_single）*/
void send_call_single(int cpu, smp_call_func_t func, void *info)
{
    struct __call_single_data *csd = &per_cpu(csd_data, cpu);
    csd->func = func;
    csd->info = info;
    smp_wmb();                                       /* (C) barrier：数据先于 IPI 可见 */
    arch_send_call_function_single_ipi(cpu);         /* (D) 写目标 hart 的 CLINT MSIP = 敲 IPI */
}

/* 目标 hart 的软件中断处理：收到 IPI 后跑回调 */
void ipi_handler(void)
{
    struct __call_single_data *csd = this_cpu_ptr(&csd_data);
    smp_mb();                                        /* (E) barrier：先看到 IPI，再读 csd */
    csd->func(csd->info);
}
```

### 1. 谁处理该 IRQ？同一个源对着 N 个 hart，为什么最终只有一个核处理？

（提示：`(A)` 的 `readl(claim)` 读到什么算“抢到”？PLIC 的 source **gateway** / **in-flight** 状态如何让其余 hart 的 claim 读到 0？
和模型里 `amoswap.w` / `AtomicU32::swap` 把标志 0→1 的“唯一赢家”是不是同一件事？）

<!-- TODO: 在此作答。 -->

### 2. 为什么必须 `complete`（即 `(B)`）？claim 与 complete 是怎样一对握手？

（提示：`(A) claim` 把 gateway 置 in-flight 是为了什么？`(B)` 写回同号又把它怎样了？）

<!-- TODO: 在此作答。 -->

### 3. 漏掉 `complete` 会怎样？

（提示：gateway 永远停在 in-flight，该源此后还会被转发给任何 hart 吗？`while` 循环里漏写又会发生什么？）

<!-- TODO: 在此作答。 -->

### 4. IPI 的内存序：`(C)` 与 `(E)` 两道 `barrier` 各保证什么？少了会怎样？

（提示：发送侧 `smp_wmb()` 让 `csd` 的写先于 `(D)` 敲 IPI 可见；接收侧 `smp_mb()` 让“看到 IPI”先于读 `csd`。
这是不是 release/acquire 配对？弱内存序下少了 barrier，目标 hart 会不会“见到 IPI 却读到旧数据”？）

<!-- TODO: 在此作答。 -->

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
