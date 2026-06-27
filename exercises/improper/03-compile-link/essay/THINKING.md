# 03 · 编译链接 思考题（在每题下作答即可）

> 判据：答案非空且命中关键字（如 `program header` / `LMA` / `VMA` / `零填充` /
> `加载地址`）即过 `ESSAY_PASS`。用自己的话写清楚即可。

## 1. ELF 与纯二进制，加载时「谁决定每个段落在哪个地址」？

为什么纯二进制必须「link 地址 == load 地址」，而 ELF 可以不必（program header 帮了什么忙）？

<!-- TODO: 在此作答。提示：链接脚本 / program header / LMA / VMA / 重定位 / 加载地址。 -->

## 2. `.bss` 为什么不占文件体积（零填充）？

它「该清多大」这条信息存在镜像的哪里？这样省了什么、又把什么责任甩给了加载器/启动代码？

<!-- TODO: 在此作答。提示：p_filesz < p_memsz / 零填充 / __bss_start..__bss_end / 启动代码清零。 -->

## 3. A→B→C 的执行顺序到底由什么决定？

不重新编译任何 app、只改 linker script / app 表，能否让 B 先于 A 跑？把它和真实的
`bootrom → bootloader → kernel` 串接链做个类比。

<!-- TODO: 在此作答。提示：.apps 段内顺序 / KEEP 输入次序 / 交接表 / 串接启动。 -->

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
