# 17 · 板级入门：BSP 与设备树——让同一个 OS 跑遍多块板子

> 不正经赛道 · 第 17 课 —— 软件 host 直接跑（内存数组模拟 MMIO 总线）；硬件选做地址译码模型。
> 一句话母题：**BSP = 把"散落的硬编码板级常量"收敛成一层可替换的胶水；
> 设备树是 firmware↔OS 的稳定 ABI——平面图不变，店长（OS）随便换。**

## 0. 这节课在讲什么

同一个店长（OS 内核）空降到不同门店（开发板），靠的不是背死每家店的电闸、收银台各在哪面墙
（那叫硬编码，换家店就抓瞎），而是进门先看墙上那张《门店平面图》（设备树 DTB）。
BSP 就是把"这家店长这样、设备在哪、时钟多快"翻译给通用店长听的那层人。妙处在于：
换个新店长（OS 升级）只要平面图没变，照样当天开门营业——这就是"设备树即稳定 ABI"的契约。

对应真实系统：rcore `boards/` 模块与 `config.rs` 里那堆 `UART_BASE=0x1000_0000`/`CLINT=0x0200_4000`
+ `board_init()`；xv6 `kernel/memlayout.h` + `start.c`；Linux 的 `fdt`/`of_match_table`/platform_bus。

## 1. 数据模型

```
struct BoardConfig { uart_base: u32, clk_hz: u32 }   // 板间差异收敛到这一个结构
struct DtNode { compatible: &str, reg_base, reg_size, irq, props: [{name, val}] }  // mini-DT 节点
板 A：uart_base = 0x1000_0000, clk = 10_000_000      // 地址 C
板 B：uart_base = 0x1002_0000, clk = 50_000_000      // 地址 D
```

> 简化：mini-DT 用**扁平节点数组 + 命名属性**代替真实 FDT（大端 / magic=0xd00dfeed / 字符串表 /
> 4 字节对齐 / phandle）；host **内存数组模拟 MMIO 总线**代替真实 qemu-virt 设备模型——概念全保留。

## 2. 你要填的四个函数（`sw/rust/src/main.rs` 或 `sw/c/bsp.c`）

| # | 函数 | 要求 | 判据 |
| :- | :-- | :-- | :-- |
| 1 | `bsp_probe(board_id)` | 硬编码表：board 0→板 A 地址，board 1→板 B 地址 | 同一 kmain 两块板都点亮 → `PROBE_A_PASS` / `PROBE_B_PASS` |
| 2 | `parse_dt(blob)` | 找 `compatible=="vlab,uart"` 节点取 `reg`/`clock-frequency` | 同一 parse_dt+kmain 喂 A/B 两份 blob → `DT_A_PASS` / `DT_B_PASS` |
| 3 | `driver_bind(blob, regs)` | 节点 × 驱动表 `compatible` 字符串匹配，统计绑定数 | 每类绑定 1 个 + 绑定数==节点数 → `BIND_uart_PASS` / `BIND_timer_PASS` / `BIND_PASS` |
| 4 | `parse_dt_v2(blob)` | 向后兼容：`clock-frequency` 缺失给默认；未知属性跳过 | v2 内核吃未改动的老 A.dtb 仍启动 → `UPGRADE_PASS` |

四段全过再打印 `ALL_PASS`。第 1、2 步各有 `// TODO[a] … // ELSE[b] …` 两种写法，择一即可。

> 关键直觉：`kmain` **只认 `cfg.uart_base`**，对"这是哪块板"一无所知。`uart_base` 给错，
> 写就落在任何设备窗口外（`faults>0`），UART 永远收不到 banner → 判 FAIL。这就是"换块板就跑飞"的痛。

## 3. 硬件变体（选做，辅助分）：板级地址译码模型

填 `hw/v/bsp_decode.v`（或 `hw/bsv/BspDecode.bsv`）的核心：

```
sel   = (addr >= BASE) && (addr < BASE + SIZE);   // 设备恰在窗口内应答
rdata = sel ? (MAGIC | (addr - BASE)) : 0;        // 窗口外不应答
```

