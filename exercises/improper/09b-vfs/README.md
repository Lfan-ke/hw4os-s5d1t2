# 09b · VFS：多文件系统共存于同一组接口

> 不正经赛道 · 第 9b 课 —— 纯软件心智模型，host 直接跑（rust / c 双语言）。
> 一句话母题：**VFS = 让 ext / nfs / proc / devfs 各不相同的文件系统，
> 全长成同一张 `open/read/write` 脸；调用者只递一个路径，VFS 按挂载表把它路由到正确的 FS。**

## 0. 这节课在讲什么

第 08 课你做了「一个具体文件系统」（块 → inode → 目录），第 09 课你做了「文件即接口」（一切皆 `read/write`）。
现在的问题是：一台机器上**同时**装着好几种文件系统——根盘是 ext4、`/mnt/nfs` 是网络盘、`/proc` 是内核虚构的、`/dev` 是设备——
`cat /etc/passwd` 和 `cat /proc/cpuinfo` 用的是**同一个** `read` 系统调用，凭什么能落到两套完全不同的代码上？

答案就是 **VFS（Virtual File System，虚拟文件系统）**：内核在「系统调用」与「各个具体 FS」之间插一层**抽象接口**。
Sun Microsystems 1986 年为了把 NFS 接进 Unix 而发明它——与其改一万处 `read`，不如让所有 FS 都实现同一组方法，
VFS 负责按路径把调用**分派**到对的那个 FS。这正是「面向接口编程」在内核里的样板。

**VFS 四大对象**（本课的极简对应）：

| VFS 对象 | 真实含义 | 本课对应 |
| :-- | :-- | :-- |
| **superblock** | 一个挂载好的 FS 实例（整盘元信息） | 一个 `FileSystem` 实例（`RamFs`/`DevFs`） |
| **inode** | 一个文件节点（元数据 + 数据指针） | 一个**节点号** `usize`/`int` |
| **dentry** | 「名字 → inode」的目录项缓存 | 各 FS 的 `lookup(rel) -> node` |
| **file** | 进程打开后的句柄（fd 背后） | `OpenFile{挂载下标, 节点号}` |

本课造两个 mock 文件系统，**背后行为天差地别、对外接口一模一样**：

1. **RamFs**：内存里几个**真文件**（`/hello`、`/motd`），写得进、读得出（有存储）。
2. **DevFs**：两个**虚拟设备**——`/null` 吞掉一切写入、读出为空；`/zero` 读出全 0（无存储，纯副作用，正是 `/dev/null` + `/dev/zero`）。

VFS 维护一张**挂载表**：`RamFs` 挂在 `/`，`DevFs` 挂在 `/dev`。
于是 `open("/hello")` 落到 RamFs，`open("/dev/zero")` **跨过挂载点** `/dev` 落到 DevFs——同一个 `open`，被路由到不同 FS。

## 1. 你要填的 2 处

软件在 `sw/rust/src/main.rs` 或 `sw/c/vfs.c`，两语言同构。其余（FS 行为、路径工具 `is_under`/`subpath`、测试 harness）都给好，勿改。

| 填空 | 位置 | 要求 | 关联判据 |
| :-- | :-- | :-- | :-- |
| (1) 挂载路由 | `Vfs::resolve` / `vfs_resolve` | 在挂载表里找**最长**匹配的挂载前缀，返回 (挂载下标, FS 内相对路径) | `MOUNT_PASS` / `DISPATCH_PASS` |
| (2) 名字解析 | `RamFs::lookup` / `ramfs_lookup` | 顺序扫文件表，名字相等返回它的下标当节点号，找不到返回 `None`/`-1` | `VFS_PASS` |

### 模型约定（判题用）

- **最长前缀路由**：路径 `/dev/zero` 同时落在 `/`（长 1）和 `/dev`（长 4）之下。
  必须选**更长**的 `/dev`，才能跨过挂载点进入 DevFs；否则会停在根的 RamFs（DISPATCH 错位）。
  已给 `is_under(mount, path)`（判前缀 + 边界，避免 `/dev` 误配 `/device`）和
  `subpath(mount, path)`（剥掉挂载点得 FS 内相对路径，根挂载返回整路径）——你只写「挑最长那条」的循环。
