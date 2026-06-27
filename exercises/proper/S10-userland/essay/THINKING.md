# S10 思考题

请结合本实验三个用户程序（app_sort.c / app_template.c / app_tui.c）与 ulib（U 态丐版 libc）作答。

## Q1. 本课无分页、用户与内核同地址空间，U 态物理上「能」直接跳进 `console_putchar`。为什么仍坚持走 `ecall` / libc？

（提示：调用号+寄存器约定 = 稳定 ABI；`console_putchar` 内部还要再 `ecall` SBI 是特权操作；可移植——换内核只要提供 `SYS_WRITE`。）

## Q2. 快排的 `partition` 决定了什么？固定取 `a[hi]` 作枢轴在什么输入下退化？真实 `qsort` 怎么缓解？

（提示：划分把区间重排成「左<枢轴≤右」并返回落定位置，使两个子问题独立；已排序/逆序/大量重复 → O(n²) + 递归深；三数取中/随机枢轴/三路划分/introsort。）

## Q3. 模板引擎与 TUI 渲染各自对应真实系统里的什么？它们共享什么工程套路？

（提示：模板=扫描+状态切换的小解释器（Jinja2/Go template/shell `${}`）；TUI=结构化文本→ANSI 转义（ncurses/Markdown 终端预览）；逻辑与表现解耦。）

<!-- LABCTL_ESSAY_TODO: 在此填写你的作答（替换本行）。 -->
