# 正经·S15 · SMP 安全与读写锁

> 承接 S13（多核启动 / 多槽多跳板）。S13 把多个 hart 拉起来、在直接映射区交换信息；
> 本课让这些核**真并行**地抢同一块共享内存，于是第一次必须面对：单核"关中断换原子"
> 的把戏失效了，互斥只能靠**硬件原子指令**。再用**读写锁**优化"读多写少"。

## 0. 这节课在讲什么

`-smp 4` 下四个 hart 同时执行。两个核同时对 `counter++` 做"读-改-写"，会丢更新——
关中断只能挡住**本核**的抢占，挡不住**别的核**此刻也在改同一物理地址。所以 SMP 的互斥
必须落到硬件：`amoswap` / `lr.sc` / `amoadd` 这类原子指令（RV64 上 GCC 的 `__sync_*`
内建就展开成它们）。

本实验四个 hart 跑三段测试：

| 阶段 | 做什么 | 判据 |
|------|--------|------|
| A 无锁累加 | 每核 `racy_counter++` 一万次 | 仅演示：结果 < 4 万（丢更新） |
| B 自旋锁累加 | 每核 `spin_lock` 下 `counter++` 一万次 | `counter==40000` → `SPINLOCK_PASS` |
| C 读多写少 | 3 个读者反复读一致快照、1 个写者偶尔整组更新 | 无撕裂读 + 读者并行 → `RWLOCK_PASS` |

跑通输出 `SPINLOCK_PASS` / `RWLOCK_PASS` / `ALL_PASS`，qemu 正常关机（exit 0）。

## 1. 多核怎么起来的（已给：`main.c` + `smp.S`）

- OpenSBI 只把**引导 hart**送进 `_start`（共享 `entry.S`，单一 `boot_stack`）；
  其余 hart 停在 OpenSBI 里等唤醒。
- `kmain` 用 **SBI HSM `hart_start`**（`EID=0x48534D, fid=0, a0=hartid,
  a1=start_addr, a2=opaque`）唤醒它们，落点是 `smp.S` 的 `secondary_entry`：
  此时 `satp=0`、`a0=hartid`，它**自备 per-hart 栈**（几个核共用一个栈会互相踩烂），
  再 `call secondary_main`。
- 引导 hartid **未必是 0**。所以代码对 `0..NHART-1` 全部尝试 `hart_start`（对自身的
  启动被 SBI 拒绝、无害），worker 的"读者/写者"角色用一个**原子派发的唯一序号**来分，
  与 hartid 解耦。
- 四个核用一个**原子计数 + 代际标志的屏障**对齐相位（直接映射区共享变量 + `fence`）。

## 2. 你要实现的（`kernel/lock.c` 的 5 个原语）

```
spin_lock   : amoswap 抢锁（__sync_lock_test_and_set），抢不到自旋；建议 test-and-test-and-set
spin_unlock : 带 release 语义的原子写 0（__sync_lock_release）
read_lock   : state>=0 时 CAS 把读者计数 +1（多读者可同时成功 → 读临界区并行）
read_unlock : 原子 -1
write_lock  : 仅当 state==0 时 CAS 到 -1（独占）
write_unlock: 把 state 从 -1 改回 0
```

`state` 语义：`0` 空闲 / `>0` 当前读者数 / `-1` 写者独占。所有 `__sync` CAS/加减都是
全屏障，兼作内存序栅栏。占位（不加锁）版本能编译能跑，但会丢更新、读到撕裂数据，于是
三个 PASS 都不会出现——把 `// TODO` 换成真原子实现即可。

```
labctl run proper/S15-smp
make -C kernel run     # 手动跑（OpenSBI banner 后见内核输出）
```

## 3. 为什么读写锁优于自旋锁（看运行输出）

参考解输出里：

- `max_concurrent_readers=3`：同一时刻**三个读者**都持有读锁在跑——读临界区**真并行**。
  换成自旋锁，这个数会被强行压成 `1`（读者被串行化）。读多写少时，rwlock 把读吞吐放大。
- `torn_reads=0`：读者在读锁内看到的永远是写者更新**前或后**的一致快照，绝不会读到
  "一半旧一半新"。写者更新整组数据时独占写锁，把所有读者挡在外面。

代价：写者可能被连绵不断的读者**饿死**（读者优先）；读多写少时这个代价小、收益大，
所以恰好适配。临界区里**读远多于写**就上读写锁；写频繁、或临界区极短，反而自旋锁更省。

## 4. 完成标准 (DoD)

- [ ] 自旋锁正确：Phase B 的 `counter == NHART*ITERS`（无丢更新），出 `SPINLOCK_PASS`。
- [ ] 读写锁正确：`torn_reads==0` 且数据全相等、写者总更新数对，出 `RWLOCK_PASS`。
- [ ] `max_concurrent_readers > 1`，能说清这正是 rwlock 相对 spin 的优势。
- [ ] `ALL_PASS` 且 qemu 正常关机；无死锁、无崩溃。
- [ ] 能回答：单核"关中断"为何到 SMP 失效？那一位 `locked` 的"原子置位"靠什么硬件保证？

## 5. 引申

- **per-cpu 结构 + 细粒度锁**：全局大锁 → 每核私有数据免锁 + 局部锁，减少争用（S15a）。
- **写者优先 / 公平读写锁**：避免写者饿死（加 `writer_waiting` 计数让新读者让路）。
- **更强的锁**：MCS / ticket 自旋锁（公平、cache 友好）、RCU（读端零开销，接 S14 异步）。
- **内存序**：本实验用全屏障 `__sync_*` 图省事；真实内核会精到 acquire/release，
  配合 RISC-V 的 `fence r,rw` / `fence rw,w` 与 `lr.sc` 的 `.aq/.rl` 位。
