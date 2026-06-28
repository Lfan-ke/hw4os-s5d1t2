# 正经·S07b · VFS：内核虚拟文件系统层（vtable 多态 + 挂载表）

> 承接 S07（单一具体 RAM-fs）。S07 里 `fs_open/fs_read` 直接捅到那一个 FS 的实现；本课在它之上
> 加一层 **VFS（Virtual File System）**——用 `vnode + vfs_ops` 这套抽象接口（C 里的 vtable）把
> 「调用方」与「具体 FS」解耦，再用 **挂载表 + 最长前缀路径解析** 把不同子树绑到不同 FS。
> 一句 `vfs_read(vn,...)` 落到 ramfs 还是 devfs，由 vnode 自带的 `ops` 决定——这就是「间接一层解千愁」。

## 0. 这节课在讲什么

```
        vfs_open("/dev/zero")              vfs_read(vn,...)
               │                                  │
        ┌──────▼───────┐  挂载表最长前缀    ┌──────▼───────┐
        │  VFS 核心    │ ── "/dev" 胜 "/" → │  vnode.ops   │ ── vtable 分发
        └──────┬───────┘                   └──────┬───────┘
         ┌─────┴─────┐                      ┌─────┴─────┐
      ramfs_ops   devfs_ops              ramfs_read  devfs_read
      （"/"）      （"/dev"）
```

三块拼起来：

1. **抽象接口**（`vfs.h`）：`struct vfs_ops{ lookup; read; write; readdir; }` 是 vtable；
   `struct vnode{ vfs_ops* ops; void* priv; type; }` 是 FS 无关的句柄，`priv` 放各 FS 私有
   （ramfs：inode 号；devfs：设备号）。
2. **两个具体 FS**（给定）：`ramfs.c`（内存 inode/目录，复用 S07 思路，预置 3 个文件）挂 `"/"`；
   `devfs.c`（`/dev/zero` 读全 0、`/dev/null` 吞写）挂 `"/dev"`。各实现一份 `vfs_ops`。
3. **VFS 核心**（`vfs.c`）：挂载表 + 路径解析 + 分发。`vfs_open` 按路径选挂载点、把相对路径
   交给该 FS 的 `lookup`；`vfs_read/write` 经 `vnode->ops` 分发。
4. **harness**（`main.c`，给定，勿改）：注册自检 → 分发自检 → 跨挂载点自检 → devfs 语义自检。

## 1. 你要实现的（`kernel/vfs.c` 两处 TODO）

- **填空 A — vtable 分发**：`vfs_open` 里已选好挂载点、造好根 vnode，补一行
  `return m->ops->lookup(&root, rel, out);`；`vfs_read` 补 `return vn->ops->read(vn, buf, max);`。
  同一句 `->ops->`，落到哪个 FS 由 vnode 携带的 `ops` 指针决定——这正是多态。
- **填空 B — `mount_resolve` 最长前缀匹配**：遍历挂载表，`path_is_prefix(path,mp)`（给定）为真的
  才是候选；在候选里挑挂载点字符串**最长**的（`"/dev"` 胜过 `"/"`，跨过挂载点）；选中后
  `rel = path + kstrlen(挂载点)`，再吃掉开头的 `'/'`，返回该 mount。

`vfs_mount / path_is_prefix / kstr*` 与两个 FS 的 ops 都已给好，直接用。

```
make -C kernel run        # OpenSBI banner 后见内核输出
```

判据：输出依次含 `VFS_REG_PASS` / `DISPATCH_PASS` / `CROSSMNT_PASS` / `DEVFS_PASS`，全过打印
`ALL_PASS`；不出现 `FAIL`。未填空时四项均为 `*_MISS`（分发返回 -1、解析返回空 → open 全失败但不崩）。

## 2. 完成标准 (DoD)

- [ ] `VFS_REG_PASS`：注册并挂载了两个 FS，挂载点恰为 `"/"` 与 `"/dev"`。
- [ ] `DISPATCH_PASS`：`vfs_open("/hello.txt")` 拿到的 vnode `ops==ramfs_ops()`，`vfs_read` 读回内容一致。
- [ ] `CROSSMNT_PASS`：`vfs_open("/dev/zero")` 解析到 devfs（`ops==devfs_ops()`），而 `"/readme"` 仍归 ramfs。
- [ ] `DEVFS_PASS` + `ALL_PASS`：`/dev/zero` 读出全 0、`/dev/null` 吞写（返回写入长度）且读即 EOF。
- [ ] 能说清「VFS 是一层间接」「vtable/trait 多态」「挂载表如何让一棵路径树跨越多个 FS」。

## 3. essay（`essay/THINKING.md`）

回答 VFS 这层间接换来什么、vtable 与 trait/虚函数的对应、挂载命名空间与 FUSE 三题；删除占位行即完成。

## 4. 引申

- 多级目录与跨挂载点的多段路径解析（逐段 lookup，遇到挂载点切换 FS）；
- 给 vnode 加引用计数与 dentry 缓存（对应 Linux 的 `dentry`/`inode` cache）；
- 把 S07 的块设备 RAM-fs 接成一个 `vfs_ops` 挂上来，与 devfs 并存；
- per-process 挂载命名空间（`mount --bind` / 容器）；用户态 FS（FUSE）把 ops 转发到用户进程。
