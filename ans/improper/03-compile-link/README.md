# 03 · 编译链接：手写链接脚本、段布局与 A→B→C 串接执行

> 不正经赛道 · 第 3 课 —— 纯软件建模，host 直接跑。
> 一句话母题：**编译器只产出"带名字的碎片(section)"，是链接脚本决定谁落在哪个地址、谁先谁后、要不要塞进同一颗镜像。**

## 0. 这节课在讲什么

你来当一回「内存的房产中介」：给 `.text/.rodata/.data/.bss` 划地块，把自定义段
`.config` 落到约定地址，搞清 **ELF vs 纯二进制**到底差在哪，最后把三个小程序 A、B、C
按一张表顺序拉起——这正是 rcore ch2 批处理内核「把用户程序嵌进内核、靠一张表顺序串接」
的最小模型。

本课**不真的调 ld/objcopy**，而是用纯软件数组/结构体把链接器的工作**建模**出来
（地址布局、对齐、ELF 头字节、app 指针表），把「地址由谁定」这一根筋讲透。对应真实
世界的 rcore `linker.ld`（`.text` 钉 `0x80200000`）、`build.rs` 的 `.incbin` app 表、
`objcopy -O binary`、xv6 `kernel.ld` / `_entry`。

## 1. 你要填的函数（`sw/rust/src/main.rs` 或 `sw/c/clink.c`）

| 子实验 | 函数 | 要求 | 判据 |
| :-- | :-- | :-- | :-- |
| E1 段布局 | `layout` / `clear_bss` | 按 text→rodata→data→bss 连续摆放；`.bss` 全 0 | 地址递增 + `.bss` 零填充 → `LAYOUT_PASS` |
| E2 自定义段 | `place_config` / `make_config` | `.config` 落 4K 对齐地址、字段对 | `SECTION_PASS` |
| E3 ELF 头 | `parse_elf` | 校验魔数、取 `e_entry`/`e_phoff` | `ELF_PASS` |
| E3 纯二进制 | `bin_base` / `bin_entry_offset` | link 基址 == load 地址，入口偏移 0 | `BIN_PASS` |
| E4 串接 | `run_apps` | 按 `.apps` 表顺序拉起 A→B→C | 出现顺序 == 段内顺序 → `CHAIN_PASS` |

五段皆过再打印 `ALL_PASS`。E4 可二选一实现：
`// TODO[a]` 函数指针表（最简，本骨架默认）/ `// ELSE[b]` `.incbin` 原始二进制顺序跳转（更像 rcore）。

```
labctl run improper/03-compile-link     # 跑 C/Rust 两条路径
labctl watch                            # 边改边自动判定
labctl hint improper/03-compile-link    # 卡住看提示
```

## 2. 关键约定（harness 已写死，照着填）

- 基址 `BASE = 0x80200000`；段顺序 `.text < .rodata < .data < .bss`。
- `.config` 魔数 `0x00C0FFEE`，`version=1, nslots=8, flags=0b101`，起始 4K 对齐。
- ELF64 头：魔数 `7F 45 4C 46`（前 4 字节）；`e_entry` 在偏移 24、`e_phoff` 在偏移 32，各 8 字节小端。
- 纯二进制：`bin_base()` 必须等于加载地址，`bin_entry_offset = entry - base` 才会是 0。
- `.apps` 表顺序即执行顺序：`[app_a, app_b, app_c]` → 打印 `APP_A/APP_B/APP_C`。

## 3. 完成标准 (DoD)

- [ ] `LAYOUT_PASS`：`text<rodata<data<bss` 连续递增、`.bss` 零填充、只读常量落只读区。
- [ ] `SECTION_PASS`：自定义 `.config` 段 4K 对齐且字段逐项对。
- [ ] `ELF_PASS` + `BIN_PASS`：同一程序两种产物都"能跑"，说清谁决定段落地址。
- [ ] `CHAIN_PASS` + `ALL_PASS`：A→B→C 顺序由 `.apps` 段内顺序决定且实测一致。
- [ ] C/Rust 任一条全过（必修）；另一条也过计辅助分。
- [ ] essay 思考题作答通过（`ESSAY_PASS`，独立辅助账）。

## 4. 引申（可扩展性：从建模链接器到真链接器/真镜像）

本课用纯软件数组/结构体**建模**了链接器的活：地址布局、对齐、ELF 头字节、app 指针表，没真的调 `ld/objcopy`、没处理重定位、没符号解析。想把这个最小版扩展成接近真实工具链，按兴趣选：

1. **真的写一份 `linker.ld` 跑通**：把本课 `BASE/段顺序` 的约定改写成真实链接脚本喂给 `riscv64-unknown-elf-ld`，用 `objcopy -O binary` 得到纯二进制，`readelf -l`/`nm`/`size` 对照你建模出的布局——验证「链接脚本决定段落地址」是真的。对照 rcore `linker.ld`（`.text` 钉 `0x80200000`）、xv6 `kernel.ld`。
2. **加重定位（relocation）**：本课地址都是写死的绝对值；真实可重定位目标文件 `.o` 里留着 `R_RISCV_*` 重定位项，链接时才回填。实现一个最小重定位表（`offset, type, symbol`）并在 `layout` 后回填，就能体会**位置无关码 PIC / GOT / PLT** 和 ASLR 为何需要它。
3. **符号表与多模块链接**：把 A/B/C 从「函数指针表」升级为带 `.symtab` 的多个模块，实现符号解析（定义/未定义/弱符号）与重复定义报错——这是链接器除布局外的另一半工作，也是 `extern`/`static` 链接语义的来源。
4. **E4 走 `.incbin` 真二进制路线**：把骨架默认的函数指针表（`TODO[a]`）换成 `ELSE[b]`——用 `build.rs` + `.incbin` 把 A/B/C 的原始二进制嵌进镜像、按 `.apps` 表顺序跳转，这正是 rcore ch2 批处理内核「把用户程序嵌进内核顺序串接」的真实做法。
5. **段属性与加载器/MMU**：给段加 `flags`（R/W/X）并模拟加载器按 program header 的权限建立映射，引出 **W^X**、`.rodata` 只读、`.bss` 由启动代码 `clear_bss`（思考题 2）——再往前一步就是真实的 ELF loader + 页表权限位。
6. **对照真实启动链**：把 `bootrom → bootloader → kernel`（思考题 3）落成可观察的多级镜像串接，体会「不改 app、只改链接脚本/app 表就能换执行顺序」与固件启动顺序的同构。

## 5. 思考题（`essay/THINKING.md` 作答即可）

1. ELF 与纯二进制，加载时「谁决定每个段落在哪个地址」？为什么纯二进制必须「link 地址 == load 地址」，而 ELF 可以不必（program header 帮了什么忙）？
2. `.bss` 为什么不占文件体积（零填充）？「该清多大」这条信息存在镜像哪里？省了什么、又把什么责任甩给了加载器/启动代码？
3. A→B→C 的执行顺序由什么决定？不重新编译任何 app、只改 linker script / app 表，能否让 B 先于 A 跑？与 `bootrom → bootloader → kernel` 串接链类比。
