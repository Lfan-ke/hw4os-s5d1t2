# 正经·S5e · 进程：fork / exec / wait + 写时复制（rcore ch5）

> 承接 S5c（SV39 分页 / 帧分配 / 缺页捕获）；复用与 S8 相同的 U 态 sret/ecall 机制（本课框架已代办）。
> 这一课把「单进程」长成「多进程」：每个进程有**自己的地址空间**，靠 `fork` 复制、
> `exec` 替换、`wait` 回收，并用**写时复制（CoW）**让 `fork` 既快又省内存。

## 0. 这节课在讲什么

- **进程 = 地址空间 + 执行上下文**。本实验每进程一张独立 SV39 根页表；调度时切 `satp` 即切地址空间。
- **`fork`**：复制父进程地址空间得到子进程。整页复制太贵——用 **CoW**：复制页表项但把可写页降为
  **只读 + `PTE_COW`**、父子**共享同一物理帧**；谁先写谁触发缺页，再**分裂**出私有帧恢复可写。
  这样 `fork` 后没立刻拷贝任何用户数据，写到哪儿才拷哪一页。
- **缺页 ↔ CoW**：CoW 必须配合缺页机制——只读共享页被写 → store 缺页 → 内核在缺页处理里分裂页。
- **`exec`**：用另一个程序替换当前进程的整个地址空间（新页表 + 新栈），PID 不变。
- **`wait`**：父进程阻塞等子进程退出，回收**僵尸（zombie）**进程、取回退出码。

> 嵌入镜像的用户程序按内核链接地址存在，但 **S 态不能从带 `U` 的页取指**（硬件约束）。
> 故内核镜像被映射两份：内核 VA（恒等、无 U，S 态跑内核/陷入）+ **用户别名 VA（带 U+X，U 态跑用户代码）**，
> 指向同一物理页。这是给定框架处理好的，理解即可。

## 1. 你要实现的（`kernel/proc.c` 两处）

其余都已给：分页/帧分配/引用计数（`mm.c`）、U 态切换与 trap 框架、`fork/exec/wait/exit`
主体与调度（`sched.c`）、两个内嵌用户程序（`user.c`）、harness（`main.c`）。

### ① `fork_copy_uvm(parent, child)` —— 复制父地址空间的用户页

`child->root` 已 `frame_alloc` 且已 `map_kernel`（内核 + 用户别名都装好）。你只处理用户私有页：

```
对每个用户栈页 va ∈ [USTACK_BASE, USTACK_TOP)：       // 栈：整页 eager 复制
    ppa = va2pa(parent->root, va)
    nf  = frame_alloc(); kmemcpy(nf, ppa, PAGE_SIZE)
    map_one(child->root, va, nf, R|W|U)
数据页 DATA_VA：                                        // 数据页：CoW 共享
    dpa = va2pa(parent->root, DATA_VA)
    map_one(parent->root, DATA_VA, dpa, R|U|COW)        // 父：去掉 W，置 COW
    map_one(child->root,  DATA_VA, dpa, R|U|COW)        // 子：同帧、同样只读 COW
    ref_inc(dpa)                                         // 该帧现被父+子共享
sfence.vma; return 0
```

> 为什么栈不走 CoW？trap 跑在**用户栈**上（共享 `__alltraps` 不换栈），若栈是只读 CoW，
> 压栈那一刻就缺页，而处理缺页又要压栈 → 嵌套死局。故栈直接复制成私有可写页，最稳。

### ② `cow_fault(fault_va)` —— 写时复制缺页分裂

只读 CoW 页被写 → store 缺页 → 这里把它分裂成**私有可写**页：

```
vpage = fault_va 向下取整到页
pa    = va2pa(current->root, vpage)
若 ref_get(pa) > 1：                                    // 还有人共享 → 真分裂
    nf = frame_alloc(); kmemcpy(nf, pa, PAGE_SIZE)
    ref_dec(pa)
    map_one(current->root, vpage, nf, R|W|U)            // 改指新帧、恢复 W、清 COW
    打印 COW_PASS（含 old/new 物理页，证明确实分裂）
否则（最后持有者）：
    map_one(current->root, vpage, pa, R|W|U)            // 无需复制，恢复 W
sfence.vma                                              // 随后硬件重执行出错指令
```

> 缺页处理**不前移 sepc**：修好映射后重执行同一条访存指令。`ecall` 才 `+4`。

```
labctl run proper/S5e-process
make -C kernel run     # 手动跑（OpenSBI banner 后见内核输出）
```

判据：`FORK_PASS`（子进程创建、父得 pid 子得 0）/ `COW_PASS`（子写数据页时分裂出新物理帧、
父子隔离）/ `EXEC_PASS`（子 exec 载入第二个内嵌程序运行）/ `WAIT_PASS`（父 wait 拿到退出码 7）/
`ALL_PASS`，不出现 `UNEXPECTED_*` / `FAIL`。

## 2. 完成标准 (DoD)

- [ ] `fork` 后父进程拿到子 pid、子进程从 `fork` 返回 0，二者各自地址空间运行。
- [ ] 子进程写共享数据页触发 `cow_fault` 分裂：新物理帧 ≠ 旧帧；父进程的值不受影响（隔离）。
- [ ] 子进程 `exec` 成功载入并运行第二个内嵌程序，退出码经父 `wait` 回收（=7）。
- [ ] 能说清：CoW 为何省内存、为何必须配合缺页；`exec` 替换地址空间与 `fork` 复制的区别。

## 3. 引申

- **CoW 也复制栈**：真实系统栈也 CoW，但需要**独立内核 trap 栈**（`sscratch` 换栈），
  否则压栈即触发缺页嵌套。本实验为简化让栈 eager 复制、只对数据页演示 CoW。
- **`vfork`**：连页表都不复制，父子共享地址空间直到子 `exec`/`exit`，父挂起——比 CoW 更激进的省法。
- **地址空间回收**：本实验 `exec`/`wait` 不回收旧页表与帧（帧池够用）；真实系统要释放 PTE/帧、减引用计数。
- **ELF 加载**：本实验用「内嵌函数 + 别名映射」替代 ELF 解析；真实 `exec` 解析 ELF 段、按段权限建映射。
- **对照 S8**：S8 是 ch2 批处理——单进程、**无分页**、仅特权级隔离；本实验是 ch5——多进程、
  每进程独立页表、缺页驱动的 CoW，进程间真正隔离。
