# S7b VFS · 思考题（参考解）

## 1. VFS 是「一层间接」，它换来了什么？与 S7 的单一具体 FS 对照。

S7 里调用方直接调那一个 FS 的函数（`fs_read` 内部就是块设备 + inode），调用方与实现硬绑死：想
再支持第二种 FS，就得在每个调用点写 `if (是 ramfs) ... else if (是 devfs) ...`。VFS 在中间插一层
`vnode + vfs_ops`：调用方只认 `vfs_read(vn,...)`，`vn->ops` 指向哪套实现就落到哪——

> "All problems in computer science can be solved by another level of indirection."

这层间接换来三件事：① **可扩展**——新增 FS 只是再实现一份 `vfs_ops` 并 `vfs_mount` 上去，调用方
零改动；② **统一命名空间**——ramfs 与 devfs 被拼进同一棵路径树（`/` 与 `/dev`），`open` 一视同仁；
③ **解耦**——块设备 FS、设备 FS、网络 FS、用户态 FS 共用同一套上层接口。代价是一次指针间接跳转和
一点抽象设计成本。对照 S7：S7 是「具体」，本课是「抽象 + 多态分发」。

## 2. C 里的 vtable 与 Rust trait / C++ 虚函数是同一件事吗？Linux VFS 的四元组是什么？

是同一回事的不同语法皮：
- 本课 `struct vfs_ops{ lookup; read; write; readdir; }` 是**手写的虚函数表**，`vnode->ops` 就是
  「虚表指针」，`vnode->ops->read(...)` 就是一次动态派发。
- C++ 编译器给带虚函数的类自动生成等价的 vtable 与 vptr；Rust 的 `dyn Trait` 胖指针 = `(数据指针,
  vtable 指针)`，`ops` 那一坨就是 trait 对象的 vtable。三者本质都是「数据 + 一张函数指针表」。

Linux VFS 的核心抽象是四个对象（与本课的简化版对应）：
- **super_block**：一个已挂载 FS 实例（≈ 本课一个 `mount` 项 + 其 `fs` 私有）。
- **inode**：文件的元数据/身份（≈ ramfs 的 `rinode`，本课 `vnode.priv` 指向它）。
- **dentry**：目录项，缓存「名字 → inode」并把 inode 串成目录树（本课用扁平 lookup 简化掉了）。
- **file**：进程打开的文件实例（偏移、模式等；本课 `vnode` 同时承担了它的角色）。
  每类都挂着自己的 `*_operations` 函数表——和本课 `vfs_ops` 一模一样的套路。

## 3. 挂载（mount）命名空间为什么需要「最长前缀匹配」？FUSE 把这层间接推到哪？

挂载表把「路径前缀」绑到「FS 实例」：`/` → ramfs、`/dev` → devfs。解析 `/dev/zero` 时，`/` 和
`/dev` 都是它的前缀，必须选**更长**的 `/dev`，否则会错误落到根 ramfs——这就是「最长前缀匹配」，
也是「跨过挂载点」的本质：在挂载点处把后续路径切换给另一个 FS。真实内核里这发生在逐段路径遍历中：
walk 到某目录若是挂载点，就跳到被挂 FS 的根继续走。**挂载命名空间**进一步让「同一路径在不同进程
看到不同挂载树」成为可能（容器、`mount --bind`、`chroot`）——挂载表从全局变成 per-namespace。

**FUSE（Filesystem in Userspace）** 把这层间接推到了用户态：内核侧的 `fuse` 仍实现一份 `vfs_ops`，
但每个 `lookup/read/write` 不在内核完成，而是打包成消息经 `/dev/fuse` 发给**用户进程**里的 FS 程序，
等它回答再返回。于是「写一个文件系统」不必写内核模块——sshfs、s3fs 皆然。这正是 vtable 多态的极致：
函数指针背后甚至可以是另一个地址空间里的进程。对照本课：我们的 `ops` 指向同进程的内核函数，FUSE 的
`ops` 指向「发消息给用户态」的桩——抽象接口不变，实现被搬到了进程边界之外。
