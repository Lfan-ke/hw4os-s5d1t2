# 16a · 总线与缓存 思考题（在每题下作答即可）

> 判据：答案非空且命中关键字（如 `STALE` / `MISSED_SIDEEFFECT` / `UNCACHED` / `SPEEDUP` /
> `直通` / `区间译码`）即过。用自己的话写清楚即可。
> 用 §2.2 地址图（regdev/sensor/switchdev 三段区间 + else=BUS_ERR）与 §2.3 时间模型
> （单次握手 0.2s）作答。

### 1. 仲裁 = 地址区间译码；cache 暂存为何只对“类内存”的 regdev 成立？

（提示：仲裁器就是对每段做 `addr>=base && addr<end` 的比较选设备；cache 暂存隐含“同址读幂等/存储语义”的假设。）

<!-- TODO: 在此作答。 -->

### 2. 突发摊薄握手：算出 BYTE_T / BURST_T / SPEEDUP

（提示：单次握手 0.2s。逐字节传 24B = 多少次握手 = 多少秒？突发一次搬完 = 多少秒？两者之比 SPEEDUP = ?）

<!-- TODO: 在此作答（写出 BYTE_T、BURST_T、SPEEDUP 的数值与推导）。 -->

### 3. sensor / switchdev 两个直通反例，并收束“MMIO 必须 UNCACHED”

（提示：sensor 的 TEMP 每 tick 变 - 缓存会读到 `STALE`；switchdev 写=翻转副作用 - 缓存吞写会
`MISSED_SIDEEFFECT`。由此为什么非存储语义的 MMIO 必须 `UNCACHED`、要 `volatile`/`fence`？）

<!-- TODO: 在此作答。 -->

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
