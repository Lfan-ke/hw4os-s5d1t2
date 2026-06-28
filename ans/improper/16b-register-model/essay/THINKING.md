# 16b · 寄存器模型 思考题（参考答案）

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

## 答

### 1. 逐寄存器判定 RO / WO / RW（看 RegField 的“访问方法”）

- `0x00 CTRL` 全是 `RegField(w, reg)`（既可读又可写）→ **RW** → `ReadWrite`。
- `0x04 STATUS` 全是 `RegField.r`（只读）→ **RO** → `ReadOnly`。
- `0x08 DATA` 是 `RegField.w`（只写）→ **WO** → `WriteOnly`。
- `0x0c ID` 是 `RegField.r`（只读常量 magic）→ **RO** → `ReadOnly`。

Chisel 把寄存器“访问方向”编码在 `RegField` 的构造方法里；tock-registers 把它编码在
`ReadOnly`/`WriteOnly`/`ReadWrite` 的**类型**里 - 同一信息，从“运行期约定”升级成“编译期类型”，
误写 RO 寄存器在 Rust 侧直接不编译。

### 2. 逐位对位（`RegField` 的宽度序列 = `OFFSET/NUMBITS`）

Chisel 的 `Seq(RegField(w1,...), RegField(w2,...), ...)` 是**从 bit0 起依次铺开**：每个 `RegField`
吃掉 `w` 位，下一个接着排。把这串宽度累加，就还原出每个字段的 `OFFSET`：
`en` 占 1 位（OFFSET 0）→ `ie` 占 1 位（OFFSET 1）→ `mode` 占 2 位（OFFSET 2）→ 保留 4 位 →
`rst`（OFFSET 8）。于是逐位抄成：

```rust
register_bitfields![u32,
    CTRL [
        EN   OFFSET(0) NUMBITS(1) [],
        IE   OFFSET(1) NUMBITS(1) [],
        MODE OFFSET(2) NUMBITS(2) [ Off = 0, Blink = 1, Solid = 2, Burst = 3 ],
        RST  OFFSET(8) NUMBITS(1) [],
    ],
    STATUS [ READY OFFSET(0) NUMBITS(1) [], BUSY OFFSET(1) NUMBITS(1) [], IRQ OFFSET(2) NUMBITS(1) [] ],
    DATA   [ BYTE OFFSET(0) NUMBITS(8) [] ],
    ID     [ MAGIC OFFSET(0) NUMBITS(32) [] ],
];

register_structs! {
    pub RegDev {
        (0x00 => ctrl:   ReadWrite<u32, CTRL::Register>),
        (0x04 => status: ReadOnly<u32, STATUS::Register>),
        (0x08 => data:   WriteOnly<u32, DATA::Register>),
        (0x0c => id:     ReadOnly<u32, ID::Register>),
        (0x10 => @END),
    }
}
```

注意两处对位细节：
- `register_structs!` 里的 `0x00/0x04/0x08/0x0c` 偏移，对应 Chisel `regmap` 的那几个 `0x.. ->` 键；
  末尾 `(0x10 => @END)` 等于声明“这个寄存器块到 0x10 为止”（4×u32），tock 会在编译期校验无重叠/无空洞。
- `MODE` 是 2 位**多值字段**（不是标志位），所以用 `NUMBITS(2) [ Off/Blink/Solid/Burst ]` 的枚举，
  而**不能**用 `bitflags!`。`bitflags!` 只适合 `EN`/`IE`、`READY`/`BUSY`/`IRQ` 这类**独立 1 位标志**。

### 3. 为什么 raw ↔ struct 的逐位镜像（MIRROR）必然成立

`RegField` 的宽度序列、`OFFSET/NUMBITS`、C 的位域、Verilog 的 `ctrl_r[3:2]` 切片 - 四者描述的是
**同一个对 32 位整字的划分**：互不重叠、合起来正好覆盖（含保留位）整字。既然是一个**无损的双射划分**，
那么“整字 → 拆成字段 → 再拼回整字”必然回到原值。代码里的 MIRROR 校验正是把这件事跑出来：
C 用 `union{u32 raw; struct fields;}`、Rust 用 `bytemuck::cast::<[u8;16], RegFile>()` 双向转换、
硬件用字段抽头/`Bit` 切片重建整字，全部断言 `重建 == 原始 raw`。这就是“**软件寄存器模型 = 硬件寄存器布局的语法皮**”
最有力的证据：换一种语言、换一层抽象，**逐位**都对得上。

### 4. 小结

Chisel 的 `regmap` 是从**硬件生成侧**描述这张表，tock-registers 的 `register_structs!` 是从
**软件驱动侧**描述同一张表；`RegField.r/.w/()` ↔ `ReadOnly/WriteOnly/ReadWrite`，宽度序列 ↔ `OFFSET/NUMBITS`。
两边抄的是同一份布局，所以 MIRROR 必过 - 这也是 proper `S06d-regmap` 在真 NS16550/PLIC 上要复用的同一手法。
