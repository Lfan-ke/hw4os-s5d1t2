# 正经·S6 · 裸机 MMIO 驱动 + 设备树（参考解）

> 承接 S2（已能进 S 态、处理 trap）。本课不靠 SBI 代劳，而是**直接读写外设寄存器**驱动一个串口；再用 `dtc` 把设备树编成 dtb，内核解析它发现设备，最后用 **compatible 字符串**在驱动表里匹配并 `probe`——这正是 Linux「平台总线」的最小骨架。

## 0. 三关

1. **MMIO 字符设备**（`uart.c`）：NS16550 UART 在物理地址 `0x10000000`，寄存器即 `volatile` 内存。`uart_putc` 轮询 `LSR.THRE` 后写 `THR`；`uart_loopback_selftest` 置 `MCR.LOOP` 后发字节、从 `RBR` 读回比对，自测收发。→ `UART_PASS`
2. **设备树**（`device.dts` + `fdt.c`）：`dtc` 编成 `device.dtb`，`dtb_blob.S` 用 `.incbin` 嵌入内核 `.rodata`。`fdt_scan` 解析大端 token 流，收集带 `reg`/`compatible` 的设备节点；从 `uart@10000000` 取 `reg` 首地址。→ `DT_PASS`
3. **平台总线**（`driver.c`）：驱动表 `{compatible, probe}`，用 `k_strcmp` 命中 `"ns16550a"` 即调 `ns16550_probe`（对 dtb 给出的 base 再做回环自测，闭环证明「设备树→驱动→真实设备」打通）。→ `PROBE_PASS`

全过 → `ALL_PASS`。

## 1. 文件

| 文件 | 作用 |
|------|------|
| `kernel/uart.c` | NS16550 MMIO 驱动：寄存器读写、putc/puts、回环自测 |
| `kernel/fdt.c` | 扁平设备树解析（大端、栈式遍历节点） |
| `kernel/driver.c` | 驱动表 + compatible 匹配 + probe |
| `kernel/main.c` | 三关测试驱动，打印各 `*_PASS` |
| `kernel/device.dts` | 设备树源；`dtc` 编成 `device.dtb` |
| `kernel/dtb_blob.S` | `.incbin "device.dtb"` 把 dtb 嵌进内核 |
| `kernel/Makefile` | 加了 `device.dtb` 生成规则与依赖 |

## 2. 跑

```
make -C kernel run
make -C kernel kernel.elf && \
  qemu-system-riscv64 -machine virt -nographic -bios default -kernel kernel/kernel.elf
```

OpenSBI banner 之后应见：`UART_PASS` / `DT_PASS` / `PROBE_PASS` / `ALL_PASS`，内核随后 `k_shutdown` 退出。

## 3. 要点

- **`volatile`**：外设寄存器随硬件状态变化，必须 `volatile` 否则编译器会把轮询优化成死循环或丢写。
- **大端 dtb**：设备树二进制全用大端整数，逐字节读取规避对齐问题。
- **平台总线解耦**：设备（compatible 字符串）与驱动（probe 函数）通过字符串匹配解耦——新增设备无需改驱动、新增驱动无需改设备表。

## 4. 引申

- 配波特率（LCR.DLAB→DLL/DLM）、开 FIFO（FCR）、中断驱动（IER + PLIC）。
- S7：RAM 盘（接口同 virtio-blk MMIO）块设备读写；S6c：更完整的 fdt + 平台总线。
