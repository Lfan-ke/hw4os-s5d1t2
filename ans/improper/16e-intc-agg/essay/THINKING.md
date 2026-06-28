# 16e · 中断聚合 · 多核仲裁 思考题（参考答案）

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

---

## 答

### 1. 谁处理该 IRQ？

**claim 读到非 0 的那一个 hart** - 而且只会有一个。该外设源虽然对每个 hart 都有一份 context，
中断到来时多个 hart 都可能被通知去跑 `plic_handle_irq`；但 PLIC 的 **source gateway** 保证：
某个源一旦被某个 hart 的 `(A) claim`（`readl(claim)`）读走，gateway 立即进入 **in-flight**，
该源不再向**任何** hart 的 context 转发。于是**抢先 claim 到非 0 的 hart 成为唯一处理者**，
其它 hart 的 `readl(claim)` 读到 **0**，`while` 直接退出、什么也不做。

这就是“多核仲裁”的全部秘密：**不需要软件自旋锁**，gateway 这个硬件状态机本身就是仲裁器 - 
等价于我们模型里 `amoswap.w`/`AtomicU32::swap` 把 in-flight 标志从 0 原子换成 1，只有换到旧值 0 的 hart 算赢。

### 2. 为什么必须 `complete`？

`(A) claim` 把源置成 in-flight（pending 清零、暂停对外转发），是为了“处理期间不要重复投递”。
`(B) complete`（`writel(hwirq, claim)`，写回同一个寄存器、同一个中断号）告诉 PLIC：
**本 hart 已处理完该源**，gateway 重新武装回空闲，源此后才能**再次**被任何 hart claim。
claim 与 complete 是一对**握手**：claim 借走 gateway，complete 归还 gateway。

### 3. 漏 `complete` 会怎样？

gateway 会**永久停在 in-flight**。这意味着：

- 该外设源此后**再也不会被转发**给任何 hart - 这个设备的中断等于**永久丢失 / 被静默掉**（设备看起来“卡死”）。
- 在 `plic_handle_irq` 的 `while` 里，若处理了却忘了 `(B)`，下一轮 `readl(claim)` 行为依赖实现：
  要么读到 0 提前退出（漏掉本该继续的中断），要么在某些设计上反复读到同号造成**忙等 / 重复处理**。

一句话：**claim 不配对 complete = 把 gateway 借走不还**，整条中断线就此静默。这是真实驱动里最常见的一类“中断丢失”缺陷。

### 4. IPI 的内存序为什么要 `barrier`？

跨核传数据有两个**配对**的内存序点：

- 发送侧 `(C) smp_wmb()`：必须在 `(D)` 敲 IPI（写 MSIP）**之前**，把 `csd->func/info` 的写**刷成对端可见**。
  否则在弱内存序（RISC-V 是弱序）下，目标 hart 可能**先看到 IPI、却读到尚未更新的 `csd`**（旧值 / 空指针）。
- 接收侧 `(E) smp_mb()`：目标 hart 在跑 `csd->func` **之前**插屏障，保证“**先观察到 IPI 这件事，再读 `csd`**”的顺序。

这正是 **release（发送写屏障）/ acquire（接收读屏障）配对**，与我们模型里
`fence(Ordering::Release)` ↔ `fence(Ordering::Acquire)`、C 的 `fence rw,rw` 一一对应。
口诀：**生产者“先写数据、barrier、再敲 IPI”；消费者“见 IPI、barrier、再读数据”** - 少任一道 barrier，弱序机器上就可能读到旧数据。

### 5. 小结

`claim/complete` 把“**同一 IRQ 只一个 hart 处理**”做成硬件 gateway 的借/还握手（仲裁不靠锁）；
IPI 把“**一个 hart 协调另一个 hart**”做成写 MSIP + `barrier` 的发布/获取握手（跨核可见靠内存序）。
两件事合流，就是多核中断聚合的真身 - proper `S06e-ipi` 在 qemu-virt `-smp` 上把它跑成真内核。
