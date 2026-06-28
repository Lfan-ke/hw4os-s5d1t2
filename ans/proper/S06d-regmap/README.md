# 正经·S06d · 类型化寄存器图 + 平台总线生命周期（参考解）

> 承接 S06（裸 MMIO + 设备树 + probe）与 S06c（PLIC）。本课做两件事：
> ① 把 S06/S06c 的**裸指针 + `#define` 偏移**升级为 **struct + union 的「寄存器图」**（C 等价于 Rust `tock-registers` 的 `register_structs!`/`register_bitfields!`）；
> ② 补齐 S06 只到 `probe` 为止、缺失的后半生命周期：**driver table → probe → bind → /dev**。

## 0. 三关

1. **类型化寄存器图**（`regmap.h` + `regmap.c`）
   - NS16550：`struct ns16550_regs{ volatile uint8_t thr_rbr,ier,iir_fcr,lcr,mcr,lsr,msr,scr; }` 直接盖在 `0x10000000` 上 - 成员偏移就是寄存器偏移（`reg-shift=0`，字节对齐）；位段用 `union{ uint8_t raw; struct{...:1}b; }` 解码（`lsr_bits`/`mcr_bits`）。回环自测全在命名字段上做。
   - PLIC：64MB 窗口不写成一个大 struct，而是按功能区间拆成三个 typed 子结构：`plic_priority{prio[1024]}`、`plic_enable{word[32]}`、`plic_context{threshold,claim}`，由基址 + 区间偏移 + 上下文号定位。配置后**回读校验**。
   - 两者都过 → `REGMAP_PASS`
2. **平台总线生命周期**（`driver.c`）：`fdt_scan` 得 `(name,compatible,reg)` → `driver_match` 按 compatible 选驱动 → `drv->probe(dev)` 用 ① 的 regmap 自测 → `driver_bind` 把 driver 绑到 device 并 `dev_register` 注册一个 `/dev` 节点。 → `BIND_PASS`
3. **经 /dev 做真实 I/O**（`main.c`）：`dev_lookup("/dev/ttyS0")` 拿回设备、用其 regmap `ns16550_emit` 真发一行。 → `DEV_PASS`

全过 → `ALL_PASS`。

## 1. 文件

| 文件 | 作用 |
|------|------|
| `kernel/regmap.h` | **类型化寄存器图**：NS16550 struct + 位段 union；PLIC 三个 typed 子结构 + `_Static_assert` 锁偏移 |
| `kernel/regmap.c` | 寄存器图上的设备操作：NS16550 回环自测 / 轮询发送；PLIC 配置 + 回读 |
| `kernel/driver.c` | 驱动表 + compatible 匹配 + probe + **bind** + **/dev 注册表** |
| `kernel/fdt.c` | 扁平设备树解析（复用 S06，大端、栈式遍历） |
| `kernel/main.c` | 三关测试驱动，打印各 `*_PASS` |
| `kernel/device.dts` | 设备树源（`plic@c000000` + `uart@10000000`）；`dtc` 编成 `device.dtb` |
| `kernel/dtb_blob.S` | `.incbin "device.dtb"` 把 dtb 嵌进内核 |

## 2. 跑

```
labctl run proper/S06d-regmap
make -C kernel run     # 手动跑（OpenSBI banner 后见内核输出）
```

OpenSBI banner 之后应见：`REGMAP_PASS` / `BIND_PASS` / `DEV_PASS` / `ALL_PASS`，其间还有一行经 `/dev/ttyS0` 直发的 `hello via bound NS16550 regmap`，内核随后 `k_shutdown` 退出。判据：`expect=["REGMAP_PASS","DEV_PASS"]`，`forbid=["FAIL","panic","UNEXPECTED"]`。

## 3. 要点

- **寄存器图 = 硬件布局的语法皮**：`struct` 成员偏移 / `union` 位段 = Verilog 的 `wire` 切片 = `tock-registers` 的 `OFFSET(n) NUMBITS(w)`。三者是同一张表的不同语言皮（见 16b 的四语对位）。
- **`volatile` 仍是地基**：寄存器图只换了「怎么写」，没换「访存语义」 - 每个 `volatile uint8_t` 成员都强制真实访存，否则轮询 `LSR.THRE` 会被优化成死循环。类型化没有也不该绕开 `volatile`。
- **大窗口设备的建模**：PLIC 这种稀疏大窗口（priority/enable/threshold 相隔数 MB）不适合单 struct，按区间拆 typed 子结构 + 索引定位，正对应 `register_structs!` 里用大间隔偏移分段的写法。
- **平台总线解耦**：device（compatible 字符串）与 driver（probe 函数）经字符串匹配解耦；bind 把二者钉在一起并对外暴露一个 `/dev` 名字 - 这正是 Linux platform bus + devtmpfs 的最小骨架。

## 4. 引申

- `probe` 失败的设备应回收（本实验 `probe` 只读不分配，无需回收）；真实内核还有 `remove`/`shutdown`、引用计数、热插拔。
- `/dev` 这里是个静态数组；接 S07 可把它接到 VFS，`open("/dev/ttyS0")` 经 file_operations 落到 `ns16550_emit`。
- Rust 侧把同一张表写成 `register_bitfields!`/`register_structs!`，`probe` 逻辑一字不改即可复用 - 见 `essay/THINKING.md` 的 improper/16 ↔ proper 对标与完整 tock-registers 代码。
