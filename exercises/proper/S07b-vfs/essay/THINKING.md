# S07b VFS · 思考题

> 在每题下作答（替换占位行）。答案非空且命中关键字即过。
> 删除下面这行后本 essay 才算完成：
> LABCTL_ESSAY_TODO 请作答后删除本行

## 1. VFS 是「一层间接」，它换来了什么？与 S07 的单一具体 FS 对照。

（提示：S07 调用方直接捅到那一个 FS；VFS 插入 `vnode + vfs_ops` 后，新增 FS 只是再实现一份 ops 并
mount，调用方零改动；统一命名空间；解耦。代价是一次指针间接。"another level of indirection"。）

你的作答：

## 2. C 里的 vtable 与 Rust trait / C++ 虚函数是同一件事吗？Linux VFS 的四元组是什么？

（提示：`vnode->ops` = 虚表指针；C++ vptr/vtable、Rust `dyn Trait` 胖指针都是「数据 + 函数指针表」。
Linux VFS：super_block / inode / dentry / file，各挂一套 `*_operations`。）

你的作答：

## 3. 挂载（mount）命名空间为什么需要「最长前缀匹配」？FUSE 把这层间接推到哪？

（提示：`/dev/zero` 的前缀里 `/dev` 比 `/` 长，必须选长的才能跨过挂载点落到 devfs；挂载命名空间让
同一路径在不同进程看到不同挂载树（容器/bind）。FUSE：内核侧 ops 把请求经 `/dev/fuse` 转发给用户态
FS 进程——函数指针背后甚至是另一个地址空间。）

你的作答：
