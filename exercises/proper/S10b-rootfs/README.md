# 正经·S10b · initramfs：解开内嵌 cpio → 灌进 RAM-fs → 交棒 /init

> 承接 S7（块设备 + inode/目录 RAM-fs）与 S8（U 态 + ecall syscall）。
> 本课把两条线接起来，演一遍真实内核启动尾声那一幕：**内核如何把棒子交给用户态**——
> 解开一段 **initramfs**（内嵌的 cpio 归档），把里面的文件灌进根文件系统，
> 在 fs 里找到 **/init**，载入并在 U 态把它跑起来。

## 0. 这节课在讲什么

Linux 启动到最后，内核手里只有一段被 bootloader 一起加载进内存的 **initramfs**——
一个 cpio 归档。内核把它解开成一棵临时根目录树（`/init`、`/etc`…），然后执行 `/init`，
**第一个用户态进程**就此诞生，启动流从内核交棒到用户态。本实验把这一幕做成最小骨架：

1. `fs_mkfs()`（承 S7，已给）：先有一张空的 RAM 根文件系统。
2. `cpio_unpack()`：逐条解析内嵌的 newc cpio 归档，把每个成员 `fs_create + fs_write` 灌进 RAM-fs。
3. `fs_lookup("init")`：在 fs 里定位 `/init`。
4. `load_and_run_init()`：把 `/init` 的字节读进可执行缓冲，`run_user()` 跌入 U 态运行
   （承 S8）。`/init` 是一段机器码，经 `ecall` 打印 banner、再 `exit(0)`，内核回收。

> 无分页：用户与内核同地址空间，仅特权级隔离（承 S8）。`/init` 直接是机器码而非 ELF——
> 把「从 rootfs 载入并执行」的主线打通，ELF 解析留作引申。

## 1. 你要实现的（两处 TODO）

- **`kernel/cpio.c` 的 `cpio_parse_one`** — 解析一条 newc 头：
  - newc 头 = 110 字节定长 ASCII，每字段 8 个十六进制字符；魔数 `"070701"`（守卫已给）。
  - 用已给的 `cpio_hex8(h+94)` 读 `namesize`、`cpio_hex8(h+54)` 读 `filesize`。
  - 名字 = `h+110`（含结尾 0）；数据起始 `= off + align4(110+namesize)`；
    下一条头 `= 数据起始 + align4(filesize)`。
  - 名字为 `"TRAILER!!!"`（结束哨兵）→ `return 0`；否则填好 `v` 各字段、`return 1`。
- **`kernel/main.c` 的 `load_and_run_init`** — 找 `/init` 并交棒：
  - `fs_lookup("init")` → `fs_read` 进 `init_exec` → `run_user((uint64_t)init_exec, 用户栈顶)`。

`cpio_hex8/align4`、`cpio_unpack` 循环骨架、`fs_*`、`run_user/return_to_kernel`、内嵌
`initramfs_cpio[]` 与 harness 均已给。

```
labctl run proper/S10b-rootfs
make -C kernel run      # 手动跑（OpenSBI banner 后见内核输出）
```

判据：输出依次含 `CPIO_PARSE_PASS` / `POPULATE_PASS` / `INIT_FOUND_PASS` /
`USERSPACE_PASS` / `ALL_PASS`，且 `/init` 在 U 态打印自己的 banner；
不出现 `UNEXPECTED_*` / `FAIL` / `panic`。占位版能编译、能跑、全 `*_MISS`，不崩。

## 2. 完成标准 (DoD)

- [ ] `cpio_parse_one` 正确解出 namesize/filesize/name/data/next，`TRAILER!!!` 处停止。
- [ ] 4 个成员（README/etc_config/motd/init）被灌进 RAM-fs，文本内容可逐字节读回。
- [ ] `fs_lookup("init")` 命中，`/init` 被读进缓冲并在 U 态运行、经 `ecall` 打印 banner、`exit(0)`。
- [ ] 四道判据齐过、`ALL_PASS`、qemu 经 `k_shutdown` 正常退出。
- [ ] 能说清：为什么用 initramfs（真盘/驱动就绪前的临时根）、它与 initrd / pivot_root 的关系。

## 3. 引申

- **newc 格式细节**：定长 ASCII 头、4 字节对齐、`TRAILER!!!` 收尾、硬链接用 `c_ino` 合并。
- **initramfs vs initrd**：前者是 tmpfs 上解开的 cpio（无需块设备/文件系统驱动），后者是块设备镜像。
- **pivot_root / switch_root**：`/init` 把真正的根挂到别处后切换根、再 `exec` 真 `/sbin/init`（如 systemd/busybox）。
- **ELF 加载**：真 `/init` 是 ELF，需解析 program header、按段映射；本实验用扁平机器码绕开（接 S8b/mmap）。
- **对照 S10**：S10 把用户程序直接编进内核镜像；本课从 rootfs 里**载入**——多了「解包→建 fs→查找→载入」一层间接，正是真实系统的形态。
