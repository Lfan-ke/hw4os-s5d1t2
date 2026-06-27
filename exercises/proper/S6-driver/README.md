# 正经·S6 · 裸机 MMIO 驱动 + 设备树

> 承接 S2（已能进 S 态、处理 trap）。本课不靠 SBI 代劳，而是**直接读写外设寄存器**驱动一个串口；再用 `dtc` 把设备树编成 dtb，内核解析它发现设备，最后用 **compatible 字符串**在驱动表里匹配并 `probe`——这正是 Linux「平台总线」的最小骨架。

## 0. 这节课在讲什么

三关，对应现代驱动模型的三层：

1. **MMIO 字符设备**：NS16550 UART 映射在物理地址 `0x10000000`。它的寄存器就是一段特殊内存——`*(volatile uint8_t*)(base+off)` 读写即可。我们用它发字符（和 SBI 控制台并存做对照），并用 MCR 回环位自测收发。→ `UART_PASS`
2. **设备树**：`device.dts` 经 `dtc -I dts -O dtb` 编成 `device.dtb`，由 `dtb_blob.S` 用 `.incbin` 嵌进内核。内核解析 fdt 头与 token 流，找到 `uart@10000000` 节点的 `reg` 基址。→ `DT_PASS`
3. **平台总线**：拿节点的 `compatible`（如 `"ns16550a"`）在驱动表里做字符串匹配，命中就调该驱动的 `probe`。→ `PROBE_PASS`

全过 → `ALL_PASS`。

## 1. 你要实现的

`fdt.c`（设备树解析）与 `main.c`（测试驱动）**已给**，无需改。你只填两处：

### `kernel/uart.c` —— UART 寄存器读写（TODO 1、2）
```
uart_reg_read(base, off)  -> 返回 *(volatile uint8_t*)(base+off)
uart_reg_write(base,off,v)-> *(volatile uint8_t*)(base+off) = v
```
占位实现读恒为 0、写丢弃，于是 LSR 的 THRE/DR 永远读不到 1，UART 自测失败。`uart_putc` / `uart_loopback_selftest` 已给，依赖你这两个函数。

### `kernel/driver.c` —— compatible 匹配（TODO 3）
```
k_strcmp(a,b)-> 相等返回 0（仿 strcmp）
```
占位恒返回 1（永远「不相等」），于是驱动表匹配不到任何设备，`PROBE_PASS` 出不来。

## 2. 跑

```
labctl run proper/S6-driver
make -C kernel run     # 手动跑（OpenSBI banner 后见内核输出）
```

判据：输出含 `UART_PASS` / `DT_PASS` / `PROBE_PASS` / `ALL_PASS`，不出现 `FAIL`/`UNEXPECTED`/`panic`。

> 未实现前：`DT_PASS` 会先出来（fdt 解析已给），但 `UART_PASS`/`PROBE_PASS`/`ALL_PASS` 缺失。

## 3. 完成标准 (DoD)

- [ ] 直接 MMIO 收发字符通过回环自测（`UART_PASS`）。
- [ ] 从 dtb 解析出 uart 的 `reg` 基址 `0x10000000`（`DT_PASS`）。
- [ ] compatible 字符匹配 → `ns16550_probe` 成功（`PROBE_PASS`）。
- [ ] 能说清：为什么寄存器访问要 `volatile`；dtb 为什么用大端；平台总线如何靠 compatible 解耦设备与驱动。

## 4. 引申

- 真正的串口还要配波特率（写 LCR.DLAB=1 后写 DLL/DLM 分频）、开 FIFO（FCR）、用中断而非轮询（IER + plic）。
- 设备树还携带 `interrupts`、`clocks`、子节点；S7 会用 RAM 盘（接口同 virtio-blk MMIO）做块设备读写。
