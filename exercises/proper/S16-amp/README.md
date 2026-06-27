# 正经·S16 · AMP（大小核）：非对称多处理

> 承接 S13（多核启动 / 多槽多跳板）与 S5（调度器）。本课把“所有核都一样”的对称假设打破：
> 引入**大核（BIG）/ 小核（LITTLE）** 的 big.LITTLE 拓扑，让调度器按**角色**分派任务，
> 用**亲和（affinity）** 约束哪类任务能上哪类核，再用**迁移（migration）** 做负载均衡。

## 0. 这节课在讲什么

对称多处理（SMP）里每颗核可互换；**非对称多处理（AMP）** 里核有“身份差异”——大核算力强、功耗高，小核省电、适合后台。OS 要做三件事：

1. **拓扑识别 + 角色分派**：知道哪颗 hart 是大核、哪颗是小核，并把跑调度器的核固定在大核上。
2. **任务亲和**：重算力任务（render/physics）钉在大核，后台任务（sensors/logd）钉在小核。
3. **负载均衡迁移**：朴素分派会把同类任务全堆在第一颗同角色核上；迁移在**同角色核之间**搬任务削峰，且不破坏亲和、不丢任务。

单镜像、按 hartid 分支：`hart0` 当大核跑调度器，另一颗 hart 当小核跑后台任务。

## 1. 你要实现的

- **`kernel/main.c` 的 `cpu_role[NCORE]` 角色分派表**（找 `TODO[角色分派表]`）：把 4 颗 hart 映射成大核/小核。
  约定：`hart0` 跑调度器必须是 `ROLE_BIG`；整机至少 1 大核 + 1 小核。
  本机拓扑（big.LITTLE 2+2）：`{ ROLE_BIG, ROLE_BIG, ROLE_LITTLE, ROLE_LITTLE }`。
  模板里现为占位 `{ ROLE_NONE, ... }`：能编译、能跑，但 `ROLE_PASS` / `AFFINITY_PASS` 不会通过。

其余（亲和判定、朴素分派、迁移削峰、负载统计、真·多核 live 演示）均已给定。

## 2. 判据

输出含三段（外加一行真多核 live 证据）：

- `ROLE_PASS`：角色分派表合法——`hart0` 是大核、至少 1 大核 + 1 小核、无未定义角色。
- `AFFINITY_PASS`：
  - 每个任务都落在满足其亲和的核上；
  - 迁移把两类核的峰值负载都降了下来（BIG `108→56`、LITTLE `15→8`）；
  - 迁移前后各类核负载总量守恒（不丢任务、不破坏亲和）。
- `AMP_LIVE`：hart0 经 SBI HSM 真唤醒一颗小核 hart，派给它一个后台任务，
  经直接映射区共享变量 + fence 回收结果（`checksum=500500`）——“按角色分派”的硬件级证据。
- `ALL_PASS`：全部通过。

不得出现 `FAIL` / `panic` / `UNEXPECTED`。

```
cd kernel && make kernel.elf
make run      # OpenSBI banner 后见内核输出；跑完内核 k_shutdown 退出 qemu
```

## 3. 完成标准 (DoD)

`make kernel.elf` 通过；qemu（`-smp 4`）输出含 `ROLE_PASS` / `AFFINITY_PASS` / `ALL_PASS`，qemu 干净退出（exit 0），无禁用串。

## 4. 简化取舍（简化的是学生负担，不是概念完整性）

- **拓扑识别**简化为静态角色表（真实系统读 DT/ACPI 的 `cpu-map` 与算力字段 `capacity-dmips-mhz`）；
  本课用单镜像按 hartid 分支建模大小核，把重心放在“角色→亲和→迁移”这条调度链上。
- **负载/算力**抽象成整数权重；真实 EAS（Energy Aware Scheduling）会算能效曲线、DVFS 频点。
- **迁移**是回合制确定性削峰（可复现），不掺真抢占/运行队列锁；真原子、运行队列迁移与
  IPI 唤醒留作引申。`AMP_LIVE` 用一颗真 hart + 直接映射共享区 + `fence` 给出真多核证据，
  说明“为什么跨核交换信息要放恒等映射区”（只有物理地址是各核共识）。

## 5. 思考题

见 `essay/THINKING.md`（大小核为何要区分角色调度、亲和与迁移的取舍、为何跨核共享区放直接映射段）。
