# 16 · 驱动入门 思考题（在每题下作答即可）

> 判据：答案非空且命中关键字（如 `compatible` / `设备树` / `解耦` / `最具体优先` /
> `链接段` / `initcall` / `自发现`）即过。用自己的话写清楚即可。

## 1. 为什么不把设备地址写死，而要绕一圈 dts → dtc → dtb → 解析 → compatible 匹配？

当一款 OS 要适配 N 块开发板时，这层解耦省下了什么？
（联系 xv6 把 UART/virtio 地址写死在 `memlayout.h` vs Linux 用设备树；以及下一课 BSP——
bootloader + 设备树不变、OS 升级时驱动如何复用。）

<!-- TODO: 在此作答。提示：硬编码地址 / 内核分叉 / 设备树解耦 / 同一镜像跑多板 / dtb 传参。 -->

## 2. “驱动表字符匹配”里：同名、多候选时如何裁决？为什么“最具体优先”？

若两个驱动 `compatible` 同一字符串、或一个设备 `compatible` 列了多个候选（从具体到通用，
如 `["acme,blink-v2","acme,blink","generic-gpio"]`），匹配该如何裁决？举一个会误匹配的例子。

<!-- TODO: 在此作答。提示：精确字符串相等 / 列表顺序从具体到通用 / 第一个命中 / 向后兼容降级 / 子串误匹配。 -->

## 3. driver derive / 链接段注册：“加一个驱动 = 加一个标注，框架一行不改”。

这种“插件自发现”你还在哪见过（`inventory`/`linkme`、内核 `initcall`、`insmod`、Rust trait 注册）？
相对“手写一张全局注册表”，它各自的成本与风险（链接顺序、初始化时机、调试可见性）是什么？

<!-- TODO: 在此作答。提示：链接段收集 / __start..__stop / initcall level / deferred probe / 顺序不可控 / 调试可见性。 -->

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
