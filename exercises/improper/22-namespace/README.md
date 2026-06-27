# 22 · 容器隔离：namespace 切视图 + cgroup 卡配额

> 不正经赛道 · 第 22 课 —— 纯软件心智模型，host 直接跑（rust / c 双语言）。
> 一句话母题：**容器不是「轻量虚拟机」，它就是几个被换了视图、被记了配额账的普通进程。
> 给它换一套 namespace（看不见别人），再给它一本 cgroup（占不了太多），盒子就成了。**

## 0. 这节课在讲什么

「容器隔离」听着玄，拆开只有两件事：

- **namespace = 换视图**。同一颗内核，给某组进程换一套「看见什么」的视图：
  - **PID namespace**：容器内进程从 1 号(init) 重新编号；宿主看到的全局进程 7777，
    在 A 容器里是 3 号、在 B 容器里是 2 号——**同一个进程，不同容器看到不同 pid**。
  - **Mount namespace**：每个容器各持一张挂载表；A 挂的 `/data-a` 在 B 里压根查不到。
- **cgroup = 记配额账**。namespace 不限量，cgroup 才给容器记内存/CPU 上限：
  申请超过配额就**被拒**，用量统计还得准。

本课不调真 `clone(CLONE_NEWPID)`、不写真 cgroup 文件，而是用**纯函数 + 给定向量**
把这三件事的心智模型「跑」出来——没吃过猪肉，但把猪跑的样子看清楚。

## 1. 你要填的 4 个纯函数

软件在 `sw/rust/src/main.rs` 或 `sw/c/namespace.c`，两语言同构。

| 子实验 | 函数 | 要求 | 判据 |
| :-- | :-- | :-- | :-- |
| 1 PID ns | `ns_local_pid` / `ns_global_pid` | 全局 pid ↔ 容器内局部 pid；init=局部 1 | `PIDNS_PASS` |
| 2 Mount ns | `mount_lookup` | 在本容器挂载表里查路径下标，不可见=-1 | `MOUNTNS_PASS` |
| 3 cgroup | `cgroup_charge` | 放得下才批准；超额被拒且用量不变 | `CGROUP_PASS` |

三段皆过再打印 `ALL_PASS`。失败会打印含 `FAIL`/`BAD` 的诊断行（如 `PIDNS_FAIL`
隔离失效、`CGROUP_FAIL` 超额却偷偷计费）。

### 模型约定（判题用）

- **PID namespace**：一个 ns 持有「按加入顺序排的全局 pid 列表」`globals`。
  `globals[0]` 是该 ns 的 init，容器内局部 pid 恒为 **1**；其后依次 2,3,…。
  `ns_local_pid(globals, g)` = 找 `g` 的下标 +1（找不到 → `None`/0）；
  `ns_global_pid(globals, local)` = `globals[local-1]`（越界 → `None`/0）。
- **Mount namespace**：`mount_lookup(paths, target)` = `target` 在本表的下标，
  未挂载（不可见）返回 `-1`。隔离 = A 的私有挂载点在 B 表里查不到。
- **cgroup**：`cgroup_charge(used, quota, request)`：
  `used+request <= quota` → 批准 `(used+request, true)`；否则拒绝 `(used, false)`，
  **用量原封不动**（超额绝不能偷偷涨）。

```
labctl run improper/22-namespace      # 跑 rust / c 两条路径
labctl watch                          # 边改边自动判定
labctl hint improper/22-namespace     # 卡住看提示
```

## 2. 完成标准 (DoD)

- [ ] 至少一条变体跑出 `PIDNS_PASS` / `MOUNTNS_PASS` / `CGROUP_PASS` / `ALL_PASS`，无任何 `*_FAIL`/`*_BAD`（必修）。
- [ ] rust 与 c 对同一向量行为一致；两条都过计辅助分。
- [ ] rust `cargo run -q` 0 warning。
- [ ] essay 子题答出「namespace 切视图 / cgroup 卡配额、容器 vs 虚拟机、6 种 namespace」要点。
- [ ] 能口述：为什么「同一全局进程在不同容器看到不同 pid」就是 PID 隔离；为什么超额申请必须被拒且用量不变。

## 3. 引申（从模型到真实）

- 真实 PID namespace 靠 `clone(CLONE_NEWPID)`：新 ns 里第一个进程就是 pid 1，
  它死了整个 ns 的进程全被清理——正如本课 `globals[0]` 永远是 init。
- 真实 mount namespace 靠 `CLONE_NEWNS` + `pivot_root`/overlayfs，给容器一张私有挂载表，
  正如本课每个容器各持一个 `paths` 数组。
- 真实 cgroup v2 通过 `memory.max`/`memory.current` 等文件限量，超额触发拒绝或 OOM kill，
  正如本课 `cgroup_charge` 的「放不下就拒、用量不动」。
- 容器 = namespace + cgroup；vs 虚拟机 = 共享内核 vs 独立内核。镜像与运行时由 **OCI** 标准
  约束，参考实现 **runc**；Docker / containerd / Kubernetes(CRI) 都落到它上面。

## 4. 思考题（`essay/THINKING.md` 作答即可通过）

1. namespace + cgroup = 容器，这两者各管「隔离」的哪一半？
2. 容器 vs 虚拟机差在哪？为什么容器「轻」？（共享内核 vs 独立内核）
3. OCI 标准与 Docker 工具分别是什么？runc / containerd 在哪一层？
4. Linux 六种主要 namespace（PID/MNT/NET/UTS/IPC/User）各管什么？
