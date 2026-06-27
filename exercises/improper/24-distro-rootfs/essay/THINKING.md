# 思考题 · 发行版与根文件系统（在此作答）

> 判题只看「答案非空 + 命中关键字」。把你的理解写在每题下面即可。
> 完成后，请在文末补上一行自检（说明软件路径已依次打印 FHS / BUSYBOX / INIT / INITRAMFS
> 各自的 PASS 并以 ALL 收尾），让判题能识别到你已跑通——具体串名见 README §2。

## 1. 为什么内核之外还要一个 rootfs？「发行版」到底是什么？

（在此作答：内核只有机制、不自带任何用户态程序；开机最后一步要执行 /sbin/init，没有 rootfs 就 panic；
rootfs = 内核能跑起来的最小 userspace；发行版 = 内核 + 一整套配好的 rootfs + 包管理……）

## 2. busybox 多合一为什么省空间？init=PID 1 又意味着什么？

（在此作答：所有 applet 共享同一份二进制/libc/链接，符号链接近乎零成本；
init 是第一个用户进程、所有进程祖先、收养孤儿回收僵尸、不能退出、读 inittab 先挂载后起 shell；
SysV inittab/runlevel vs systemd 依赖图/respawn……）

## 3. initramfs/initrd 是什么？buildroot / Yocto / 发行版各解决什么？

（在此作答：内核里打包一段 cpio 归档，启动时解到 tmpfs 当临时根再跑 /init，解决根盘驱动的鸡生蛋；
cpio 流式可解析无需 mkfs；手搓 rootfs / buildroot 整树交叉编译 / Yocto 造发行版的工厂 / Debian 包管理拼根……）

<!-- 自检（完成后取消注释并按 README §2 的串名填写）：
软件路径已依次打印 ____ / ____ / ____ / ____ 并以 ____ 收尾。
-->

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
