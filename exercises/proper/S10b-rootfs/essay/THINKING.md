# S10b initramfs · 思考题

> 在每题下作答（替换占位行）。答案非空且命中关键字即过。
> 删除下面这行后本 essay 才算完成：
> LABCTL_ESSAY_TODO 请作答后删除本行

## 1. 把 initramfs 启动流走一遍：内核 → 解包 → /init → 用户态

（提示：内核建空根 fs → 解 cpio 灌文件 → 执行 /init 即第一个用户态进程 PID 1 →
`sret` 跌入 U 态、之后只能经 ecall 求服务。哪一步是「交棒」？）

你的作答：

## 2. newc cpio 格式：为什么用它当 initramfs 容器？

（提示：110B 定长 ASCII 头 + 名字 + 4B 对齐 + 数据 + 4B 对齐，`TRAILER!!!` 收尾；
纯顺序、自描述、无需 seek、不依赖块设备/fs 驱动。）

你的作答：

## 3. 为什么要 initramfs？真盘/驱动还没就绪时它扮演什么角色？

（提示：根盘驱动/工具常躺在根盘里 → 鸡生蛋；initramfs 是与内核一起加载、解进内存即用的
临时根，自带驱动与 /init，用完即弃。）

你的作答：

## 4. pivot_root / switch_root：从临时根切到真根

（提示：/init 挂好真根后把它变成 `/`、再 exec 真 `/sbin/init`；根可在运行中被替换。）

你的作答：

## 5. initramfs vs initrd；busybox init 的角色

（提示：initrd 是块设备镜像、需 fs 驱动、挂载使用；initramfs 是 cpio、解包到 tmpfs、用 /init。
busybox 按 argv[0] 一身多职，几百 KB 给一套 userland。）

你的作答：

## 6. 对照 S10：直接内嵌程序 vs 从 rootfs 载入

（提示：S10 把用户程序编进内核镜像直接跳；S10b 多了「解包→建 fs→查找→读字节→运行」一层间接，
正是真实系统形态。ELF 加载是下一步。）

你的作答：
