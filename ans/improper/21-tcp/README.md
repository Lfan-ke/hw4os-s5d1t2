# 21 · TCP 连接状态机：三次握手、四次挥手与 seq/ack 推进

> 不正经赛道 · 第 21 课 —— 纯软件状态机 + mock 通道，host 直接跑（rust / c 双语言）。
> 一句话母题：**UDP 只是「把一包字节扔出去」；TCP 在不可靠数据报之上，靠一台状态机
> 和几个序号字段，凭空造出「一条有始有终、字节不丢不乱的连接」。** 本课就把这台状态机
> 亲手走一遍——这正是 `proper/S11` 我们只做了 UDP 的**另一半**。

## 0. 这节课在讲什么

网络栈到了传输层有两条路：

- **UDP**：无连接。`sendto` 一包就走，不管对方在不在、收没收到、顺序乱不乱。简单、快、无状态。
- **TCP**：面向连接、可靠、有序。它在 UDP 那样的「尽力而为数据报」之上，用一套**状态机**
  和 **seq/ack 序号**，把不可靠的底层包装成一条「字节流管道」。

「连接」这东西在物理上并不存在——网线里跑的还是一个个独立的包。所谓连接，**就是两端
状态机里那点共同维护的状态**（我到哪了、你确认到哪了）。本课用一条 mock 通道在两个端点
之间传「报文」结构，把这台状态机的三件大事走通：

```
三次握手   CLOSED → SYN_SENT → ESTABLISHED            建链（交换初始序号）
数据传输   发 N 字节，对端 ack = seq + len             seq/ack 推进
四次挥手   ESTABLISHED → FIN_WAIT_* → TIME_WAIT → CLOSED 拆链（各自半关闭）
```

## 1. 报文与序号约定

一个报文（`Seg`）= 三个标志位 + 三个序号字段：

| 字段 | 含义 |
| :-- | :-- |
| `flags` | `SYN`(建链)`/ACK`(确认)`/FIN`(拆链)，可叠加（如 `SYN\|ACK`） |
| `seq` | 本段第一个字节的序号 |
| `ack` | 期望对端下一个字节的序号（= 我已连续收到的序号） |
| `len` | 载荷字节数 |

**核心规则**：`SYN` 与 `FIN` **各占 1 个「虚拟序号」**（它们不带数据却要被可靠确认）；
数据段占 `len` 个；**纯 ACK 不占序号**。每端维护两个游标：

- `snd_nxt`：我要发的下一个序号（发出 k 个序号就 `+= k`）。
- `rcv_nxt`：我期望收到的下一个序号（= 我回出去的 `ack`）。收到 `[seq, seq+len)` 后
  `rcv_nxt = seq + len`。

本课把初始序号固定成 `ISS_A=1000`（客户端）、`ISS_B=5000`（服务器），方便逐字段对拍。
（真实 TCP 用**随机** ISN，见 essay：防旧连接、防盲注。）

## 2. 三大流程（看清「连接」就是状态 + 序号）

**三次握手**（为什么是三次见 essay）：

```
A: CLOSED --active_open--> 发 SYN(seq=1000) --> SYN_SENT          snd_nxt=1001
B: LISTEN  收 SYN          回 SYN|ACK(seq=5000,ack=1001) -> SYN_RCVD  snd_nxt=5001, rcv_nxt=1001
A: SYN_SENT 收 SYN|ACK     回 ACK(seq=1001,ack=5001) -> ESTABLISHED    rcv_nxt=5001
B: SYN_RCVD 收 ACK         -> ESTABLISHED
```

**数据传输**（seq/ack 推进）：A 发 10 字节 `seq=1001,len=10`，`snd_nxt→1011`；
B 收下 `rcv_nxt=1001+10=1011`，回 `ACK(ack=1011)`——`ack` 恰为 `seq+len`，意思是
「到 1011 之前我都连续收到了」。

**四次挥手**（各自半关闭，本课给定）：A 发 `FIN`→`FIN_WAIT_1`；B 回 `ACK`→`CLOSE_WAIT`；
A 收 `ACK`→`FIN_WAIT_2`；B 再发 `FIN`→`LAST_ACK`；A 回 `ACK`→`TIME_WAIT`，2MSL 定时器
到期才 `CLOSED`（为什么有 `TIME_WAIT` 见 essay）。

## 3. 你要填的三处 `// TODO`

