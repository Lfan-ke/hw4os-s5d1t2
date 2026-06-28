# 正经·S05c · SV39 虚拟内存（rcore ch4 核心）

> 这是真·分页：内核第一次给 CPU 装上一张「地址翻译表」，从此每个虚拟地址都要过三级页表
> 才落到物理内存。对照 improper-12 用软件模拟的 MMU——这里是硬件 MMU + SV39 真表。

## 0. 这节课在讲什么

CPU 取指/访存用的都是**虚拟地址 (VA)**；开启分页后，硬件 MMU 按 `satp` 指向的根页表，
把 VA 经**三级页表 walk** 翻译成**物理地址 (PA)** 再访问 DRAM。SV39 的 39 位 VA 布局：

```
 38      30 29      21 20      12 11        0
+---------+----------+----------+-----------+
|  vpn2   |  vpn1    |  vpn0    |  offset   |   每段 9 位 → 每级 512 项
+---------+----------+----------+-----------+
   root[]     L1[]      L0[]      页内偏移
```

每级页表 512 项、每项 8 字节 = 恰好一页 (4KB)。PTE 格式：

```
 63        54 53                10 9 8 7 6 5 4 3 2 1 0
+-----------+--------------------+---+-+-+-+-+-+-+-+-+
| reserved  |       PPN[43:0]    |RSW|D|A|G|U|X|W|R|V|
+-----------+--------------------+---+-+-+-+-+-+-+-+-+
```

- `V`=有效；`R/W/X`=读/写/执行权限；`U`=U 态可访问；`A/D`=访问/脏位。
- **R=W=X 全 0 的有效 PTE = 指向下一级页表的「非叶」节点**；否则是「叶」PTE（直接给出物理页）。
- 物理页 → PTE：`(pa >> 12) << 10 | flags | V`。

**为什么必须恒等映射保活**：写下 `satp` 的那一瞬间，下一条指令就已经走分页了。如果内核当前
PC 所在的物理页、栈所在的物理页没有被映射，CPU 取下一条指令立刻缺页 → 当场暴毙。所以开分页前，
必须先把「内核运行所需区间」做**恒等映射**（VA==PA）。本实验 harness 已替你恒等映射
`0x80000000~0x80800000`（内核 text/data/bss + 帧池 + 栈）外加 UART MMIO 页——前提是你的
`map_one` 正确。

## 1. 你要实现的（`kernel/paging.c` 两个 TODO）

`frame_alloc`（帧分配器）、`trap.c`（缺页钩子）、`main.c`（harness）、`paging.h`（PTE 位定义）都已给。
你只填：

### `map_one(root, va, pa, flags)` —— 三级 walk + 建中间表 + 写叶 PTE
```
table = root
for level in (2, 1):                 # 走前两级
    vpn = (va >> (12 + 9*level)) & 0x1FF
    if table[vpn] 无效:
        next = frame_alloc()          # 新建中间页表
        table[vpn] = (next>>12)<<10 | V   # 非叶 PTE：只置 V
        table = next
    else:
        table = ((table[vpn]>>10) & 0xFFFFFFFFFFF) << 12   # 取 PPN 还原下级地址
vpn0 = (va >> 12) & 0x1FF
table[vpn0] = (pa>>12)<<10 | flags | V    # 叶 PTE
```

### `enable_paging(root)` —— 写 satp 开 SV39
```
satp = (8 << 60) | ((uint64_t)root >> 12)   # MODE=8(SV39) | 根表 PPN
sfence.vma ; csrw satp ; sfence.vma
```

> 提示：两个 TODO 都没填时，内核不会写 `satp`、停在 `PAGING_ON_MISS` 安全退出（不崩、不死循环），
> 但拿不到 `ALL_PASS`。填对后才会一路 `PAGING_ON_PASS → MAP_PASS → TRANSLATE_PASS → FAULT_PASS → ALL_PASS`。

跑起来：

```
labctl run proper/S05c-paging
make -C kernel run     # 手动跑（OpenSBI banner 后见内核输出）
```

判据：依次输出 `PAGING_ON_PASS` / `MAP_PASS` / `TRANSLATE_PASS` / `FAULT_PASS` / `ALL_PASS`，
不出现 `*_MISS` / `*_BAD` / `FAIL`。

## 2. 完成标准 (DoD)

- [ ] `map_one` 正确建三级表：恒等映射内核区后写 `satp` 不崩 → `PAGING_ON_PASS`。
- [ ] 给新 VA `0x5000_0000` 映射新帧，经该 VA 写入 `MAGIC` 再读回相等 → `MAP_PASS` / `TRANSLATE_PASS`。
- [ ] 访问未映射 VA `0x4000_0000` 触发缺页（`scause`=13 load page fault），trap 置标志并跳过指令安全恢复 → `FAULT_PASS`。
- [ ] 能讲清：物理地址 vs 虚拟地址、为何恒等映射保活、三级 walk 怎么走、缺页的用途。

## 3. 引申（怎么扩成完整版）

- **U 态独立地址空间**（接 S08）：每进程一张页表，U 态页带 `U` 位、内核页不带；切进程换 `satp`+`sfence.vma`。
- **按需调页 (demand paging)**：先建空洞，首访缺页时再 `frame_alloc` 补叶 PTE、重执行——本实验缺页捕获的延伸。
- **写时复制 (COW)**：`fork` 后共享只读页，写时缺页再复制私有页。
- **`mmap`/文件映射**：缺页时从 VFS（S07）读入页。
- 用 **2MB 大页**（L1 级放叶 PTE）减少页表占用；本实验为教学全用 4KB 叶页。
