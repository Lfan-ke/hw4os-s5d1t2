# 11 · 堆与栈：SP 是一根指针，allocator 是一个记账员

> 不正经赛道 · 第 11 课 —— 纯软件、host 直接跑（用数组/结构体软件模拟「设备当内存」）。
> 一句话母题：**栈指针 SP 只是一根会上下挪的指针，堆 allocator 只是个在内存上划地盘的记账员**。

## 0. 这节课在讲什么

你以为「`new` 一个对象」「函数调用压栈」是语言魔法，拆开看其实平平无奇：

- **SP** = 一个能加减的下标，约定栈「向下生长」；
- **allocator** = 一个游标（cursor），约定堆「向上生长」；
- **越界保护** = 连一根比较器（`heap_top` 与 `sp` 比大小）；
- **全局分配器** = 把你写的 allocator「注册」成 `Box`/`malloc` 背后那一个。

对应真实系统：rcore `mm/heap_allocator.rs` 的 `#[global_allocator] static HEAP_ALLOCATOR` + `init_heap()`（把静态 `HEAP_SPACE` 喂给 allocator）、`entry.asm` 里 `la sp, boot_stack_top`；xv6 `kalloc.c` 空闲链表、用户栈向下生长 + guard page、`sbrk` 抬堆。经典布局：`text/data/bss/heap↑ … ↓stack`，堆栈对向逼近，靠 guard page / `RLIMIT_STACK` 兜底。

## 1. 四段递进（同一个可执行里逐段点亮 `*_PASS`）

| 子实验 | 你要填 | 判据 |
| :-- | :-- | :-- |
| **11.1 单设备单栈** | `Stack::sp_init/push/pop`（满递减栈，越界报 `STACK_OVERFLOW`） | LIFO 还原对 + 越界被检出 → `STACK_PASS` |
| **11.2 两块设备** | `HeapDev::alloc`（bump 向上，越界 OOM） | 栈 A、堆 B 各自触顶、互不波及 → `HEAP_INDEP_PASS` |
| **11.3 单设备对向生长** | `OneDev::alloc/push`（各自自查边界） | 恰好相遇于边界报错、无静默覆盖 → `COEXIST_PASS`；若静默覆盖打 `COLLIDE_UNDETECTED` 判挂 |
| **11.4 注册到 global** | rust 加 `#[global_allocator]` 一行 / C `g_alloc = &my_bump;` | 高层容器分配的字节落进你的区域 → `GLOBAL_PASS` |

四段全过再打印 `ALL_PASS`。

文件：`sw/rust/src/main.rs` 与 `sw/c/heapstack.c`，只填带 `TODO` 的函数体与「注册」一行，`harness（勿改）` 负责喂数据、校验、打印。

```
labctl run improper/11-heap-stack     # 跑 C/Rust 两条路径
labctl watch                          # 边改边自动判定
labctl hint improper/11-heap-stack    # 卡住看提示
```

### 关键约定速记

- **满递减栈**：空栈 `sp == top`；`push` 先 `sp-=1` 再写；`pop` 先读再 `sp+=1`；`sp <= base` 还要压 = 溢出。
- **bump 堆**：`heap_top` 是下一个空闲槽；`alloc(n)` 当 `heap_top + n > 上限` 即 OOM；只进不退（不实现 free 合并）。
- **单设备对向生长**：堆区 `[0, heap_top)` 向上、栈区 `[sp, cap)` 向下，二者「上限」互为对方的游标——`alloc` 拿 `sp` 当上限、`push` 拿 `heap_top` 当下限。恰好相遇时 `heap_top == sp`。

## 2. 完成标准 (DoD)

- [ ] `STACK_PASS`：满递减栈 LIFO 还原正确，越界被检出（不静默覆盖）。
- [ ] `HEAP_INDEP_PASS`：两块独立设备，栈/堆各自 OOM/overflow，互不侵犯。
- [ ] `COEXIST_PASS`：单设备堆栈对向生长，恰好在边界报 `OOM`/`STACK_OVERFLOW`，无 `COLLIDE_UNDETECTED`。
- [ ] `GLOBAL_PASS` + `ALL_PASS`：注册 allocator 后，`Box`/`Vec`（rust）/`my_malloc`（C）分配的字节确实落进你自管的区域。
- [ ] C/Rust 任一条路径全过（必修）；另一条也过计辅助分。
- [ ] `essay/THINKING.md` 三问作答。

## 3. 思考题（`essay/THINKING.md` 作答即可通过）

1. SP 只是一个能加减的指针：为什么栈「向下生长」、堆「向上生长」是**约定而非物理必然**？把方向对调、或让 SP「先写后减」改「先减后写」，会改变什么、不会改变什么？（联系真实 layout 与调用 ABI）
2. rcore 里 `#[global_allocator]` + 一行注册完，`Box`/`Vec` 就能用；C 里没有这个语言钩子，你是怎么「**手动注册**」的？编译器替 rust 做了哪件你必须替 C 补的事？
3. 单设备里堆栈对向生长、靠一个**比较器**检测碰撞——它和硬件 MMU/MPU 的 **guard page**、栈溢出保护是什么关系？为什么换成两块独立设备就天然不会互相侵犯？这对「隔离 vs 共享同一地址空间」的取舍有什么启示？

## 4. 简化取舍

- 「设备 = 内存」抽象成平坦的字数组 + 游标，不掺扇区/块 I/O 真实粒度。
- allocator 用 bump（只进不退，不做 free 合并 / 对齐 / buddy / slab）。
- 栈帧抽象成 push/pop 一个 word，不走真实 RV64GC 调用 ABI。
- 碰撞用显式比较器而非真 MMU 缺页——「真·虚拟内存 + guard page」顺势抛给下一章「12 地址空间」。
- 单线程，去掉 rcore `LockedHeap` 的自旋锁。
- **完整版作为引申**：buddy/slab、对齐与 `free`+coalesce、`mmap`/`brk` 语义、MMU 硬件 guard page 与栈金丝雀、多核加锁。
