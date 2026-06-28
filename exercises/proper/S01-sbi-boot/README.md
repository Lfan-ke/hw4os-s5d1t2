# 正经·S01 · 传统引导与 SBI

> 正经赛道第一课。前置：理解 rcore 的启动。环境：`qemu-virt`（真裸机 + OpenSBI）。

## 0. 这节课在讲什么

QEMU 用 **OpenSBI** 作为 M 态固件先启动，初始化基本外设后，跳入你的 S 态内核（链接在 `0x80200000`）。内核要做事（打印、关机）得通过 **SBI 调用**：执行 `ecall` 从 S 态陷入 M 态，请固件代劳。

启动链：`QEMU → OpenSBI(M 态) → 你的内核 _start(S 态) → kmain → k_shutdown`。

## 1. 你要实现的（`kernel/sbi.c`）

只填一个函数 `sbi_call`（内联汇编版 SBI 调用）：

```
eid → a7,  fid → a6,  参数 → a0/a1/a2,  执行 ecall,  返回值在 a0
```

填对后 `console_putchar`（legacy EID=1）与 `k_shutdown`（SRST）自动可用。`kmain`（给定）会用 SBI 打印自检串，再返回让 `entry.S` 调 `k_shutdown` 关机，qemu 退出。

```
labctl run proper/S01-sbi-boot      # make kernel.elf + qemu-system-riscv64 跑
labctl hint proper/S01-sbi-boot
make -C kernel run                 # 手动跑（看 OpenSBI banner + 内核输出）
```

判据：输出含 `SBI_BOOT` / `PUTCHAR_PASS` / `ALL_PASS`。

## 2. 完成标准 (DoD)

- [ ] 内核经 OpenSBI 引导进入 S 态，`sbi_call` 正确。
- [ ] SBI console putchar 打印自检串；SRST 关机使 qemu 退出。
- [ ] 能说清 SBI 是什么、`ecall` 在特权级间做了什么。

## 3. 引申

- 自制 mini-SBI（M 态固件，`-bios none`）；timer SBI 扩展驱动时钟中断（见 S02+）。