软件在 `sw/rust/src/main.rs` 或 `sw/c/tcp.c` 的 `recv`（状态机核心）里。被动/主动关闭、
挥手各态、mock 通道、harness 都已给定，**只动握手与数据推进这三处**：

| TODO | 位置 | 要做什么 | 判据 |
| :-- | :-- | :-- | :-- |
| ① | `LISTEN` 收纯 `SYN` | `rcv_nxt=seq+1`；回 `SYN\|ACK(seq=iss,ack=rcv_nxt)`；`snd_nxt=iss+1`；→ `SYN_RCVD` | `HANDSHAKE_PASS` |
| ② | `SYN_SENT` 收 `SYN\|ACK` | `rcv_nxt=seq+1`；回 `ACK(seq=snd_nxt,ack=rcv_nxt)`；→ `ESTABLISHED` | `HANDSHAKE_PASS` |
| ③ | `ESTABLISHED` 收数据(`len>0`) | `rcv_nxt=seq+len`；回纯 `ACK(seq=snd_nxt,ack=rcv_nxt)` | `DATA_PASS` |

四项判据：`HANDSHAKE_PASS`（两端 ESTABLISHED + seq/ack 对）、`DATA_PASS`（seq/ack 推进对）、
`TEARDOWN_PASS`（两端回 CLOSED）、`STATE_PASS`（非法转移被拒 + 状态序对）。全过 `ALL_PASS`。

失败诊断不含 `FAIL`：用 `*_MISS`（该回的报文没回，多半是 TODO 没填）/ `*_BAD`（字段或状态错），
如 `HANDSHAKE_MISS B 收到 SYN 没回 SYN+ACK` 或 `DATA_BAD B 的 ACK 错: ack=.. 应 ack=..（=seq+len）`。

```
labctl run improper/21-tcp     # 跑 rust/c 两条路径
labctl watch                   # 边改边自动判定
labctl hint improper/21-tcp    # 卡住看提示
```

## 4. 关键约定（判题用）

- **SYN/FIN 各占 1 序号，纯 ACK 占 0**：握手后 `snd_nxt = iss + 1`；挥手后再 `+ 1`。
- **`ack = seq + len`**：收方回的 `ack` 永远是「我连续收到的下一个序号」。这是 seq/ack 的灵魂。
- **`recv` 返回要回发的报文**；`present=false`（C 里 `present=0`）表示「这一步无需回复」
  （如握手第三步的 ACK 之后、收到纯 ACK 之后）。
- **非法转移一律拒绝**：落到状态机末尾的 catch-all → `last_accepted=false`、`rejected++`、不改状态。
  `STATE_PASS` 就是在验证「CLOSED 收数据该拒」「ESTABLISHED 收 SYN 该拒」等。
- **`STATE_PASS` 的 (a)~(d) 不依赖你的 TODO**（拒绝逻辑已给定），所以哪怕握手没填，它也能单独过——
  这正好帮你区分「是状态机框架坏了，还是握手没写」。

## 5. 完成标准 (DoD)

- [ ] 至少一条变体跑出 `HANDSHAKE_PASS`/`DATA_PASS`/`TEARDOWN_PASS`/`STATE_PASS`/`ALL_PASS`，无 `*_BAD`（必修）。
- [ ] rust 与 c 两路对同一序号约定行为一致（跨语言都过计辅助分）。
- [ ] 能口述三次握手/四次挥手的状态转移，以及「连接 = 两端状态机里维护的序号状态」。
- [ ] 能解释 `seq/ack` 怎么推进、为什么 `ack = seq + len`。
- [ ] essay 答出三次握手防旧连接、`TIME_WAIT` 的意义、滑动窗口/拥塞控制（粗讲）、TCP vs UDP、对照 `proper/S11`。

## 6. 思考题（`essay/THINKING.md` 作答即可通过）

1. 为什么握手是**三次**而不是两次？（防「旧连接的延迟 SYN」误建连）
2. 为什么主动关闭方要停在 `TIME_WAIT` 等 2MSL，而不是立刻 `CLOSED`？
3. 粗讲**滑动窗口**与**拥塞控制**：它们各自解决什么问题？（流量控制 vs 网络拥塞）
4. TCP vs UDP：各自的取舍？什么场景该用谁？
5. 对照 `proper/S11`（我们只做了 UDP）：要把它升级成 TCP，至少得补哪些机制？
