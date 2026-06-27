# 正经·S7 · 文件系统：块设备 + 内核内简易 FS（RAM 盘）

> 承接 S6（块/字符驱动）。本课在一块 **RAM 盘**上，从「按块读写」一路搭出「能 mkfs、能
> create/write/read/lookup、能 ls」的最小文件系统——体会 **文件/目录都是块设备上的字节约定**，
> 抽象自 rcore easy-fs，但全在内存里跑。

## 0. 这节课在讲什么

三层自底向上：

1. **块设备**（`blockdev.c`，给定）：内核静态数组 `g_disk[NBLOCKS][BSIZE]` 当 RAM 盘，
   `bread(blk,buf)` / `bwrite(blk,buf)` 按块搬运。上层 FS 只依赖这两个原语——底层换成
   virtio-blk MMIO 就能跑真硬件。
2. **简易 FS**（`fs.c`）：盘上布局 = 超级块(block0) + inode 区 + 数据区（bump 分配）。
   inode 仅用直接块映射；目录是一串 32 字节目录项 `dirent{ inum; name[28] }`。
3. **harness**（`main.c`，给定，勿改）：块设备读写自检 → mkfs + 建 3 文件写读校验 → ls。

## 1. 你要实现的（`kernel/fs.c` 两处 TODO）

- **`ialloc(type)`** — 分配一个空闲 inode：扫描 `inum=0..ninodes`，`iread` 出 dinode，
  找 `type==T_FREE` 的占用（清零、置 `type`、`iwrite` 写回），返回 inum；满了返回 -1。
- **`dir_lookup(dir_inum, name)`** — 目录按名查找：目录数据是 dirent 数组，项数 =
  `dir.size/sizeof(dirent)`，第 i 项在直接块 `addrs[i/DPB]` 的第 `i%DPB` 槽；
  `bread` 该块、`kmemcpy` 出 dirent、`kstreq(de.name,name)` 命中则返回 `de.inum`；查无返回 -1。

`iread/iwrite/balloc/dir_add/kmemcpy/kmemset/kstreq` 都已给好，直接用。

```
make -C kernel run        # OpenSBI banner 后见内核输出
```

判据：输出依次含 `BLK_PASS` / `FS_PASS` / `LS_PASS`，全过打印 `ALL_PASS`；不出现 `FAIL`。
未填空时块设备自检照样通过（`BLK_PASS`），但文件系统不通过——这就是你要点亮的部分。

## 2. 完成标准 (DoD)

- [ ] `BLK_PASS`：块设备写进去再读出来逐字节相等。
- [ ] `FS_PASS`：mkfs → 建 3 个文件 → 写入内容 → `lookup` 命中 → 读回一致；查不存在的名字返回 -1。
- [ ] `LS_PASS` + `ALL_PASS`：`ls /` 列出全部 3 个文件名各一次。
- [ ] 能说清「文件/目录都是块设备上的字节约定」「名字→inode→数据块」两层映射。

## 3. essay（`essay/THINKING.md`）

回答块设备抽象边界、为何分两层映射、RAM 盘如何做到掉电不丢三题；删除占位行即完成。

## 4. 引申

- inode 加间接块（large file）；inode/数据 bitmap 取代 bump 分配 + 支持删除回收；
- 多级目录（目录里再放目录）；把 RAM 盘换成 virtio-blk MMIO 真驱动；崩溃一致性（日志）。
