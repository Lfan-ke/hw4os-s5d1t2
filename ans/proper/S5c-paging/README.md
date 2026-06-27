# 正经·S5c · SV39 虚拟内存（rcore ch4 核心）

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
PC 所在的物理页、栈所在的物理页没有被映射，CPU 取下一条指令立刻缺页 → 取异常处理入口又缺页
→ 当场暴毙。所以开分页前，必须先把「内核运行所需区间」做**恒等映射**（VA==PA），让开启前后
执行流无缝衔接。本实验恒等映射 `0x80000000~0x80800000`（内核 text/data/bss + 帧池 + 栈）外加
UART MMIO 页。

## 1. 你要实现的

`kernel/paging.c` 里两个 `// TODO`（其余 `frame_alloc` / trap 钩子 / harness 已给）：

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

跑起来：

```
labctl run proper/S5c-paging
make -C kernel run     # 手动跑（OpenSBI banner 后见内核输出）
```

判据：依次输出 `PAGING_ON_PASS`（开分页后仍存活）/ `MAP_PASS`（经新 VA 写成功）/
`TRANSLATE_PASS`（经新 VA 读回 == 写入值，且物理帧确含该值）/ `FAULT_PASS`（访问未映射 VA
触发缺页被 trap 捕获）/ `ALL_PASS`，且不出现 `*_MISS` / `*_BAD` / `FAIL`。

## 2. 完成标准 (DoD)

- [ ] `map_one` 正确建三级表：恒等映射内核区后写 `satp` 不崩 → `PAGING_ON_PASS`。
- [ ] 给新 VA `0x5000_0000` 映射新帧，经该 VA 写入 `MAGIC` 再读回相等 → `MAP_PASS` / `TRANSLATE_PASS`。
- [ ] 访问未映射 VA `0x4000_0000` 触发缺页（`scause`=13 load page fault），trap 置标志并跳过指令安全恢复 → `FAULT_PASS`。
- [ ] 能讲清：物理地址 vs 虚拟地址、为何恒等映射保活、三级 walk 怎么走、缺页的用途。

## 3. 引申（怎么扩成完整版）

- **U 态独立地址空间**（接 S8）：每个进程一张页表，U 态页带 `U` 位、内核页不带 `U`；切进程时
  换 `satp` + `sfence.vma`。用户访问内核地址直接缺页被挡——真隔离。
- **按需分配 (demand paging)**：先建「不带 `V`」的空洞映射，首次访问缺页时 trap 里再 `frame_alloc`
  补上叶 PTE、重执行——这正是本实验缺页捕获能力的延伸。
- **写时复制 (COW)**：`fork` 后父子共享物理页、PTE 去掉 `W`；任一方写触发缺页，复制一份私有页再放行。
- **`mmap` / 文件映射**：把文件页按需映入地址空间，缺页时从 VFS（S7）读入。
- 用 **2MB 大页**（在 L1 级直接放叶 PTE）减少页表占用；本实验为教学全用 4KB 叶页。
