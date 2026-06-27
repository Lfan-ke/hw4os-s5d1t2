# 08 · 文件管理：从裸指针块设备到 easy-fs 风格的简易文件系统

> 不正经赛道 · 第 8 课 —— 块设备 = 软件大数组；host 直接跑（硬件变体走本地仿真）。
> 一句话母题：**文件 / 目录 / KV 记录，都是软件在「一大片能按块寻址的格子」上约定出来的字节排布。**

## 0. 这节课在讲什么

磁盘在硬件眼里只是一片可按块寻址的 RAM。所谓「文件」「目录」全是软件的故事。
本课把一块 RAM 块设备，一路喂养成能 `mkfs`、能建目录、能读写文件的小文件系统。
三段逐题递进，对应 rcore 的 easy-fs（`BlockDevice`/`SuperBlock`/`DiskInode`/`DirEntry`）
与 xv6 的 `fs.c`/`mkfs.c`：

1. **(a) 裸指针 / MMIO 块设备**：把块号换成 MMIO 窗口基址，用 `volatile` 裸指针搬整块。
2. **(b) KV 记录扫描**：`EMM233` 开头、`EMM666` 结尾、头后 3 字节当 key（短文件名雏形）。
3. **(c) easy-fs 风格 inode / 目录 / 目录项**：`mkfs → create → ls → 读写回路`。

## 1. 磁盘布局（软件路径）

```
block 0      : SuperBlock { magic, num_blocks, inode_start, inode_count, data_start, next_inode, next_block }
block 1      : inode 区  —— 16 个 DiskInode，每个 32 B：size(4)+type(4)+direct[6]*4
block 2      : 根目录数据块 —— DirEntry[16]，每个 32 B：name[28]+inode_no(4)
block 3..63  : 文件数据区
```

KV 记录布局：`EMM233 | key[3] | len(u8) | data[len] | EMM666`，头不匹配即视为记录区结束。

## 2. 你要填的函数

| 文件 | 函数 | 判据 |
| :-- | :-- | :-- |
| `sw/{rust,c}` | `bd_write_block` / `bd_read_block` | 写进去读出来逐字节相等 → `BDEV_PASS` |
| `sw/{rust,c}` | `find_by_key` | 命中记录数 + 数据校验和正确 → `KV_PASS` |
| `sw/{rust,c}` | `fs_mkfs` / `fs_create` / `fs_ls` / `fs_write` / `fs_read` | mkfs→建 3 文件→ls 命中→读写一致 → `FS_PASS` |
| `hw/v` | `ram_bdev` 的 `always` | tb 全块写读相等 → `BDEV_PASS` |
| `hw/bsv` | `mkBdev` 的 `wr`/`rd` | 同上 → `BDEV_PASS` |

三段（软件）皆过再打印 `ALL_PASS`；硬件块设备过即 `BDEV_PASS`+`ALL_PASS`。
块设备搬运可二选一实现：`// TODO[a]` 按字节循环 / `// ELSE[b]` 整块 memcpy。

```
labctl run improper/08-filesystem     # 跑全部变体
labctl watch                          # 边改边自动判定
labctl hint improper/08-filesystem    # 卡住看提示
```

## 3. 完成标准 (DoD)

- [ ] `BDEV_PASS`：块设备写进去再读出来逐字节相等（至少一条路径，必修）。
- [ ] `KV_PASS`：能按 key 找全记录并算对校验和。
- [ ] `FS_PASS` + `ALL_PASS`：`mkfs→create→ls→读写` 全绿，根目录能列出所有文件名。
- [ ] 硬件路径 0 warning（`hw-v` / `hw-bsv` 任一过即过）。
- [ ] 能讲清「文件 / 目录 / 分区都是块设备上的字节约定」（essay 思考账本）。

## 4. 思考题（`essay/THINKING.md` 作答即可通过）

1. (b) 把 key 当短文件名、(c) 把名字塞进 `DirEntry`——「文件名」本质是什么？为什么真实
   文件系统要把「名字→inode」（目录项）和「inode→数据块」（inode 表）分两层而不是一张大哈希表？
2. `mkfs` / `mount` / `dd` 各自的职责边界？「格式化一个 img」与「把 img 刷进块设备」为什么
   是两件独立的事？换成真实磁盘时，分区表、文件系统、引导扇区谁先谁后？
3. 我们的块设备是 RAM 模型（断电即失）。要「掉电不丢已写入的文件」，软件（日志/写屏障）与
   硬件（持久化介质 + 写完成语义）分别要补什么？这与 (a) 里「写完立即能读到」的内存语义差在哪？

## 5. 引申（不计入必修）

- inode 只留 direct 块映射 → 一/二级间接块；单层根目录 → 多级路径解析与 `.`/`..`。
- 无 block cache / 无并发锁 → rcore 的 `BlockCache` + `Mutex`。
- 4 项极简分区表 → 完整 MBR/GPT；本课自洽 `mkfs` → Linux 原生 `mkfs.ext2` + `mount` + `dd` 刷写闭环。
