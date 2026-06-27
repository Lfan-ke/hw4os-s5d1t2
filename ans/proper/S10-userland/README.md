# 正经·S10 · 用户程序（排序 + 模板引擎 + MD→ANSI TUI）

> 承接 S8（跌入 U 态 + `ecall` 系统调用）与 S9（最小 libc）。本课在自研 OS 上
> 用「丐版 libc（`ulib`）」写出三个**真正跑在 U 态**的用户程序，验证整条工具链 +
> syscall ABI + 运行时是通的——这是 rcore「用户程序集」那一步的最小落地。

## 0. 这节课在讲什么

内核搭好了（trap / U 态 / syscall / libc），现在该写**用户态应用**了。三个程序：

1. **排序算法**（`app_sort.c`）：对一组整数做快速排序，自检非降序 → `SORT_PASS`。
2. **极简模板引擎**（`app_template.c`）：把 `"Hi {{name}}, welcome to {{os}} v{{ver}}!"`
   里的 `{{key}}` 替换成键值表里的值，比对期望串 → `TMPL_PASS`。
3. **MD→ANSI TUI 组件**（`app_tui.c`，已给）：读一段 Markdown（行首 `# ` 标题、
   行内 `**粗体**`），渲染成带 ANSI 转义序列的终端文本 → `TUI_PASS`。

三者都过 → 用户 `exit(0)` → 内核回收并打印 `ALL_PASS`、关机。

```
ulib（U 态丐版 libc）   ──ecall(SYS_WRITE/SYS_EXIT)──▶  syscall 分发（S 态）──▶ console
   u_puts/u_write/u_putint/u_strcmp ...                  do_syscall                 SBI
```

> 与 S8 一脉相承：U 态程序**只认调用号 + 寄存器约定**，不直接调用内核 `console_putchar`；
> 无分页，用户与内核同地址空间，仅靠特权级隔离（PMP 由 OpenSBI 给 S/U 全权）。

## 1. 你要实现的

### (a) `app_sort.c` 的 `partition()`（Lomuto 划分）

```
pivot = a[hi]; i = lo
for j in lo..hi-1:
    if a[j] < pivot: swap(a[i], a[j]); i++
swap(a[i], a[hi])      // 枢轴归位
return i               // a[lo..i-1] < a[i] <= a[i+1..hi]
```

`quicksort` 与驱动已给：`quicksort(a,lo,p-1)` + `quicksort(a,p+1,hi)`。划分对了，
整段就排好，自检通过打印 `SORT_PASS`。占位 `return hi`（不划分）→ 数组原封不动 → 不过。

### (b) `app_template.c` 的 `render()` 替换循环

```
扫 tmpl：
  遇 "{{"  → 跳过它，读 key 直到 "}}"，跳过 "}}"，
            在 env(nenv 项) 里按「key 长度相等 + 逐字符相等」查到 val，拷进 out
  其余字符 → 原样拷进 out
out 末尾补 '\0'
```

`env`、模板、期望串、驱动已给。替换对了，输出 == `"Hi Ada, welcome to NanoOS v3!"`，
打印 `TMPL_PASS`。占位「照抄模板」会把 `{{name}}` 字面量留在输出 → 不匹配 → 不过。

### (c) `app_tui.c` —— **已给**，无需改动

行首 `# X` → `ESC[1;4;36m X ESC[0m`（加粗+下划线+青）；行内 `**X**` → `ESC[1m X ESC[0m`（加粗）。

## 2. 运行

```
labctl run proper/S10-userland
make -C kernel run     # 手动跑（OpenSBI banner 后见用户程序输出）
```

判据：输出含 `SORT_PASS` / `TMPL_PASS` / `TUI_PASS` / `ALL_PASS`，不出现 `FAIL` / `panic` / `UNEXPECTED`。

## 3. 完成标准 (DoD)

- [ ] `SORT_PASS`：快排把乱序数组排成非降序。
- [ ] `TMPL_PASS`：模板引擎把 `{{key}}` 正确替换为表中值。
- [ ] `TUI_PASS`：MD→ANSI 组件生成标题/粗体的转义序列（已给）。
- [ ] 三者全过、用户 `exit(0)`、内核打印 `ALL_PASS` 且 qemu 正常关机。
- [ ] 能说清：用户程序为何只经 `ecall`/libc 求服务，而非直接调内核函数（隔离 + ABI 稳定）。

## 4. 引申

- 真正的 libc（S9 完整版）：`malloc`/`free` 堆分配、`printf` 变参、`open/read` 文件 IO，
  本课用静态缓冲 + `u_putint` 顶替。
- 模板引擎完整版：条件 `{{#if}}`、循环 `{{#each}}`、转义、嵌套——本课只做最朴素的变量替换。
- TUI 完整版：颜色主题、列表/表格/边框盒子、宽字符对齐、终端尺寸查询（`ioctl TIOCGWINSZ`）。
- 多道用户程序：把三个 app 做成独立进程经 `fork/exec` 拉起（接 S8 进程模型），而非顺序内联。
