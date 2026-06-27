# 16 · 驱动入门：裸机 MMIO → 设备树匹配 → 可插拔注册 → 平台总线

> 不正经赛道 · 第 16 课 —— 软件 host 直接跑；硬件走 iverilog/bsc 仿真。
> 一句话母题：**驱动是硬件与 OS 之间可插拔的适配器；设备树把“硬件描述”从内核源码里解耦；
> “加一个驱动 = 加一个标注”是现代内核可插拔性的本质。**

## 0. 这节课在讲什么

上一课（引导入门）你学会“先把寄存器置好，设备才肯干活”。这一课你正式当一回驱动工匠，
四步递进：

1. **16.1 裸机 MMIO 手工艺**：对一个固定地址的设备手敲寄存器——读 `ID` 探测、写 `CTRL` 使能、
   轮询 `STATUS.ready`、读写 `DATA` 收发。设备用软件寄存器模型建模（硬件变体则反过来由你写设备）。
2. **16.2 设备树**：自编 `board.dts` → `dtc` 编成 `dtb` → 大端遍历解析 → 拿每个节点的
   `compatible` 在驱动表里**精确字符匹配**，命中 `probe`、未知跳过。
3. **16.3 driver derive**：给第三个驱动**加一行注册标注**，框架遍历“登记集合”即自动发现它——
   框架代码一行不改。
4. **16.4 平台总线**：枚举 dtb 设备 → match → `bind`（调 probe 建实例）→ 登记 `/dev/<name>`；
   用户态 `open/read/write` 只转发到设备 FileLike，不直接碰 MMIO。

对应真实系统：xv6 `kernel/uart.c`/`virtio_disk.c`、rcore 的 console/virtio-blk MMIO 驱动、
Linux 的 `of_match_table`/`platform_match()`/`module_platform_driver()`/`__initcall`。

## 1. MMIO 寄存器契约（16.1）

| 偏移 | 名字 | 方向 | 含义 |
| :-- | :-- | :-- | :-- |
| 0x0 | ID | R | 读到 magic `0x426C6E6B`（"Blnk"）才算探到设备 |
| 0x4 | CTRL | W | bit0 = 使能 |
| 0x8 | STATUS | R | bit0 = ready（本简化设备：使能即就绪） |
| 0xC | DATA | R/W | 写（需就绪）= 驱动把字节交给设备；读 = 回显最近写入 |

驱动序列：`probe`(读 ID) → 写 CTRL 使能 → 轮询 STATUS.ready → 突发写 DATA。
握手可二选一实现：`// TODO[a]` 忙等轮询单字节 / `// ELSE[b]` 读可写计数后突发——对外输出一致。
硬件变体里你写**设备侧**：组合读多路器 + 时序写更新；tb 当参考驱动来回握手。

## 2. 设备树与匹配（16.2）

`board.dts` 三个节点：`acme,blink`@0x10001000、`acme,gpio`@0x10002000、
`acme,blink-v2`@0x10003000。本课只解最小 FDT 子集：`magic + BEGIN_NODE/PROP/END_NODE + compatible/reg`。
你可以用 `dtc` 验证你的 dts：

```
dtc -I dts -O dtb -o board.dtb board.dts && fdtdump board.dtb
```

程序内置的 `build_fdt`/`parse_fdt` 用的就是这套真实 FDT 线格式（与 dtc 产物互通）。
**隐藏向量**：打乱节点顺序后匹配结果不变——证明是按名片（`compatible`）匹配而非位置。

## 3. 可插拔注册与平台总线（16.3 / 16.4）

- 16.3：`acme,blink-v2` 在 16.2 时是“未知”被跳过；你给它**加一行注册**后，框架遍历登记集合
  自动发现并 probe 它。Rust 在 `all_drivers()` 追加 `driver!(...)`；C 加一行 `REGISTER_DRIVER`
  （`__attribute__((section("drivers")))` 链接段自发现，仿 Linux `__initcall`）。
- 16.4：bind 后每个设备成为 `/dev/blink0`、`/dev/gpio0`、`/dev/blink20`，用户态读写经 FileLike 转发。

## 4. 判题与完成标准 (DoD)

软件路径按序打印：
`PROBE_PASS` `IO_PASS` `MMIO_PASS`（16.1）· `DTB_PASS` `MATCH_PASS`（16.2）·
`DERIVE_PASS` `PLUG_PASS`（16.3）· `BIND_PASS` `USER_PASS` `BUS_PASS`（16.4）· 最后 `ALL_PASS`。
硬件路径打印 `DEV_PASS` `MMIO_PASS` `ALL_PASS`。统一 `expect = [MMIO_PASS, ALL_PASS]`，
`forbid = [FAIL, panic, ERROR]`。

```
labctl run improper/16-driver     # 跑 sw-rust/sw-c/hw-v/hw-bsv
labctl watch                      # 边改边自动判定
labctl hint improper/16-driver    # 卡住看提示
```

- [ ] 16.1：probe + 握手收发 → `MMIO_PASS`（硬件路径 0 warning）。
- [ ] 16.2：自编 dts 经 dtc 可编出 dtb，解析按 `compatible` 匹配、乱序/未知正确 → `MATCH_PASS`。
- [ ] 16.3：框架零改、仅加一个注册标注即自动发现新驱动 → `DERIVE_PASS`。
- [ ] 16.4：枚举→bind→用户态 open/read/write 一致 → `BUS_PASS` + `USER_PASS`。
- [ ] 软件任一语言全过（必修）；另一语言、两条硬件路径多过计辅助分。
- [ ] `essay/THINKING.md` 说清三组取舍（思考题）。

## 5. 简化取舍（每条都留完整版作引申）

- FDT 只解 `compatible`/`reg`（`#address/size-cells` 固定 1/1）；不做 phandle/中断树/overlay。
- 匹配只做精确字符串相等 + 线性表；不做“最具体优先”多候选（见思考题 2）。
- 设备无 DMA/无中断/轮询握手；不接 PLIC。
- 注册段只收集 `(compatible → probe)`；不做 init level / deferred probe。
- 平台总线单总线、bind 即 probe、无热插拔/无 unbind/无引用计数。
- 用户态只暴露 FileLike `read`/`write`（复用第 09 课）；无 `ioctl`/`mmap`/`poll`。

## 6. 思考题（`essay/THINKING.md` 作答即可）

1. 为什么不把地址写死，而要绕一圈 `dts → dtc → dtb → 解析 → compatible 匹配`？适配 N 块板省下什么？
2. 同名 `compatible`、或一个设备列多候选（从具体到通用）时匹配如何裁决？为何“最具体优先”？举一个会误匹配的例子。
3. “加一个驱动 = 加一个标注”的插件自发现你还在哪见过？相对手写全局表，链接顺序/初始化时机/调试可见性各有什么成本与风险？