参数 `BASE` 综合成板 A=0x1000_0000 或板 B=0x1002_0000——这是软件 `bsp_probe` 表的硬件镜像。
tb 校验"窗口内应答、窗口外（含另一块板基址）不应答" → `DECODE_A_PASS` / `DECODE_B_PASS` → `ALL_PASS`。
`always @(*)` 每条分支务必给 `sel`/`rdata` 全赋值（防 latch）且至少读 `addr`（0 warning 闸门）。

```
labctl run improper/17-bsp     # 跑全部变体
labctl watch                   # 边改边自动判定
labctl hint improper/17-bsp    # 卡住看提示
```

## 4. 完成标准 (DoD)

- [ ] 任一软件语言依次打印 `PROBE_*` → `DT_*` → `BIND_*` → `UPGRADE_PASS` → `ALL_PASS`，无 `FAIL`（必修）。
- [ ] 同一个 `kmain` + 同一个 `parse_dt`，在 A.dtb(地址 C) 与 B.dtb(地址 D) 上**零板级专有代码**双双点亮。
- [ ] v2 内核吃**未改动**的 v1 DT 仍成功绑定（向后兼容）。
- [ ]（辅助）硬件译码模型 `DECODE_A/B_PASS` 且 0 warning；另一软件语言也过计辅助分。
- [ ] 能用一句话说清"BSP 在做什么、设备树凭什么让一个 OS 跑遍多板"（思考题）。

## 5. 引申：从 mini-DT 到真实板级生态

本课用扁平节点数组冒充 FDT、用内存数组冒充 MMIO 总线、`bsp_probe` 用硬编码表，只覆盖两块板。想接近真实 BSP/设备树生态，可往这些方向深入：

1. **真 FDT 解析**：把扁平数组换成真实 FDT 二进制（`magic=0xd00dfeed`、大端、结构块 + 字符串表、4 字节对齐、`phandle` 引用），对照 Linux `libfdt`/`of_*` API 自己写一个解析器。
2. **真驱动模型**：把 `compatible` 单串匹配扩成 `of_match_table` 风格的兼容串列表 + 优先级 + 驱动 `probe`/`remove` 生命周期 + deferred probe（依赖设备未就绪时重试）。
3. **中断拓扑**：解析 `interrupt-parent`/`interrupts` 与 `phandle` 引用，构建中断控制器树（PLIC/CLINT），让 `driver_bind` 不仅认地址还认中断号。
4. **多发现机制对照**：在 DT 之外再实现 x86 ACPI 表解析与 PCIe 配置空间枚举两条路径并列比较——板级信息分别存哪、由谁产生、由谁消费。
5. **DT overlay 与热插**：支持 overlay 叠加节点（扩展板/cape/HAT）、运行时增删节点，理解设备树并非只在启动期一次性消费。
6. **BSP 化真实内核**：把 rcore 写死的 `UART=0x1000_0000`/`CLINT=0x0200_4000` 真正抽到 `boards/` + `board_init()`，由 DT 填充 `BoardConfig`，让同一二进制跑遍 qemu-virt 与真实 SoC。

## 6. 思考题（`essay/THINKING.md` 作答即可通过）

1. bootloader + 设备树都不变、只升级 OS，为什么仍能开机？把"设备树"当作 firmware↔OS 的稳定 ABI 论证。
   反过来：新 OS 想用一个 DT 里没描述的新设备，该谁动手——重出 DT/重刷 bootloader，还是 OS 自探测？各自代价？
2. 同一个 kernel 二进制凭什么跑遍 board A/B？把"硬编码地址"换成"读 DT"省了什么、又引入什么成本？
   对照 rcore 写死的 `UART=0x1000_0000`、`CLINT=0x0200_4000`，要"BSP 化 / DT 化"需动哪几处、新增哪个解析步骤？
3. 驱动靠 `compatible` 字符串去认硬件——相比 01-hw-vlan 硬连死的 MMIO 地址，灵活在哪、代价在哪？
   x86 的 ACPI、ARM/RISC-V 的 Device Tree、PCIe 的即插即用枚举，各自把"板级信息"存哪、由谁产生、由谁消费？
