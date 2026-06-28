# 16b · 寄存器模型 思考题（在每题下作答即可）

> 判据：答案非空且命中关键字（如 `register_structs` / `ReadOnly` / `OFFSET` / `MIRROR` /
> `ReadWrite` / `WriteOnly` / `逐位`）即过。用自己的话写清楚即可。

## 题：把下面这段 Chisel `regmap` 抄成 `register_structs!` + `register_bitfields!`

rocket-chip / diplomacy 风格的寄存器图描述（`RegField(w, reg)`=RW、`RegField.r`=只读、
`RegField.w`=只写、`RegField(w)`=保留填充）：

```scala
node.regmap(
  0x00 -> Seq(                       // CTRL
    RegField(1, ctrl.en),            //   bit0  RW
    RegField(1, ctrl.ie),            //   bit1  RW
    RegField(2, ctrl.mode),          //   bit3:2 RW（off/blink/solid/burst）
    RegField(4),                     //   bit7:4 保留
    RegField(1, ctrl.rst)),          //   bit8  RW
  0x04 -> Seq(                       // STATUS
    RegField.r(1, status.ready),     //   bit0  RO
    RegField.r(1, status.busy),      //   bit1  RO
    RegField.r(1, status.irq)),      //   bit2  RO
  0x08 -> Seq(                       // DATA
    RegField.w(8, data.byte)),       //   bit7:0 WO
  0x0c -> Seq(                       // ID
    RegField.r(32, magic)))          //   bit31:0 RO
```

### 1. 逐寄存器判定 RO / WO / RW - 各对应 tock-registers 的哪种类型？

（提示：`RegField(w,reg)` / `RegField.r` / `RegField.w` 各对应 `ReadWrite` / `ReadOnly` / `WriteOnly`。）

<!-- TODO: 在此作答。 -->

### 2. 逐位对位 - 把 `RegField` 的宽度序列翻成 `OFFSET/NUMBITS`，写出 register_bitfields!/register_structs!

（提示：`Seq(RegField(w1,...), RegField(w2,...), ...)` 从 bit0 起依次铺开；累加宽度即得每个字段的 OFFSET。
为什么 `MODE` 必须用 `NUMBITS(2)` 而不能用 `bitflags!`？）

<!-- TODO: 在此作答（贴出你抄出的 register_bitfields! 与 register_structs!）。 -->

### 3. 为什么 raw ↔ struct 的逐位镜像（MIRROR）必然成立？

（提示：宽度序列 / 位域 / wire 切片描述的是对同一个 32 位整字的“无损双射划分” - 拆了再拼必回原值。）

<!-- TODO: 在此作答。 -->

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
