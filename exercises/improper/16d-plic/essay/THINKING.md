# 16d · 核外中断 PLIC 思考题（在每题下作答即可）

> 判据：答案非空且命中关键字（如 `interrupts` / `interrupt-parent` / `claim` / `complete` /
> `priority` / `enable` / `threshold` / `绑定` / `compatible`）即过。用自己的话写清楚即可。

## 题：把“中断”说清楚为设备树里的一种总线绑定 - 与 16-driver 对位

上一课 16-driver 里，设备节点用 `compatible`（我是谁）+ `reg`（我住哪儿）把自己绑定到
**地址空间**。现在给同一棵树补上中断相关的属性：

```dts
plic: interrupt-controller@c000000 {
    compatible = "riscv,plic0";
    #interrupt-cells = <1>;
    interrupt-controller;
    reg = <0x0c000000 0x4000000>;
};

uart@10000000 {
    compatible = "ns16550a";
    reg = <0x10000000 0x100>;
    interrupt-parent = <&plic>;    // 我的中断挂到 plic
    interrupts = <10>;             // 我的 IRQ 号 = 10
};
```

### 1. 中断为什么也算“一种总线绑定”？把它与 16-driver 的 `reg`/`compatible` 逐条对位。

（提示：`reg` 把设备绑到地址空间；`interrupts` + `interrupt-parent` 把设备绑到哪儿？
运行期 `writel` 对应中断侧的什么操作？）

<!-- TODO: 在此作答。 -->

### 2. `priority` / `enable` / `threshold` / `claim` / `complete` 各处在这条绑定链的哪一环？

（提示：设备树只给静态编号绑定 `interrupts = <10>`；优先级/使能/阈值是谁在何时写的？
`claim` 把“源号”翻回成什么 - 与 16-driver “按 compatible 找驱动”有何镜像关系？）

<!-- TODO: 在此作答。 -->

### 3. 为什么必须 `complete`？漏掉会发生什么？

（提示：claim 后该源进入 in-service 且 pending 被清；complete 之前网关不再转发同源。）

<!-- TODO: 在此作答。 -->

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
