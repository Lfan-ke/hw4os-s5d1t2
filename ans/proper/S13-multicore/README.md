# 正经·S13 · 多核启动（SBI HSM 唤醒 + 直接映射区握手）

> 前面所有阶段都跑在**单核**（hart0）上：OpenSBI 默认只把引导核引入内核，其余 hart 停在
> HSM `STOPPED` 态等命令。本课让 hart0 主动**唤醒 hart1**，两核经一块共享内存「握手」，
> 第一次出现「多核」。

## 0. 这节课在讲什么

`qemu -smp 4` 给了 4 个 hart，但默认只有引导核（hart0）进内核。其它 hart 由 SBI 的
**HSM（Hart State Management）扩展**管理，停在 `STOPPED` 态。要让它们干活，引导核得调
`sbi_hart_start` 把目标 hart「点亮」，告诉它**从哪条指令、带什么栈**开始跑。

三件事：

1. **唤醒**：引导核调 `sbi_hart_start(target, hart1_entry, 0)`——`target` 是一个确定不同于引导核的 hart（由给定代码从引导 hartid 算出；`-smp` 下引导核可能非 hart0，故不写死）。SBI 让该 hart 以
   **S 态、satp=0** 跳到 `hart1_entry`，入口处 `a0=hartid`、`a1=opaque`。
2. **各自的栈**：hart1 **不能**用 hart0 的 `boot_stack`（会互踩）。`hart1.S` 已为它备好独立
   `hart1_stack`，入口先 `la sp, hart1_stack_top` 再进 C。
3. **握手**：两核经一块**共享邮箱** `g_mbox` 通信——hart1 报上线、做一次**槽位交换**，
   hart0 轮询并核对。

判据：`SMP_BOOT_PASS`（hart1 上线）/ `SLOT_PASS`（槽位交换正确）/ `ALL_PASS`。

## 1. 为什么共享槽位必须放「直接映射区」（物理地址），不能放虚拟地址

这是本课的核心。两个 hart 要对「同一块内存」达成共识，前提是**它们用同一个地址访问到同一个物理单元**。

- 本阶段**没有建页表**，每个 hart 的 `satp=0`（裸机/直接映射）：**虚拟地址 == 物理地址**。
  hart0 写 `&g_mbox`、hart1 读 `&g_mbox`，编译期就是同一个数（内核 .bss 里的地址，
  落在 `0x8020_xxxx` 物理 RAM），两核访问的就是**同一物理单元**——天然共识。
- 若改用「虚拟地址」共享，必须先给**两个 hart 各自建一套页表**，把那个虚拟地址都映射到
  **同一物理页**，并保证 `satp`/TLB 一致。本阶段连页表都没有，谈「虚拟地址共享」没有意义。
- 一句话：**物理地址是各 hart 唯一天然共识的坐标系**。多核共享数据先落在直接映射区，
  等以后引入分页，再用「共享同一物理页 + 各页表映射」把它抬到虚拟地址。

> 共识只是第一步。还要**可见性与顺序**：用 `volatile` 防编译器把轮询优化掉，用
> `fence rw,rw`（`smp_fence`）保证「先写数据、后举旗」的次序对另一核成立——
> 否则 hart0 可能先看到 `slot_done=1` 却还没看到交换后的 `slot[]`。

## 2. 你要实现的（`kernel/main.c`）

参考解已给全。学生版需补两处 `// TODO`：

```
A) kmain 里「唤醒 hart1」：
   long r = sbi_hart_start(target, (uint64_t)hart1_entry, 0);    // target 已由给定代码算出；EID 0x48534D, fid 0

B) hart1_main 里「hart1 入口逻辑」：
   g_mbox.hart_id = hartid; smp_fence(); g_mbox.online = 1;       // 报上线
   smp_fence();
   a=slot[0]; b=slot[1]; slot[0]=b; slot[1]=a;                    // 交换两槽位
   smp_fence(); g_mbox.slot_done = 1;                             // 举完成旗
```

`hart1.S`（裸入口设栈）、`smp.h`（邮箱结构 / `sbi_hart_start` / `smp_fence` / `wait_flag`）已给。

```
labctl run proper/S13-multicore
make -C kernel run     # 手动跑（OpenSBI banner 后见内核输出；用 -smp 4）
```

> 注意 `qemu` 输出含 NUL 字节，自验证时**重定向到文件再 `grep -a`**，别直接管道 `grep`。

## 3. 完成标准 (DoD)

- [ ] hart0 调 `sbi_hart_start` 返回 0，hart1 从 `hart1_entry` 起跑、写 `online=1`：`SMP_BOOT_PASS`。
- [ ] hart1 交换 `slot[0]/slot[1]`，hart0 核对得 `0x5a5a / 0xa5a5`：`SLOT_PASS`。
- [ ] 打印 `ALL_PASS`，qemu 正常关机（exit 0）。
- [ ] 能说清：为什么共享槽位放**直接映射区（物理地址）**而非虚拟地址；fence/volatile 各保证什么。

## 4. 引申

- **多核引导风格**：另一种是「所有 hart 都进 `_start`，按 `mhartid`/`a0` 分流，非引导核自旋等
  唤醒标志」（xv6/部分 rcore）。本课用 **HSM 主动 start**，更贴近 SBI 规范、按需上电。
- **关核**：hart1 干完可 `sbi_hart_stop` 回 `STOPPED`；本课简化为停在 `wfi`，由 hart0 关机统一退出。
- **真并发**：多核同写共享数据需**原子指令**（`amoadd`/`lr/sc`）或锁；本课握手是「单写者+举旗」
  的无锁模式，避开了竞争。接 S5 调度器即可做**每核就绪队列 / 负载均衡**。
- **IPI**：核间中断（SBI `send_ipi` / S 态软中断）用于唤醒/通知，是 SMP 调度的另一半。
