# 09 · 设备文件：一切皆文件，文件即接口

> 不正经赛道 · 第 9 课 —— 软件 host 直接跑 + 硬件 iverilog/bsc 仿真。
> 一句话母题：**设备文件 = 给硬件的副作用穿上 read/write 的外衣；「一切皆文件」本质是「一切皆 read/write 接口」。**

## 0. 这节课在讲什么

你在「文件管理」里把块设备当成一堆字节排布。现在反过来问：如果一个「文件」读出来恒为 1、写进去石沉大海，它还存了字节吗？没有——它只是把 `read/write` 两个接口，安在了一段「硬件因果」上。本课造两个「不存数据的文件」：

1. **常量设备 ConstDev**：`read()` 恒 1、`write(x)` 恒 0（吞掉、无存储）。像 `/dev/zero` + `/dev/null`。
2. **求和设备 RingSum**：深度 2 的环形寄存器；`write` 推入 / `233` 复位，`read` 求和。一个**有副作用**的文件。

对应真实系统：rcore 的 `File` trait（`Stdin/Stdout/Pipe/Inode` 都实现它）、xv6 的 `struct devsw{read,write}`、Linux 的 `file_operations`、`/dev/zero`、`/dev/null`、MMIO `readl/writel`、环形缓冲 `kfifo`。

## 1. 数据模型

```
trait FileLike { fn read(&mut self) -> u32; fn write(&mut self, x: u32) -> u32; }

ConstDev   : read()=1            write(x)=0
RingSumDev : read()=r0+r1        write(x)= if x==233 {r0=r1=0} else {r1=r0; r0=x}; 0
```

寄存器宽度 16-bit（够放 666/777/233）。`233` 当 magic 复位。

## 2. 你要填什么（按变体）

| 变体 | 文件 | 填什么 |
| :-- | :-- | :-- |
| `sw-rust` | `sw/rust/src/main.rs` | `ConstDev` / `RingSumDev` 的 `read`/`write` 方法体 |
| `sw-c` | `sw/c/absfile.c` | `const_read/const_write/ring_read/ring_write` 四个函数体 |
| `hw-v` | `hw/v/abstract_dev.v` | `const_read` 赋值 + `always@(posedge clk)` 状态机 |
| `hw-bsv` | `hw/bsv/AbstractDev.bsv` | `mkRingDev` 的 `wr` 方法 + `constRead` 方法 |

分支择一（judge 不关心走哪条）：
- 常量设备：`// TODO[a]` 拆 OneSource+NullSink 两对象 / `// ELSE[b]` 合成一个 ConstDev。
- RingSum 写入：`// TODO[a]` 移位寄存器（`r1<-r0; r0<-x`）/ `// ELSE[b]` head 指针（两槽轮流写，读出和相同）。

## 3. RingSum 真值表（软硬一致）

复位后 r0=r1=0，依次写入并读出：

| write | r1, r0（更新后） | read = r0+r1 | 期望 |
| :-: | :-: | :-: | :-: |
| 666 | 0, 666 | 666 | 666 |
| 111 | 666, 111 | 777 | 777 |
| 222 | 111, 222 | 333 | 333 |
| 233 | 0, 0（复位） | 0 | 0 |

```
labctl run improper/09-abstract-file     # 跑 C/Rust/Verilog/BSV 四条路径
labctl watch                             # 边改边自动判定
labctl hint improper/09-abstract-file    # 卡住看提示
```

## 4. 判据与完成标准 (DoD)

判题靠输出子串：`FILELIKE_PASS`（常量设备 read 三次全 1、write 三次全 0）+ `RING_PASS`（666/111/222/233 → 666/777/333/0）+ 末尾 `ALL_PASS`；`forbid=["FAIL","panic","ERROR"]`。

- [ ] 至少一条功能变体打印 `FILELIKE_PASS` + `RING_PASS` + `ALL_PASS`（必修，`require=1`）。
- [ ] RingSum 对 `666/111/222/233` 依次读出 `666/777/333/0`（覆盖最旧 + magic 复位）。
- [ ] 硬件路径 0 warning（`warn_gate`）；软/硬任选两条对照，输出逐位一致。
- [ ] 能一句话说清「设备文件 = 把 read/write 接口安到硬件副作用上」，并完成 essay。

## 5. 引申（可扩展性：从两个玩具设备到真实 VFS）

本课的 `FileLike` 只有 `read()->u32` / `write(u32)->u32` 两个方法、无参数无错误码、无 `open/close`、无偏移、设备只有常量与深 2 环形求和两种。「一切皆文件」的真实威力远不止于此，按兴趣选方向深入：

1. **把接口做成真正的 `file_operations`**：给 `read/write` 加 `buf: &mut [u8]` + `len` + 返回「实际读写字节数」，再补 `open/close/seek/ioctl/poll`。对照 Linux `struct file_operations`、xv6 `struct devsw{read,write}`、rcore `File` trait（`Stdin/Stdout/Pipe/Inode` 全实现它），体会「同一组函数指针，不同设备各填一份」就是多态的本质。
2. **再造几个经典字符设备**：`ConstDev` 的 read 恒 1 / write 恒 0 已经覆盖 `/dev/zero`+`/dev/null`；补 `/dev/full`（write 恒返回 `ENOSPC`）、`/dev/random`（带状态的 PRNG）、`/dev/tty`（回显），把「设备 = 安在硬件因果上的 read/write」铺开成一个小 `/dev`。
3. **RingSum 升级为真 ring buffer / kfifo**：当前深度写死 2、读出是「求和」。改成可配深度、`read` 真正弹出最旧元素、写满返回背压（满/空指针 + 计数），就是 Linux **kfifo** / 单生产者单消费者无锁环——再接上 07 课的信号量做阻塞 read/write。
4. **挂进一个最小 VFS / 设备号路由**：实现 `open("/dev/xxx")` → 按 major/minor 设备号查表得到对应 `FileLike`，让用户代码只认路径不认设备，体会 VFS 的「路径名 → inode → 操作表」三级间接，以及 `mknod`/devtmpfs 怎么把设备挂上文件树。
5. **设备文件 ↔ 真 MMIO**：把 `RingSumDev` 的寄存器换成约定 MMIO 地址，`read/write` 编译成 `readl/writel`（volatile，不可被优化/重排），体会「设备文件」与「裸 MMIO 寄存器」其实是同一段硬件因果的两层皮，呼应第 01 课「软件能做的硬件也能做」（思考题 3）。
6. **硬件路径补握手**：当前硬件设备是即时读写；给 RingSum 配 valid-ready 握手与忙状态，做成可被 07 课 IPC 协议驱动的时序设备，体会「设备 done 位」如何成为软硬件的接缝。

## 6. 思考题（essay，`essay/THINKING.md` 作答即过）

1. **《三体》人列计算机**：从「稳定可复现的因果 A→B 当作 0/1」角度，它和你写的硬件设备本质相同/不同在哪？单兵看错旗 = 什么硬件故障？传令延迟 = 什么？谁当「石英晶振」打拍子？
2. **红石与晶振**：一台「计算机」最少需要哪几类稳定物理因果（开关、放大、存储、时钟）？水力/气压/电力/人力/磁力/铜线光纤各举一种，说它充当哪一类。
3. **一切皆文件**：你写的 read 恒 1 / write 恒 0 分别对应 `/dev/zero`、`/dev/null`、`/dev/full` 的哪个？为什么「一切皆文件」是强抽象？把它和 MMIO 设备、第 01 课「软件能做的硬件也能做」串起来谈一段。
