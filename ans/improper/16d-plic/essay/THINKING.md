# 16d · 核外中断 PLIC 思考题（参考答案）

## 题：把“中断”说清楚为设备树里的一种总线绑定 - 与 16-driver 对位

上一课 16-driver 里，设备节点用 `compatible`（我是谁）+ `reg`（我住哪儿）把自己绑定到
**地址空间**。现在给同一棵树补上中断相关的属性，问：**“中断”凭什么也算一种“总线绑定”？**
`priority/enable/threshold/claim/complete` 各自落在这条绑定链的哪一环？

```dts
/ {
    soc {
        plic: interrupt-controller@c000000 {
            compatible = "riscv,plic0";
            #interrupt-cells = <1>;
            interrupt-controller;          // 我是中断父节点
            reg = <0x0c000000 0x4000000>;
        };

        uart@10000000 {
            compatible = "ns16550a";
            reg = <0x10000000 0x100>;
            interrupt-parent = <&plic>;    // 我的中断挂到 plic
            interrupts = <10>;             // 我的 IRQ 号 = 10
        };

        virtio@10001000 {
            compatible = "virtio,mmio";
            reg = <0x10001000 0x1000>;
            interrupt-parent = <&plic>;
            interrupts = <1>;
        };
    };
};
```

## 答

### 1. 中断 = 一种总线绑定：两条平行的“名片→认领”链

16-driver 教过：`reg = <base size>` 是设备把自己**绑定到地址总线**的一张名片 - 内核解析 dtb，
按 `reg` 把这段物理窗口映射给驱动，`readl/writel` 就能打到它。**中断是完全同构的第二条绑定链**，
只不过绑的不是“地址空间”，而是“中断控制器的输入端口”：

| 维度 | 地址绑定（16-driver） | 中断绑定（本课） |
| :-- | :-- | :-- |
| 绑到谁 | 地址空间（隐含的内存总线） | 中断父节点 `interrupt-parent = <&plic>` |
| 名片属性 | `reg = <base size>` | `interrupts = <irq_id>` |
| 父节点声明 | `#address-cells`/`ranges` | `interrupt-controller` + `#interrupt-cells` |
| 内核动作 | 解析 `reg` → ioremap → 驱动拿到 base | 解析 `interrupts` → 把 irq 号登记进 PLIC → 注册 handler |
| 运行期 | `writel(v, base+off)` | IRQ 触发 → `claim` → handler → `complete` |

所以一个设备节点有**两张名片**：`reg`（“我的寄存器窗口在地址总线的哪一段”）和
`interrupts`（“我的中断线接到中断控制器的第几号输入”）。**`interrupt-parent` 就是中断这条总线的
“总线指针”** - 它指明这张中断名片要拿到哪个控制器去认领，正如地址名片默认拿到根总线认领。
QEMU virt 把 UART 接在 PLIC 第 10 号、virtio 接在第 1 号，这串数字就是**硬件连线**在设备树里的影子。

### 2. `priority/enable/threshold/claim/complete` 在绑定链里各处一环

设备树只完成**静态绑定**（“源 10 = 这台 UART”），PLIC 的五个机制是这条绑定在**运行期的动态语义**：

- **`interrupts = <10>`（设备树）**：把“UART 这个外设”静态绑定到“PLIC 源 10”。这是**编号绑定**，
  对应本课 toy 里 `raise(1<<id)` - 设备拉高它那一号 IRQ 线，置位 `pending[id]`。
- **`priority[10]`（内核 probe 时写）**：给这个源定优先级。设备树只给编号，**优先级是软件策略**，
  由驱动/内核在 probe 阶段写 PLIC 寄存器决定（对应 16-driver 的 “bind 之后驱动初始化设备”）。
- **`enable[context][10]`（内核为某 hart 上下文写）**：决定“哪个 hart 认领这个源”。这正是地址绑定
  没有的一环 - 中断要额外回答**“谁来处理”**。它把“源↔上下文”这条多对多关系定下来。
- **`threshold[context]`**：上下文级的优先级闸门，运行期可调（如临界区里抬高门槛屏蔽低优先级中断）。
- **`claim`（读 CLAIM）/ `complete`（写 CLAIM）**：真正“认领”发生在中断到来时 - hart 读 CLAIM 拿到
  当前最高优先级可见源的 id（**这一步把“源号”翻回“哪台设备的 handler”**，就是 16-driver 的
  “按 compatible 找驱动”在中断侧的镜像：按 irq 号找 handler），处理完写 CLAIM 做 EOI 放行下次中断。

一句话：**`reg` 把设备绑进地址空间、`interrupts` 把设备绑进中断空间**；前者运行期是 `writel`，
后者运行期是 `claim/complete`。两者都是“设备树静态名片 → 内核解析认领 → 运行期访问”的同一套路。

### 3. 为什么必须 `complete`，漏了会怎样

`claim` 时 PLIC 网关把该源标记为 in-service 并清 pending；在 `complete`（写回该 id）之前，网关**不会
再转发同一个源的新中断**。这是防“中断风暴”的去抖/握手：handler 还没处理完，同源的新 IRQ 不应抢进来。
**漏掉 `complete`**：该源被永久卡在 in-service，PLIC 再也不向这个上下文转发它 - 表现为“这台设备第一次
中断后就彻底哑了”。这正是写裸机/内核驱动最常见的一类 bug，对应本课 `CLAIM_PASS` 里“complete 后重新
raise 源3 才能再 claim 到 3”这一步：不走完握手，下一次就路由不出去。

### 4. 小结

16-driver 让设备用 `reg` + `compatible` **绑定到地址总线并认领驱动**；本课让设备用
`interrupts` + `interrupt-parent` **绑定到中断控制器并认领 handler**。`priority/enable/threshold`
是这条中断绑定的“路由策略寄存器”，`claim/complete` 是它的“运行期访问协议” - 
**中断不是地址绑定之外的另一种东西，它就是同一套“设备树名片 → 解析 → 认领 → 访问”落在中断空间的一份拷贝。**
