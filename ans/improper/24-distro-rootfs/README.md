# 24 · 发行版与根文件系统：从光秃秃的内核到能进 shell 的 rootfs

> 不正经赛道 · 第 24 课 —— 纯软件心智模型，host 直接跑。
> 一句话母题：**内核只是引擎，要一个根文件系统(rootfs) 装上 init + 工具，开机才进得了 shell——这就是「发行版」的雏形。**

## 1. 这节课在讲什么

刚编译出来的内核光秃秃的：它能初始化 CPU、调度、管内存，却**没有任何用户态程序可跑**。
开机最后一步「执行 `/sbin/init`」会失败——因为根本没有 `/`，没有 `/sbin`，没有 init。
所以发行版的本质，是在内核之外**额外准备一整套 userspace**：一棵符合约定的目录树
（rootfs），里面有 init、有 shell、有工具、有库。内核挂上它、跑起 `/init`，你才看到提示符。

本课在一棵「内存文件树」（路径字符串 → 节点）上，把这套 rootfs 一步步搭出来，四段递进：

1. **(a) FHS 目录树**：`/bin /etc /dev /proc /sys /lib` 各就各位，放进 busybox 和符号链接。
2. **(b) busybox 多合一二进制**：一个程序，按 `argv[0]` 分发成 `ls`/`cat`/`echo`/`mount`。
3. **(c) mock init（PID 1）**：挂 `/proc` `/sys`、读 `/etc/inittab`、spawn 一个 shell。
4. **(d) cpio 打包 / 解包**：把整棵树压成一段字节归档，再解开还原（这就是 initramfs）。

## 2. 你要实现什么

文件树节点三态：`Dir` / `File(字节)` / `Link(目标)`。四段判据与你要填的两处：

| 文件 | 函数 | 判据 |
| :-- | :-- | :-- |
| `sw/{rust,c}` | `build_rootfs`（已给） | 六大目录 + busybox + 4 个符号链接 → `FHS_PASS` |
| `sw/{rust,c}` | **`busybox_main`（你填）** | 同一函数按 `argv[0]` 分发出 4 个命令 → `BUSYBOX_PASS` |
| `sw/{rust,c}` | `mock_init`（已给） | PID 1 → 挂载 `/proc` `/sys` → 起 shell → `INIT_PASS` |
| `sw/{rust,c}` | **`cpio_unpack`（你填）** | 打包→解包还原整棵树逐项相等 → `INITRAMFS_PASS` |

四段皆过再打印 `ALL_PASS`。两处填空可二选一实现：
busybox `// TODO[a]` 直接 `match`/`if` 分发 / `// ELSE[b]` 查一张 `name→fn` 表；
cpio `// TODO[a]` 线性逐条解析 / `// ELSE[b]` 先扫描索引再回填。

cpio 记录布局（自定义、易解析）：

```
magic[4]="0707" | type(1: 'd'/'f'/'l') | namelen(LE32) | bodylen(LE32) | name | body
... 末尾一条 name=="TRAILER!!!" 的哨兵记录表示归档结束 ...
```

```
labctl run improper/24-distro-rootfs     # 跑全部变体
labctl watch                             # 边改边自动判定
labctl hint improper/24-distro-rootfs    # 卡住看提示
```

## 3. 完成标准 (DoD)

- [ ] `FHS_PASS`：rootfs 六大目录就位，applet 是指向 busybox 的符号链接（已给，编译即过）。
- [ ] `BUSYBOX_PASS`：同一个 `busybox_main`，仅凭 `argv[0]` 就分发出 `ls`/`cat`/`echo`/`mount`。
- [ ] `INIT_PASS`：init 宣告 PID 1 → 读 inittab → 挂 `/proc` `/sys` → 在挂载之后起 shell。
- [ ] `INITRAMFS_PASS` + `ALL_PASS`：cpio 打包→解包，还原出的树与原树**逐项相等**。
- [ ] 能讲清「为什么内核之外还要一个 rootfs / 发行版」（essay 思考账本）。

## 4. 引申（不计入必修）

- 单层扁平 `路径→节点` 映射 → 真正的目录树（inode + dentry，多级路径解析）。
- 我们的 busybox 只有 4 个 toy applet → 真实 busybox 三百多个 applet，仍只一份二进制。
- mock init 只跑一遍 inittab → systemd 的依赖图 / target / 服务监督与 `respawn`。
- 内存文件树「断电即失」→ initramfs 解到 tmpfs 后 `switch_root` 到真实磁盘根。
- 手搓 rootfs → buildroot（小巧、整树重编）/ Yocto（可裁剪的发行版工厂）/ Debian/Arch 的包管理装出根。
