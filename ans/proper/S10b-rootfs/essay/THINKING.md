# S10b initramfs · 思考题（参考解）

## 1. 把 initramfs 启动流走一遍：内核 → 解包 → /init → 用户态

bootloader 把内核与一段 **initramfs**（cpio 归档）一起加载进内存。内核启动到尾声时：

1. 在内存里建一张空的根文件系统（真实 Linux 是 **tmpfs/rootfs**；本实验是 S7 的 RAM-fs）。
2. **解开 cpio**：逐条读头 → 在根 fs 里 `create` 同名文件 → 把数据 `write` 进去
   （Linux 在 `init/initramfs.c` 的 `unpack_to_rootfs`；本实验是 `cpio_unpack`）。
3. 内核执行根目录下的 **`/init`**——这是**第一个用户态进程**（PID 1）。控制权由 S 态内核
   经 `sret` 跌入 U 态（本实验 `run_user`），启动流就此**交棒**给用户态。
4. `/init` 在 U 态只能经 `ecall`（syscall）求内核办事：打印、挂载、再 `exec` 真正的 init。

一句话：**内核负责把第一棒（/init）放进用户态的手里，之后的用户空间初始化由 /init 接管。**

## 2. newc cpio 格式：为什么用它当 initramfs 容器？

newc（"070701"）是 SVR4 cpio 的一种：归档 = 若干记录首尾相接，每条记录 =
**110 字节定长 ASCII 头** + 名字（含结尾 0）+ 4 字节对齐 + 数据 + 4 字节对齐。头里全是
8 字符十六进制字段（ino/mode/uid/gid/nlink/mtime/**filesize**/dev.../**namesize**/check），
归档以名为 `TRAILER!!!` 的空记录收尾。

适合当 initramfs 容器的原因：**纯顺序、自描述、无需 seek、无目录索引区**——解包器从头到尾
扫一遍就能把整棵树建出来，不依赖任何块设备/文件系统驱动（解析逻辑全是「读头、按长度跳」）。
全 ASCII 头还便于跨架构、跨字节序，连接器/`gen_init_cpio` 都能轻松生成。

## 3. 为什么要 initramfs？真盘/驱动还没就绪时它扮演什么角色？

鸡生蛋问题：要挂真正的根文件系统，得先有**根盘的驱动**（NVMe/SATA/virtio-blk）、
可能还要 LVM/RAID/加密/网络盘的用户态工具。但这些驱动/工具往往**以模块形式躺在根盘里**——
没挂上根就读不到，读不到就挂不上。

initramfs 打破死循环：它是一段**与内核一起加载、解进内存就能用**的临时根，自带必要的
驱动模块与 `/init` 脚本。`/init` 在这个临时根里加载驱动、组装/解密/探测真正的根设备，
挂好后再切换过去。**它是「真盘就绪前的脚手架」**——用完即弃。

## 4. pivot_root / switch_root：从临时根切到真根

`/init` 探测并挂好真正的根（如挂到 `/mnt/root`）后，要把它**变成 `/`**：

- `pivot_root(new, old)`：把当前进程的根换成 `new`，旧根挂到 `old` 下，随后可卸载旧根。
- initramfs 实践里常用 **`switch_root`**：删掉 initramfs 内容、把 `new` 移成 `/`、再
  `exec` 真根上的 `/sbin/init`（systemd/OpenRC…），PID 1 就地被替换为真正的 init。

要点：**根可以在运行中被替换**，临时根的内存随后释放，干净交棒给落盘的真实根。

## 5. initramfs vs initrd；busybox init 的角色

- **initrd（旧）**：一个**块设备镜像**（如 ext2.img），内核当成 ramdisk 挂载，需要对应
  文件系统驱动，且占一块固定大小的 ram 盘，用 `/linuxrc`。
- **initramfs（新）**：一个 **cpio 归档**，内核**解包到 tmpfs**，不需块设备/fs 驱动、
  大小随内容增减、用 `/init`。今天默认走 initramfs。
- **busybox init**：嵌入式/救援环境里，`/init` 常是个 busybox——一个多合一二进制，
  按 argv[0] 充当 `sh/mount/insmod/switch_root...`，几百 KB 就给出一套可用 userland，
  正好塞进 initramfs 当第一棒。

## 6. 对照 S10：直接内嵌程序 vs 从 rootfs 载入

- **S10**：用户程序被**直接编进内核镜像**（`user_main` 等就是内核 `.text` 里的函数），
  `run_user` 跳过去即可——没有「文件」概念，程序与内核同一次链接。
- **S10b**：用户程序作为**文件**躺在 initramfs 里，内核要**解包 → 建进 fs → 按名查找 →
  读字节进缓冲 → 才能运行**。多出来的这层间接（归档→文件系统→载入）正是真实系统的形态：
  内核与用户程序解耦、可独立更新、可有任意多个文件。本实验的 `/init` 用扁平机器码绕开了
  ELF 解析；补上 ELF program header 映射，就是通向 `execve` 的下一步（接 S8b/mmap）。
