# S06d 思考题（参考解） - improper/16 ↔ proper 对标 + tock-registers 写法

## Q1. 「同一份 probe，软件模型 vs 真硬件」 - improper/16 与本实验是什么关系？

improper 的 `16-driver`/`16b-register-model` 在 host 上用一块**被内存背书的寄存器文件**（`union{u32 raw; struct fields}`）当作 `regdev`，跑 probe/bind 的**心智模型**：没有真硬件，寄存器的「副作用」（READY=EN、IRQ=EN&IE）由一个 `dev_sync()` 软件函数模拟。本实验 `S06d` 把**完全相同的手法**搬到 qemu-virt 的**真 NS16550/PLIC**：

- 16b 的 `regdev` 布局表 → 这里的 `struct ns16550_regs` / PLIC 三个 typed 子结构；
- 16b 的「抄布局、逐位一致」→ 这里 `_Static_assert(offsetof(...)==...)` 把偏移钉死在编译期；
- 16-driver 的 `driver_table → match → probe` → 这里同构的 `driver_match → drv->probe`，**只是 probe 体内换成对真寄存器的回环/回读**。

**probe 的控制流不变，变的只是寄存器访问落到内存模型还是真 MMIO**。improper 用最小模型把生命周期讲清楚，proper 把同一份逻辑钉到真硬件上验证 - 这就是「软件能做的硬件也能做」的双向对标。

## Q2. 类型化寄存器图换掉了 `#define` + 裸指针，那 `volatile` 和内存序还需要吗？

需要，而且是地基。寄存器图只改了**寻址的语法**（命名字段代替 `base+off`），没有改**访存语义**：

- `struct ns16550_regs` 的每个成员都是 `volatile uint8_t`，所以 `u->lsr` 仍是一次不可省略、不可合并、不可缓存的真实访存 - 轮询 `THRE` 才不会被优化成死循环。**类型化不能、也不该绕过 `volatile`**：若把成员写成非 `volatile`，编译器照样能把读 `lsr` 提到循环外。
- 多核 / 中断打开后，配置「优先级 → 使能 → 阈值」与「设备拉中断线」之间还需 `fence` 保证设备可见顺序（本实验单核轮询、未开中断，`volatile` 已足够，故未显式 `fence`；S06c/S06e 才需要）。

`tock-registers` 的 `ReadWrite<u8>` 内部正是 `VolatileCell` - 它把 `volatile` 封进类型，不是取消它。

## Q3. PLIC 这种稀疏大窗口为什么不写成一个大 struct？怎么建模才对？

PLIC 窗口 64MB，`priority`(@0x0)、`enable`(@0x2000，每 context 0x80)、`threshold/claim`(@0x200000，每 context 0x1000) 相隔数 MB，中间全是空洞。写成一个 struct 要么塞巨大的 `_reserved[...]` 数组（易错、`register_structs!` 会编译期校验每个空洞），要么放弃。正确做法是**按功能区间拆 typed 子结构 + 索引定位**：

```c
struct plic_priority { volatile uint32_t prio[1024]; };       /* +0x000000 */
struct plic_enable   { volatile uint32_t word[32]; };          /* +0x002000 + ctx*0x80 */
struct plic_context  { volatile uint32_t threshold, claim; };  /* +0x200000 + ctx*0x1000 */
```

调用点 `plic_prio()->prio[irq]` / `plic_enable(ctx)->word[irq/32]` / `plic_context(ctx)->threshold` 既有类型又能索引。`register_structs!` 处理同样的稀疏布局时也是用大间隔偏移把各段隔开（见 Q4）。

## Q4. 把同一张表写成 Rust `tock-registers` - 给出 NS16550 + PLIC 的 `register_structs!`/`register_bitfields!`，并说明 probe 为何能一字不改复用。

NS16550（字节寄存器；位段进 `register_bitfields!`）：

```rust
use tock_registers::registers::{ReadOnly, ReadWrite, WriteOnly};
use tock_registers::{register_bitfields, register_structs};
use tock_registers::interfaces::{Readable, Writeable};

register_bitfields![u8,
    LCR [ DLAB OFFSET(7) NUMBITS(1) [] ],
    MCR [ LOOP OFFSET(4) NUMBITS(1) [] ],
    LSR [ DR OFFSET(0) NUMBITS(1) [], THRE OFFSET(5) NUMBITS(1) [], TEMT OFFSET(6) NUMBITS(1) [] ],
];
register_structs! {
    pub Ns16550 {
        (0x00 => thr_rbr: ReadWrite<u8>),
        (0x01 => ier: ReadWrite<u8>),
        (0x02 => iir_fcr: ReadWrite<u8>),
        (0x03 => lcr: ReadWrite<u8, LCR::Register>),
        (0x04 => mcr: ReadWrite<u8, MCR::Register>),
        (0x05 => lsr: ReadOnly<u8, LSR::Register>),
        (0x06 => msr: ReadOnly<u8>),
        (0x07 => scr: ReadWrite<u8>),
        (0x08 => @END),
    }
}
```

PLIC（稀疏窗口；用大间隔偏移分段，与 C 的「按区间拆子结构」一一对应）：

```rust
register_structs! {
    pub Plic {
        (0x000000 => priority: [ReadWrite<u32>; 1024]),
        (0x001000 => pending: [ReadOnly<u32>; 32]),
        (0x001080 => _reserved0),
        (0x002000 => enable: [[ReadWrite<u32>; 32]; 15872]),   // 每 context 0x80
        (0x1f2000 => _reserved1),
        (0x200000 => context: [ContextCtrl; 15872]),           // 每 context 0x1000
        (0x4000000 => @END),
    }
}
register_structs! {
    pub ContextCtrl {
        (0x0000 => threshold: ReadWrite<u32>),
        (0x0004 => claim: ReadWrite<u32>),
        (0x0008 => _reserved),
        (0x1000 => @END),
    }
}
```

回环自测 / PLIC 配置的 probe 逻辑：`u.mcr.modify(MCR::LOOP::SET)`、`while !u.lsr.is_set(LSR::THRE) {}`、`u.thr_rbr.set(b)`、`plic.priority[irq].set(1)`、`plic.context[ctx].threshold.set(0)` - **控制流与 C 版逐句对应**。这就是为什么 `driver_table → match → probe → bind` 的生命周期代码与寄存器**库**正交：换语言、换 `volatile`/`register_structs!` 实现，probe 的算法不变。

> runnable 变体以 **C（在 qemu-virt 上运行）** 为准。`tock-registers` 可离线编入 `riscv64gc-unknown-none-elf` 的 no_std 内核，但默认 `rust-toolchain.toml` 固定 `channel="stable"`，该工具链在当前环境缺失对应二进制，`labctl` 的 `make → cargo build` 会落到不可用的 stable；为避免为实验固定一个环境特定的 nightly，Rust/tock-registers 的完整写法见本思考题（对标 16b 的 Rust 四级），与 C 版逐位同构。
