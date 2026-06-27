# 思考题参考解 · 发行版与根文件系统

> 判题只看「答案非空 + 命中关键字」。下面是参考答案，写下你自己的理解即可。
> 自检：软件路径已依次打印 FHS_PASS / BUSYBOX_PASS / INIT_PASS / INITRAMFS_PASS，并以 ALL_PASS 收尾。

## 1. 为什么内核之外还要一个 rootfs？「发行版」到底是什么？

内核只是引擎：它会初始化硬件、建立调度/内存/文件系统**机制**，但它**不自带任何用户态程序**。
开机最后一步是「执行第一个用户进程」（传统是 `/sbin/init`）——这一步要求磁盘上**已经有一个根**
（`/`）、有 init、有它依赖的库和工具。没有 rootfs，内核启动到这一步只能 panic（"No init found"）。

所以 **rootfs = 内核能跑起来的最小 userspace 世界**：FHS 目录树 + init + shell + 工具 + 共享库。
而**发行版（distro）= 内核 + 一整套配好的 rootfs + 包管理 + 默认配置**。同一个 Linux 内核，
配 Debian 的 rootfs 就是 Debian，配 Alpine 的就是 Alpine。区别几乎全在「userspace 怎么装、装什么」。
这也解释了 (a)：rootfs 的第一件事就是把 `/bin /etc /dev /proc /sys /lib` 这些**约定的家**摆好——
FHS（Filesystem Hierarchy Standard）让「可执行去 /bin、配置去 /etc、库去 /lib」成为跨发行版的共识。

## 2. busybox 多合一二进制为什么省空间？init=PID 1 又意味着什么？

**busybox 多合一**：把几百个命令（ls/cat/echo/mount/sh…）编进**同一个二进制**，靠 `argv[0]`
（被调用的名字）在入口分发——`/bin/ls`、`/bin/cat` 全是指向 `/bin/busybox` 的**符号链接**。
省空间的原因有三：
- **代码只存一份**：所有 applet 共享同一个 ELF、同一份 libc 启动代码、同一套公共函数，
  不像 coreutils 那样每个命令一个独立可执行文件、各自重复链接一遍。
- **链接/重定位开销只付一次**，符号链接近乎零成本（只是目录项里一根指向 busybox 的线）。
- 对嵌入式/initramfs 这种「几 MB 就要塞下整套工具」的场景，这是决定性的。
对应 (b)：`busybox_main` 仅凭 `argv[0]` 的 basename 就变出不同命令——同一段代码，多张面孔。

**init = PID 1**：内核创建的第一个用户进程，PID 永远是 1，是所有进程的祖先。它特殊在：
- 它读 `/etc/inittab`（或 systemd 的 unit），按 `sysinit` 动作挂载 `/proc` `/sys` 等虚拟文件系统，
  再 spawn `getty`/shell（对应 (c) 的 `askfirst`）。开机序列必须「先挂载、后起 shell」。
- 它**收养孤儿进程**并负责 `wait` 回收僵尸；它**不能退出**——PID 1 一死内核就 panic。
- 传统 SysV init 跑脚本、按 runlevel；systemd 用依赖图并行启动、监督服务、`respawn` 重启崩溃的服务。
  二者职责相同：把「裸内核」接力成「能登录、有服务」的系统。

## 3. initramfs/initrd 是什么？buildroot / Yocto / 发行版各在解决什么？

**initramfs（早期 initrd）**：内核镜像里（或紧随其后）打包一段 **cpio 归档**——就是 (d) 做的事：
把一棵文件树序列化成「头 + 名字 + 内容」的记录流，末尾一条 `TRAILER!!!` 哨兵收尾。内核启动时把它
**解包到内存里的 tmpfs**，当作**临时根文件系统**，然后执行其中的 `/init`。它的存在是为了解决
「**鸡生蛋**」：真正的根文件系统可能在 LVM/RAID/加密盘/网络盘上，要先有驱动和工具才能挂上它——
这些驱动/工具就先放进 initramfs，跑起来后再 `switch_root` 切到真实磁盘根。cpio 选型是因为它格式极简、
**流式可解析、不需要预先 mkfs**，内核解包逻辑可以做得很小。

**手搓 → 工具链**这条谱系：
- **手搓 rootfs**：像本课一样一个个建目录、塞 busybox、写 inittab——理解原理，但不可维护、不可复现。
- **buildroot**：一套 Makefile/Kconfig，从源码**整树交叉编译**出一个紧凑 rootfs（+ 可选内核 + 镜像）。
  适合「固定的小设备」，改一点常要重编整棵；产物小、可复现。
- **Yocto/OpenEmbedded**：「**造发行版的工厂**」——用 recipe/layer 描述每个包怎么建，输出**可裁剪、可升级、
  带包管理**的自定义发行版。比 buildroot 重、灵活度和可维护性高，适合产品线长期演进。
- **Debian/Ubuntu/Arch 等发行版**：不自己编整棵树，而是**预编译二进制包 + 包管理器（apt/pacman）**，
  用户按需 `install` 把 rootfs「拼」出来。面向通用桌面/服务器，强调生态与升级，而非极致裁剪。

一句话收束：内核给的是机制，rootfs 给的是「能用的世界」；从手搓到 buildroot 到 Yocto 到发行版，
是同一件事在「可复现 / 可裁剪 / 可维护 / 可升级」四个维度上不断工程化的过程。
