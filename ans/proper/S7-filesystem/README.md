# 正经·S7 · 文件系统：块设备 + 内核内简易 FS（RAM 盘）· 参考解

> 与 `exercises/proper/S7-filesystem` 同构；此处 `kernel/fs.c` 的 `ialloc` 与 `dir_lookup`
> 已实现。三层：块设备 RAM 盘 → 简易 FS（超级块/inode/目录项）→ harness 自检。

## 盘上布局（块为单位，`BSIZE=512`，`NBLOCKS=128`）

```
block 0            超级块 superblock{ magic,nblocks,ninodes,inode_start,data_start,next_data }
block 1..4         inode 区：NINODES=32 个 dinode，每块 IPB=8 个（INODE_SIZE=64）
block 5..          数据区：bump 分配（next_data 递增）
```

- `dinode{ type; size; addrs[NDIRECT=14] }`：仅直接块映射，单文件 ≤ 14*512 = 7KB。
- 目录数据 = `dirent{ inum; name[28] }` 数组，每项 32B，每块 DPB=16 项。
- 根目录 = inode 0（mkfs 时 `ialloc(T_DIR)` 分得）。

## 两个核心函数

- `ialloc(type)`：线性扫 inode 表找 `T_FREE`，占用并写回，返回 inum。≈ easy-fs 的 inode 位图分配。
- `dir_lookup(dir,name)`：遍历目录项、`kstreq` 比名字。≈ easy-fs `DiskInode::find`。

其余 `fs_mkfs/fs_create/fs_write/fs_read/fs_lookup/fs_ls`、`balloc/dir_add/iread/iwrite` 均已给。

## 自验证

```
make -C kernel kernel.elf
timeout 15 qemu-system-riscv64 -machine virt -nographic -bios default -kernel kernel.elf > /tmp/q.txt 2>&1
grep -aE "BLK_PASS|FS_PASS|LS_PASS|ALL_PASS" /tmp/q.txt
make -C kernel clean
```

期望依次出现 `BLK_PASS` / `FS_PASS` / `LS_PASS` / `ALL_PASS`，qemu 经内核 `k_shutdown` 正常退出。

## 计分

`require=1`：软件 C 变体（`kernel-c`）打全 4 个 PASS 即必修达成。essay 变体独立判，作辅助账。
