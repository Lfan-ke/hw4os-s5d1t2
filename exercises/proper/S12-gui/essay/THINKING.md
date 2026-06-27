# S12 思考题 · 简易 GUI 与 html/css 转译

回答下面三问（结合本实验的软件 framebuffer 与极简标记渲染）。

## Q1 framebuffer 为什么是 GUI 的"最底层"？本实验的软件 framebuffer 与真 virtio-GPU 的差别在哪？

（提示：显示硬件本质是周期性扫描一块约定格式的内存；真 GPU 多出的是"把这块内存交给 host 显示"的协议 —— 资源创建/附 backing、SET_SCANOUT、TRANSFER+FLUSH。）

## Q2 把 `<div style="color:..">text</div>` 转译成绘制命令，和浏览器渲染管线相比省了什么？

（提示：对照 解析→DOM+CSSOM→样式计算→布局(reflow)→绘制(paint)→合成 这条流水线，本实验只保留了哪一段？DOM/层叠/盒模型/重排各省在哪。）

## Q3 为什么用 framebuffer 校验和而不是逐像素断言来判对错？这种判法有什么局限？

（提示：校验和一次覆盖全部像素、对错位极敏感、适合自动评测；但只给"对/不对"不给"哪里错"，且对期望值固化敏感。）

---

LABCTL_ESSAY_TODO: 在此写下你的作答（替换本行）。
