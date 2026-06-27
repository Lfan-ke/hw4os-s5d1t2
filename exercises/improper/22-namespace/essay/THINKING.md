# 22-namespace 思考题（essay 变体）

在每题下方写下你的理解即可（命中关键字即通过：共享内核 vs 独立内核、视图隔离/配额、
OCI/runc、PID/MNT/NET/UTS/IPC/User 等）。

## 1. namespace + cgroup = 容器。这两者各管「隔离」的哪一半？

（提示：namespace 管「看见什么」(视图/可见性)，cgroup 管「能用多少」(配额/限流)。）

TODO: 在此作答。

## 2. 容器 vs 虚拟机差在哪？为什么容器「轻」？

（提示：共享宿主内核 vs 各跑独立内核；启动开销、隔离强度、密度的取舍。）

TODO: 在此作答。

## 3. OCI 标准与 Docker 工具分别是什么？runc / containerd 在哪一层？

（提示：image-spec / runtime-spec / runc；docker→containerd→runc 的分层。）

TODO: 在此作答。

## 4. Linux 六种主要 namespace 各管什么？

（提示：PID、Mount(MNT)、Network(NET)、UTS、IPC、User，各隔离哪一类内核资源。）

TODO: 在此作答。

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
