# 正经·S05b · 内核堆分配器（rcore ch4 的 kernel heap）

> 承接 S03（内核已能跑 C、有静态数据）。到目前为止内核里所有东西都是**静态/栈上**的：
> 大小写死、生命周期跟着作用域。可一旦要管「数量不定、寿命不一」的对象——进程控制块、
> 文件描述符、内核缓冲区——就需要一个**动态分配器**：按需 `kalloc`，用完 `kfree`，
> 且释放的空间要能**复用**、相邻空闲要能**合并**。本课就在一块静态字节池上把它造出来。

## 0. 这节课在讲什么

没有 `malloc` 的世界里，内核得**自己当 libc**。最经典的做法是 **free-list 分配器**：

- 把一整块静态内存（这里 64KB）看成「一个大空闲块」。
- 每个块开头藏一个 16 字节**头** `block_t{ size, next }`：`size` 是本块连头带尾的总字节数
  （恒 16 对齐），`next` 把**空闲**块串成一条**按地址升序**的链表（占用时这块内存还给用户当 payload）。
- `kalloc(n)`：**first-fit** 扫链表，找第一个够大的空闲块；剩余够大就**分裂**出尾块，否则整块取走。
- `kfree(p)`：把块按地址**插回**链表，再与**相邻**空闲块**合并(coalesce)**——这是对抗
  **外部碎片**的关键：否则连续释放出的相邻碎块各自太小，攒一堆却装不下一个稍大的请求。

> 这正是 rcore ch4 引入的 kernel heap（Rust 里用 `#[global_allocator]` 挂一个 buddy/slab
> 分配器，让 `Box`/`Vec` 在内核里可用）。本课用确定性的 C free-list 把同一件事讲透。

## 1. 你要实现的（`kernel/kheap.c` 的两个 `// TODO`）

`block_t`、`heap_pool`、`free_list`、`kheap_init`、`align_up` 与对齐/分裂的常量都已给好。
你只需补两处核心逻辑：

**`kalloc`：找 / 切空闲块（first-fit + 分裂）**
```
total = HDR + align_up(n, ALIGN)          // 需要的总字节
遍历 free_list（地址有序）找第一个 size>=total 的块 b：
    若 b->size >= total + HDR + ALIGN：分裂——尾部 (char*)b+total 处建剩余空闲块，b 收缩成 total
    否则：整块取走
    把 b（或顶替它的剩余块）从链表摘除
    返回 (char*)b + HDR
找不到 → 返回 0（池满）
```

**`kfree`：插回 + 相邻合并（coalesce）**
```
b = (char*)p - HDR
按地址升序找到 prev < b < cur，把 b 接进链表
若 b 紧邻后继 cur（(char*)b + b->size == (char*)cur）：b 吞掉 cur（size 相加、跳过 cur）
若前驱 prev 紧邻 b（(char*)prev + prev->size == (char*)b）：prev 吞掉 b
```

提示：用一个 `prev` 指针（或二级指针 `block_t **link`）来从单链表里摘除/插入。

```
labctl run proper/S05b-kheap
make -C kernel run     # 手动跑（OpenSBI banner 后见内核输出）
```

判据（harness 给定，勿改）：
- `ALLOC_PASS`：连续分配的几块互不重叠、16 对齐、写各自 magic 后读回一致。
- `REUSE_PASS`：释放中间一块再分配同样大小，复用刚释放的**同一地址**。
- `COALESCE_PASS`：释放相邻两块后，能分配一个「单块装不下」的更大块，且它正落在合并区起点
  （证明两块真的合并了，而非各自为政）。
- 三关全过 → `ALL_PASS`。不得出现 `FAIL` / `panic` / `UNEXPECTED`。

## 2. 完成标准 (DoD)

- [ ] `kalloc` first-fit 正确，必要时分裂；返回 16 对齐指针，池满返回 0。
- [ ] `kfree` 按地址插回，并与前驱/后继相邻空闲块合并。
- [ ] `ALLOC_PASS` / `REUSE_PASS` / `COALESCE_PASS` / `ALL_PASS` 全出，qemu 正常关机。
- [ ] 能说清：为什么需要 coalesce（外部碎片），first-fit/best-fit/buddy/slab 的取舍。

## 3. 引申（怎么扩成完整版）

- **buddy 分配器**：池大小取 2 的幂，按 2^k 分级；分配向上取整到某级、对半分裂，
  释放时与「伙伴块」（地址异或块大小）合并。O(log n)、合并 O(1)，rcore 常用。
- **slab / 对象缓存**：对固定大小对象（如 PCB、inode）预切等大槽位，分配/释放 O(1)、零外碎片；
  内核里高频小对象更划算。可在本 free-list 之上叠一层。
- **统计与防越界**：记录 `used/free/峰值`；每块加 **canary**（头尾写魔数，`kfree` 时校验）
  与对齐/双重释放检查，把「悄悄踩坏」变成「当场报错」。
- **接 Rust `global_allocator`（对照 S01b）**：把 `kalloc/kfree` 包成 `GlobalAlloc`，
  内核里就能用 `Box`/`Vec`——这正是 rcore ch4 的形态。