- **名字解析**：`RamFs` 的文件表是 `[("/hello", ...), ("/motd", ...)]`，`lookup("/hello")` 返回下标 `0`。
- **open = 路由 + 解析**：`open(path)` 先 `resolve` 路由到 FS，再让该 FS `lookup` 解析名字，**两步都成**才算打开成功。
  所以 `/dev/missing` 能路由到 devfs 但 `lookup` 失败（路由 ≠ 存在）。

```
labctl run improper/09b-vfs      # 跑 rust / c 两条路径
labctl watch                     # 边改边自动判定
labctl hint improper/09b-vfs     # 卡住看提示
```

## 2. 判据与完成标准 (DoD)

判题靠输出子串：`VFS_PASS`（经统一接口 open/read 一个 ramfs 文件）+ `MOUNT_PASS`（挂第二个 FS，路径跨过挂载点进入它）
+ `DEVFS_PASS`（/dev/null 吞写、/dev/zero 读全 0）+ `DISPATCH_PASS`（操作按路径路由到正确 FS）+ 末尾 `ALL_PASS`；
`forbid=["FAIL","panic","ERROR"]`。

- [ ] 至少一条变体打印 `VFS_PASS` / `MOUNT_PASS` / `DEVFS_PASS` / `DISPATCH_PASS` / `ALL_PASS`，无任何 `*_FAIL`/`*_BAD`（必修，`require=1`）。
- [ ] `open("/hello")` 经**统一接口**读出内容；`open("/dev/zero")` 跨过 `/dev` 落到 DevFs。
- [ ] `/dev/null` 吞写读空、`/dev/zero` 读出全 0——同一 `read/write` 接口背后两种 FS 行为。
- [ ] rust 与 c 对同一向量行为一致；rust `cargo run -q` 0 warning。
- [ ] 能一句话说清「VFS = 多 FS 共存于统一接口 + 按路径分派」，并完成 essay。

## 3. 引申（从模型到真实）

- 真实 Linux VFS：`sys_read` → `vfs_read` → `file->f_op->read_iter`（一张 `struct file_operations` 函数指针表）→
  ext4 / nfs / proc 各自的实现。本课的 `FileSystem` trait / 函数指针表就是它的玩具版。
- 真实 `mount(2)` 把一个 superblock 挂到目录树某点，内核维护挂载点；路径查找（`path_lookup`）逐段走、遇到挂载点就跨进子 FS——
  本课的「最长前缀路由」是它最朴素的影子。
- **「一切皆文件」如何延伸**：靠 `devfs`/`procfs`/`sysfs` 这类**虚拟 FS**。它们没有真磁盘，
  `read/write` 背后是设备副作用或内核状态——`/proc/cpuinfo` 读出的是临时拼的字符串，`/dev/zero` 读出的是凭空生成的 0。
  正因为都走 VFS 那张统一接口，`cat` 才能既读真文件又读它们。
- 对照：**08-filesystem**（造**一个**具体 FS：块/inode/目录）是 VFS 下面的一块拼图；
  **09-abstract-file**（文件即 `read/write` 接口）是 VFS 对外那张脸的单点。本课把多块拼图收进同一张脸下，并补上「按路径分派」。

## 4. 思考题（essay，`essay/THINKING.md` 作答即过）

1. **为什么需要 VFS**：如果没有 VFS，内核想多支持一种文件系统要改多少地方？VFS 把「可变的部分」收敛到哪个接口后面？这和应用层「面向接口编程 / 多态」是不是一回事？
2. **四大对象**：superblock / inode / dentry / file 各是什么、谁缓存谁？为什么打开同一个文件两次会有两个 `file` 但共享一个 `inode`？对照本课的极简映射说一遍。
3. **mount 与命名空间**：`mount` 把一棵 FS 子树嫁接到目录树某点，路径跨过挂载点就进了另一个 FS。这和容器的 mount namespace（每个容器一张私有挂载表）是什么关系？为什么「最长前缀」是路由的关键？
4. **一切皆文件的延伸**：`/proc`、`/sys`、`/dev` 这些「假文件系统」凭什么也能 `cat`？它们的 `read` 背后没有磁盘块，那读出来的字节从哪来？把 devfs/procfs 和本课的 DevFs（读出凭空生成的 0）联系起来谈一段。
