# F2 · 微内核（microkernel）：内核只剩 IPC + 调度 + 能力

> 形态认知专题 · 第 2 形态 —— host 软件直觉 demo（不是完整内核），Rust / C 双语言。
> 一句话母题：**把 fs、驱动、网络全部赶出内核态。内核退化成一个「邮局 + 门卫」——
> 只负责转发消息（IPC）、校验能力券（capability）、给崩掉的服务挂上「停业」牌。
> 真正干活的 echo / store 都是用户态服务，谁也别想绕过门卫直接摸它们。**

## 0. 这节课在讲什么

宏内核（Linux）把 fs / 驱动 / 调度 / 内存全塞进一个特权地址空间，一个驱动 bug 能拖垮整机。
**微内核**走另一条路：内核只留**最小机制**——IPC、调度、能力管理；其余皆为用户态进程，
彼此隔离，只能经消息 + 能力互访。代价是「过内核」多了一跳，好处是**故障隔离**与**最小可信基（TCB）**。

本 demo 用最朴素的软件模型把这套权衡演出来，对应真实系统：

- **能力表 `Cap{rights,target}`** ↔ seL4 的 CSpace / cap、Zircon 的 handle table + `zx_rights_t`。
- **内核 `kernel_call()`** ↔ seL4 `seL4_Call`、Zircon `zx_channel_call`：先校验后转发。
- **echo / store 用户态服务** ↔ seL4 的 rootserver 派生的服务、Zircon 的 fs/driver host、MINIX 3 的 server。
- **服务崩溃标记下线** ↔ MINIX 3 的 reincarnation server（驱动崩了重启，内核不动）。

## 1. 模型与你要填的 2 个函数

软件在 `sw/rust/src/main.rs` 或 `sw/c/micro.c`。一个最小「内核」夹在中间：

```
用户态: [proc 持 Cap]  --消息+能力券-->  ┌─ 内核 kernel_call ─┐  -->  [echo / store 服务]
                                          │ ① cap_check 校验   │
                                          │ ② dispatch 路由     │
                                          └────────────────────┘
```

| 你填的函数 | 职责 | 判据 |
| :-- | :-- | :-- |
| `cap_check(cap, world)` | 能力校验：空券→DenyNoCap；缺 SEND 权→DenyNoRight；目标下线→DenyOffline；都过→Ok | `CAP_PASS` / `ISOLATE_PASS` |
| `dispatch(target, msg, world)` | 消息分发：按 target 路由到 echo_service / store_service | `MICRO_PASS` |

`kernel_call` 的铁律（给定，勿改）：**永远先 `cap_check` 再 `dispatch`**。被拒绝时根本不进
`dispatch`，所以无能力的访问对服务**零副作用**——这正是 capability 安全的本质。

三段皆过再打印 `ALL_PASS`。失败会打印小写诊断行（如 `cap: 空能力却被放行 ...`），不含 PASS 串。

```
labctl run forms/F2-microkernel      # 跑 rust / c 两条路径
labctl hint forms/F2-microkernel     # 卡住看提示
```

## 2. 三个判据（本质权衡的三个切面）

- **MICRO_PASS（IPC 通了）**：echo 发 `0xABCD` 原样回来；store `PUT` 槽再 `GET` 槽拿回同值。
  —— 证明「消息往返 + 服务路由」工作正常，这是微内核的命脉（一切服务都靠 IPC 调用）。
- **CAP_PASS（不可绕过内核）**：空能力券、缺 `RIGHT_SEND` 的券，统统被内核挡下；
  且被拒的 `PUT` 没改到 `store`。—— 证明用户态拿不到能力就摸不到资源（不像 POSIX 的环境权威）。
- **ISOLATE_PASS（故障隔离）**：把 echo 服务标记崩溃下线，内核**不跟着崩**，只对 echo 返回
  `DenyOffline`；邻居 store 服务**毫发无损**照常往返。—— 这是微内核相对宏内核最大的卖点。

## 3. 完成标准 (DoD)

- [ ] 至少一条变体跑出 `MICRO_PASS` / `CAP_PASS` / `ISOLATE_PASS` / `ALL_PASS`，无任何小写诊断行（必修）。
- [ ] Rust 0 warning（`cargo build`）；C 用 `gcc -Wall -Wextra -O2` 0 warning。
- [ ] 另一条软件路径也过计辅助分（rust + c 都过有奖励）。
- [ ] essay 答出「capability vs POSIX fd」「Tanenbaum–Linus 之争」「seL4 形式化验证 + IPC fastpath」要点。
- [ ] 能口述：为什么微内核「慢」是 1990 年代旧账（IPC fastpath 把同步 IPC 压到几百 cycle）。

## 4. 关键约定（判题用）

- `cap_check` 返回 `Status::Ok`（C 为 `ST_OK`）表示放行，否则返回拒绝理由枚举。判定顺序固定：
  **空券 → 缺权 → 下线 → 放行**（顺序错会让某些用例落到错误的拒绝码）。
- `dispatch` 只在 `cap_check` 放行后被调用，因此 `default` 分支理论上不可达，返回 0 即可。
- 单线程确定性模型：harness 喂固定向量，逐项断言。把「能力 / 隔离」的心智模型留下来，
  免去真并发的非确定性调试。

## 5. 引申（本最小模型 → 更真实的微内核）

本课把"过内核"简化成了一次普通函数调用、能力表简化成定长数组、服务跑在同一线程的确定性向量里——没有真地址空间、没有真上下文切换。按兴趣可沿这些方向深入：

1. **把"内核 call"换成真同步 IPC**：实现 seL4 风格的 `Call`/`ReplyRecv`——真上下文切换、寄存器传 4-word 消息、切地址空间，把本课"零成本的 dispatch"还原成有代价的跨态调用，亲手测出那"一跳"到底多贵。
2. **加 IPC fastpath**：在同步 IPC 上做快路径（跳过完整调度器、寄存器直接传消息、直接切到 receiver），复现思考题 2 里"微内核慢是 1990 年代旧账"——把往返压到几百 cycle / <1μs。
3. **能力表升级成 CSpace**：把 `Cap{rights,target}` 数组扩成 seL4 的多级 CNode + cap 派生树（`mint`/`derive`/`revoke`），实现能力撤销（revoke 整棵子树），对照 Zircon 的 handle table + `zx_rights_t`。
4. **服务进真地址空间 + 真崩溃**：把 echo/store 放进独立进程（`fork`/`mmap`），用真 `SIGSEGV` 模拟服务崩溃，再写一个 MINIX 3 风格的 reincarnation server 重启它而内核不动——把 `DenyOffline` 升级成真正的"崩了再活"。
5. **给 endpoint 加 badge**：让多个客户端共享一个 endpoint，用 badge 区分调用者，体会 seL4 如何在最小机制上承载复杂多客户端服务。
6. **形式化一小步**：对 `cap_check` 的不变量"无能力则资源零副作用"做 model checking（如 TLA+/Kani），呼应 seL4 用 refinement 三层精化证明 9 千行内核。

## 6. 思考题（`essay/THINKING.md` 作答即可通过）

1. capability 与 POSIX `fd` 都是「整数句柄」，本质区别在哪？（不可伪造对象引用 + 自带 rights vs 进程私有下标 + 环境权威）
2. Tanenbaum–Linus 之争里「微内核太慢」的论据是什么？为什么今天看是旧账？（IPC fastpath：跳过调度器、寄存器传消息、< 1μs）
3. seL4 凭什么能把约 9 千行 C 内核完全形式化证明？微内核的「最小 TCB」与可证明性是什么关系？（refinement 三层精化、TCB 越小证明越可行）
