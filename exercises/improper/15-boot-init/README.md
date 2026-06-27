# 15 · 引导入门：启动握手——先置位，后开工（前置程序与链接顺序）

> 不正经赛道 · 第 15 课 —— 软件 host 直接跑 / 硬件仿真同构 / 思考题。
> 一句话母题：**软件是硬件的开机咒语，链接顺序决定谁先念咒。**

## 0. 这节课在讲什么

一块芯片刚上电不是“开箱即用”，而是“半睡半醒”——时钟没起、核心没使能、配置总线还锁着。
你若不先做“晨间唤醒”就直接用它，读回的全是 `0x0BADB007` 这种胡话。本课让你亲手写那段
“在 `main` 之前偷偷跑”的前置程序：往几个 MMIO 寄存器按顺序敲对几个位，把设备从“执行错乱”
哄到 `READY`。

对应真实系统：rcore/xv6 的 `entry.S → _start → rust_main` 启动链与 `linker.ld` 的
`ENTRY`/段顺序；SBI/BootROM 在内核之前替你“置位”基础外设；SoC 的 RCC 时钟使能位、
PLL lock 轮询、看门狗/写保护的 magic-unlock 与 lock 寄存器；ELF 的 `.init_array`/构造器
“先于 `main` 执行”机制。与 01-hw-vlan 一脉相承：**软件敲位 / 硬件解锁，是同一套握手协议的两面。**

## 1. 寄存器图（软硬共用）

| 寄存器 | 偏移 | 说明 |
| :-- | :-- | :-- |
| `UNLOCK` | 0 | 写 `0xB007C0DE`（MAGIC）解锁配置总线 |
| `CLKDIV` | 1 | 时钟分频，合法 `1..15`；写 `0` → BADCLK，设备永不就绪 |
| `CTRL`   | 2 | bit0=`EN` 使能 · bit1=`LE` 锁定使能 |
| `STATUS` | 3 | bit0=`READY` 1=`LOCKED` 2=`BADCLK` 3=`NOTEN` |
| `DATA`   | 4 | 就绪后读回变换值 `raw ^ 0xCAFE`；**未就绪读回 `0x0BADB007`** |

四步握手：`UNLOCK=MAGIC` → `CLKDIV=合法值` → `CTRL=EN|LE` → 轮询 `STATUS.READY`。

## 2. 你要填什么

### 软件（`sw/rust/src/main.rs` 或 `sw/c/boot.c`）

1. **`boot_init()`**（对应 15.2）：四步握手。顺序很重要——解锁必须在使能之前，
   否则使能时设备仍锁着、`READY` 永不置位、忙等轮询会**卡死**。
2. **让 `boot_init` 先于 `app_main` 跑**（对应 15.3）：
   - C：在 `__attribute__((constructor))` 的 `boot_ctor` 里调用 `boot_init(&g_dev)`，
     C 运行时会在 `main` 之前执行它（`.init_array` 机制）。
   - Rust：在 `register_inits()` 里返回 `vec![boot_init]`，给定的 `crt_start` 会先遍历
     这张 `.init_array` 再进 `app_main`。

判题分组：`LOCK_PASS`（坏启动被正确拒）→ `BOOT_PASS`（握手后就绪）→ `USE_PASS`
（DATA 变换正确）→ `ORDER_PASS`（boot 先于 app）→ 全过打 `ALL_PASS`。
抢跑（先用后置位）会触发 `BOOT_FAULT`（判题禁止串）。

### 硬件（`hw/v/boot_gate.v` 或 `hw/bsv/BootGate.bsv`）

实现“解锁/使能译码”的组合逻辑：给定已锁存的配置（`unlocked/clkdiv/en/data_raw`），
按 `addr` 译出 `rdata`——未就绪读 `DATA` 吐 `0x0BADB007`，就绪后给 `data_raw ^ 0xCAFE`；
`STATUS` 按 `READY/LOCKED/BADCLK/NOTEN` 拼位。给定 tb 驱动“先误用 → 再正确握手 →
顺序检查”的规范序列，打同名 `*_PASS` 子串，`warn_gate=true` 强制 0 warning。

```
labctl run improper/15-boot-init     # 跑五条路径（任一过即过；多过计辅助分）
labctl watch                         # 边改边自动判定
labctl hint improper/15-boot-init    # 卡住看提示
```

## 3. 硬件真值表（`rdata`）

```
clkdiv_valid = (clkdiv != 0)                       // 1..15 合法
ready        = unlocked & en & clkdiv_valid

addr==STATUS(3): (ready?READY) | (!unlocked?LOCKED) | (!clkdiv_valid?BADCLK) | (!en?NOTEN)
addr==DATA(4)  : ready ? (data_raw ^ 0xCAFE) : 0x0BADB007
其它           : 0
```

软件 `mmio_read` 与硬件 `boot_gate` 对外逐项一致：同一握手协议，软件忙等轮询、硬件组合译码。

## 4. 完成标准 (DoD)

- [ ] 任一软件变体跑出 `LOCK_PASS`/`BOOT_PASS`/`USE_PASS`/`ORDER_PASS` + `ALL_PASS`，
      全程无 `BOOT_FAULT`（必修）。
- [ ] `boot_init` 确实先于 `app_main` 执行（构造器或 `.init_array` 任一方式）；
      先用后置位被判 `BOOT_FAULT`。
- [ ]（辅助分）`hw-v`/`hw-bsv` 实现解锁门并 0 warning，对外子串与软件逐项一致。
- [ ]（辅助分）`essay/THINKING.md` 说清“为何前置、为何 magic-unlock、CLKDIV=0 为何乱、
      为何链接到 main 之前”。

## 5. 引申：从四步握手到真实启动链

本课把启动压成"敲 4 个 MMIO 寄存器 + 模拟 `.init_array`"，刻意省掉了汇编入口、多级引导与真实时钟序列。想接近真实开机流程，可往这些方向深入：

1. **真 `entry.S` + `linker.ld`**：把 `register_inits`/构造器换成真实 `_start` 汇编（设栈指针、清 `.bss`、跳 `rust_main`），亲手写 `linker.ld` 的 `ENTRY`/段顺序与加载地址（衔接正经赛道 S1 SBI 引导）。
2. **多级引导接力**：把单段握手扩成 BootROM → SBI(OpenSBI/RustSBI) → kernel 的多级接力，按 RISC-V 约定用 `a0=hartid`、`a1=dtb 指针` 传参，理解每一级"置位"到哪、把什么交给下一级。
3. **真时钟/PLL 序列**：把 `CLKDIV` 换成 RCC 时钟使能 + PLL 配置 + lock 轮询，并给轮询加超时与退避——把 `BADCLK` 从"死等"变成可恢复的故障路径。
4. **初始化依赖图**：多个外设彼此有先后依赖时，把线性四步换成拓扑排序的初始化图，对照 Linux 的 `initcall` 分级（early/core/device…）与 deferred probe。
5. **安全启动**：给 `UNLOCK` 的 magic 加签名校验、一次性熔丝(eFuse)、写保护锁存位（`LE`），串起 measured/secure boot 的信任链。
6. **忙等 → 中断**：把轮询 `READY` 换成"就绪中断 + 看门狗超时"，并讨论为何早期启动阶段常被迫忙等（中断控制器/时钟尚未就绪）。

## 6. 思考题

见 `essay/THINKING.md`（15.1 观察坏启动 + 15.5 启动哲学）。作答即通过。
