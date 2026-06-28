# 正经·S08b · mmap 与按需调页（demand paging，建在 S05c 之上）

> S05c 是「开机就把页全映射好」。本实验反过来：`mmap` 时**什么都不分配**，只在一张 VMA 表里
> 登记「这段 VA 将来合法」；等程序**真的访问**到某一页，硬件缺页，内核才临时给那一页补一个
> 物理帧、映进页表、让出错指令重跑一遍成功。缺页在这里不是错误，而是 OS 介入补页的**机制**。

## 0. 这节课在讲什么

S05c 里我们手动 `map_one` 把每个虚拟页都映射到物理帧——**预先全映射 (eager)**。但真实系统里
进程的地址空间往往很大、却只用到一小块（malloc 一大片只写几页、可执行文件映射进来只跑到的页才
读……）。预先全映射既费内存又拖慢启动。

**按需调页 (demand paging)** 把分配推迟到「第一次访问」：

```
mmap(addr,len,prot)  ──►  只登记一个 VMA（虚拟内存区域），不分配/不映射任何物理页
                          ┌─────────────────────────────────────────────┐
                          │ VMA 表:  [start, len, prot]  ……一条记录而已   │
                          └─────────────────────────────────────────────┘

首次访问该区域某页 VA
        │ 硬件 MMU 走页表发现叶 PTE 不存在
        ▼
   缺页异常 scause = 12/13/15 (取指/load/store page fault)
        │ trap → vma_handle_fault(stval)
        ▼
   查 VMA：命中？──否──► 真错误（野地址 / 已 munmap）：报错、跳过指令安全恢复
        │ 是
        ▼
   frame_alloc() 取一个零帧 → map_one 把「这一页」映进页表 → sfence.vma
        │ 不前移 sepc
        ▼
   sret 重执行刚才那条 load/store —— 此刻页已就位，访问成功
```

**VMA (Virtual Memory Area)**：一段连续、同权限的虚拟地址区间的元数据 `{start, len, prot}`。
它回答缺页处理器一个问题：「这个出错地址，是该补页的合法区域，还是该报错的野地址？」

## 1. 你要实现的

只动 `kernel/vma.c` 里两个 `// TODO`（分页 `paging.c`、trap 钩子 `trap.c`、harness `main.c`、
VMA 表 / `vma_find` / `munmap` 均已给定）：

### `mmap(addr, len, prot)` —— 只登记一个 VMA（lazy）
```
addr 向下对齐页、len 向上取整到整页
找一个空 VMA 槽：填 {start=addr, len, prot, used=1}
return addr
关键：到此为止——不要 frame_alloc，不要 map_one。物理页留给缺页时再补。
```

### `vma_handle_fault(fault_va)` —— 缺页：查 VMA，命中则补帧映射
```
v = vma_find(fault_va)
if (!v) return 0;                         // 不在任何 VMA → 交回 trap 当真错误
page_va = PAGE_DOWN(fault_va)             // 只补「出错的这一页」
pa = frame_alloc()                        // 一个零帧
map_one(g_root, page_va, pa, prot→flags)  // 映进页表
sfence.vma                                // 刷 TLB，重执行才能命中
return 1                                  // 已处理：trap 不前移 sepc → 重跑出错指令成功
```

`prot→flags`、`g_root`（由 `vma_init` 绑定）、`PAGE_DOWN` 均已在给定代码里备好。

跑起来：
```
labctl run proper/S08b-mmap
make -C kernel run     # 手动跑（OpenSBI banner 后见内核输出）
```

判据：依次输出
- `MMAP_PASS`：mmap 登记后 VMA 在、但页表里这段 VA 仍无任何叶 PTE（确实没提前分配）；
- `FAULT_HANDLED_PASS`：访问触发缺页 → 按需补页 → 读回 == 写入值；
- `LAZY_PASS`：只触碰第 0、2 页时恰好 2 次按需缺页、恰好多用 2 个物理帧，未碰的第 1、3 页仍未映射；
- `UNMAP_PASS`：`munmap` 后再访问被识别为真错误缺页（不在任何 VMA）；
- 最后 `ALL_PASS`，且不出现 `*_MISS` / `*_BAD` / `FAIL` / `panic`。

## 2. 完成标准 (DoD)

- [ ] `mmap` 只写 VMA 不映射页：`MMAP_PASS`（VMA 在、页表空）。
- [ ] `vma_handle_fault` 命中补页 + 不前移 sepc 重执行：`FAULT_HANDLED_PASS`（写入值读得回）。
- [ ] 真·按需：缺页次数 == 被触碰页数、未碰页不占帧：`LAZY_PASS`。
- [ ] `munmap` 后再访问落到「无 VMA」分支被识别：`UNMAP_PASS`。
- [ ] 能讲清：mmap 用途、按需调页为何省内存/加快启动、缺页是机制不是错误、CoW 也靠缺页。

## 3. 引申（怎么扩成完整版）

- **写时复制 (CoW)**：`fork` 后父子共享物理页、PTE 去掉 `W`；任一方写 → store 缺页 → 复制一份私有
  页再放行。和本实验一样靠缺页钩子，只是命中后的动作从「补新帧」变「复制旧帧」。
- **文件映射 `mmap(fd,...)`**：VMA 记下 `{文件, 偏移}`；缺页时不是给零帧，而是从 VFS(S07) 读入该页。
- **匿名映射 / 共享内存**：本实验就是匿名映射（补零帧）；多个进程的 VMA 指向同一组帧即共享内存。
- **栈自动增长 / Guard Page**：栈底放一个不在任何 VMA 的 Guard Page，越界访问落「真错误」分支即捕获；
  合法增长则扩 VMA 后由缺页补页。
- **回收**：本实验 `munmap` 只清叶 PTE、不回收帧（bump 分配器无 free）。完整版需位图/链表帧分配器。
- 对照 **S05c（预先全映射）**：S05c 开机 `map_one` 把区间每页都映了；这里 mmap 只登记、页表留空，
  按需补页——同一套页表机制，分配时机从 eager 改成 lazy 即得 mmap/CoW/栈增长。
