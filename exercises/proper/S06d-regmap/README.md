# 正经·S06d · 类型化寄存器图 + 平台总线生命周期

> 承接 S06（裸 MMIO + 设备树 + probe）与 S06c（PLIC）。本课做两件事：
> ① 把 S06/S06c 的**裸指针 + `#define` 偏移**升级为 **struct + union 的「寄存器图」**（C 等价于 Rust `tock-registers` 的 `register_structs!`/`register_bitfields!`）；
> ② 补齐 S06 只到 `probe` 为止、缺失的后半生命周期：**driver table → probe → bind → /dev**。

## 0. 这节课在讲什么

三关：

1. **类型化寄存器图**：`regmap.h` 已把布局写成类型 - NS16550 是 `struct ns16550_regs{ volatile uint8_t thr_rbr,ier,iir_fcr,lcr,mcr,lsr,msr,scr; }`（成员偏移=寄存器偏移），位段是 `lsr_bits`/`mcr_bits` 这种 `union{ uint8_t raw; struct{...:1}b; }`；PLIC 是三个 typed 子结构 `plic_priority`/`plic_enable`/`plic_context`。你在**命名字段**上做读写，不再 `base+off`。 → `REGMAP_PASS`
2. **平台总线生命周期**：`fdt_scan → driver_match → probe → bind → dev_register`。驱动表 / 匹配 / probe / 注册表都已给，你补 `bind`。 → `BIND_PASS`
3. **经 /dev 做真实 I/O**：`dev_lookup("/dev/ttyS0")` 拿回设备、用其 regmap 真发一行。 → `DEV_PASS`

全过 → `ALL_PASS`。

## 1. 你要实现的（共 3 处 TODO）

`regmap.h` / `dev.h` / `fdt.c` / `main.c` / `device.dts` **已给**，无需改。只填两文件：

### `kernel/regmap.c` - 在类型化寄存器图上做设备操作

- **TODO(1) `ns16550_loopback`**：经 typed regmap 完成回环自测 - `mcr_bits` 置 `.b.loop=1` 写 `u->mcr`，对 `"RV64"` 逐字节轮询 `(lsr_bits){.raw=u->lsr}.b.thre` 后写 `u->thr_rbr`，轮询 `.b.dr` 后读回比对，最后恢复 `u->mcr`。
- **TODO(2) `plic_regmap_config`**：经三个 typed 子结构配置 `plic_prio()->prio[irq]=1` / `plic_enable(ctx)->word[irq/32] |= 1<<(irq%32)` / `plic_context(ctx)->threshold=0`，再**回读校验**。

### `kernel/driver.c` - 平台总线的 bind

- **TODO(3) `driver_bind`**：`dev->drv` 为空返回 0；否则置 `dev->bound=1`，用 `class_to_node(dev->drv->class)` 取 `/dev` 路径调 `dev_register`，注册成功（≥0）返回 1。

占位实现下：`ns16550_loopback`/`plic_regmap_config` 恒返回 0 → `REGMAP_MISS`，且 probe 失败；`driver_bind` 恒返回 0 → `BIND_MISS`/`DEV_MISS`。

## 2. 跑

```
labctl run proper/S06d-regmap
make -C kernel run     # 手动跑（OpenSBI banner 后见内核输出）
```

判据：输出含 `REGMAP_PASS` 与 `DEV_PASS`（其间还应见 `BIND_PASS` 与经 `/dev/ttyS0` 直发的一行），不出现 `FAIL`/`UNEXPECTED`/`panic`。

> 未实现前：`dt nodes = 3` 会先出来（fdt 已给），但三个 `*_PASS` 都缺、并打印对应 `*_MISS`。

## 3. 完成标准 (DoD)

- [ ] NS16550 经类型化 regmap 回环自测一致、PLIC 经 typed 子结构配置回读一致（`REGMAP_PASS`）。
- [ ] `ns16550a` 经 compatible 匹配 → probe → bind → 注册 `/dev/ttyS0`（`BIND_PASS`）。
- [ ] `dev_lookup("/dev/ttyS0")` 拿回设备、经其 regmap 真发一行（`DEV_PASS`）。
- [ ] 能说清：类型化寄存器图为何仍要 `volatile`；PLIC 大窗口为何按区间拆 typed 子结构；improper/16 的 probe 与本实验的 probe 是什么关系。

## 4. 引申

- 把 `/dev` 接到 VFS：`open("/dev/ttyS0")` 经 file_operations 落到 `ns16550_emit`（接 S07）。
- Rust 侧把同一张表写成 `register_bitfields!`/`register_structs!`，probe 逻辑一字不改即可复用 - 见 `essay/THINKING.md`。
