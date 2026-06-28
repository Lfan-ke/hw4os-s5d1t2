# 03b · 动态链接加载器（软件建模）：静态各塞一份 vs 动态按 key 共享 + ld.so 解析回填 GOT

> 不正经赛道 · 第 3.5 课 —— 纯软件建模，host 直接跑。承接 [03 编译链接](../03-compile-link/)。
> 一句话母题：**静态链接把库各塞一份（费空间、改 bug 逐个重编）；动态链接只存「名字引用 + 一张空 GOT」，库留一份共享，由 `ld.so` 按【符号名 key】查表、把「基址+偏移」回填进各程序私有的 GOT。**

## 0. 做什么 / 有什么用 / 为什么（先想清楚）

100 个程序都要用 `puts`。两种把"别人写好的库"接进来的办法：

- **静态链接**：把整份 `libc` **拷进每个可执行**。好处是单文件、无依赖、好部署；
  代价是 100 个程序 = **100 份**相同代码（空间 ×100），libc 出 bug 要**逐个重新编译**。
- **动态链接**：库**只留一份**共享；每个程序只记「我需要 `libc`」(名字) + 一张**空 GOT**
  （每个外部符号一个待填的槽）。程序加载时，`ld.so` 按**符号名**去那一份共享库**查表**，
  把符号真实地址（`库基址 + 库内偏移`）**回填**进这个程序**私有的 GOT**。
  好处是空间小（库一份 + 各自一张小 GOT）、**换一个 `.so` 全体程序一起修好**。

这节课你来当一回 `ld.so`：用纯软件（`HashMap`/数组）把"动态库的公共资源"建模成一张
**按 key 查的符号表**，亲手体会 **"各塞一份=费空间" vs "按 key 共享一份=省空间"**，
并把符号解析、GOT 回填、lazy/now 绑定、`dlopen` 一路做出来。

## 0.1 大致轮廓 → 简单示例 → 走向现实（本课的递进弧线）

本课是一座桥：从"意境/心智模型"出发，每一题更靠近真实 `ld.so`，走完即等价真实重定位——
**完全现实的版本就是正经实验 [`S09b-linking`](../../proper/S09b-linking/)**（用真 `gcc`/`readelf`/`ldd` 跑）。

```
意境(E1-E2)  ──→  接近现实(E3-E5)  ──→  几乎现实(E6)  ══接力══>  正经 S09b（真工具链）
各塞一份/名字引用    按key解析回填GOT/省空间/绑定时机   dlopen插件         真 .so / readelf -r / ld.so
```

## 1. 你要填的函数（`sw/rust/src/main.rs` 或 `sw/c/dynlink.c`）

| 子实验 | 函数 | 要求 | 判据 |
| :-- | :-- | :-- | :-- |
| E1 静态各塞一份 | `static_total_space` / `make_static` | 每程序 = own + **一整份库拷贝**；总空间 `n*(own+lib)` | `STATIC_PASS` |
| E2 动态名字引用 | `make_dyn` | 不塞库，只 `needed=[库名]` + 每个外部符号一个**空 GOT 槽** | `DYNSYM_PASS` |
| E3 ld.so 解析 | `Library::lookup` / `resolve` | 按**符号名 key** 查 → `base+偏移`；遍历 GOT 空槽**回填** | `RESOLVE_PASS` |
| E4 共享省空间 | `dyn_total_space` | **一份共享库** + `n*(own+小GOT)`，且远小于静态 | `SHARE_PASS` |
| E5 绑定时机 | `now_bind` / `lazy_bind` | now 解析全部槽；lazy 只解析**被调用**的符号 | `BIND_PASS` |
| E6 dlopen | `dlopen` / `dlsym` | 运行时装库；查符号，**缺库/缺符号→Err（不崩）** | `DLOPEN_PASS` |

六段皆过再打印 `ALL_PASS`。

```
labctl run improper/03b-dynlink-sim     # 跑 C/Rust 两条路径
labctl watch                            # 边改边自动判定
labctl hint improper/03b-dynlink-sim    # 卡住看提示
```

## 2. 关键约定（harness 已写死，照着填）

- 库 `libc` 导出 `puts@0x100 / printf@0x240 / malloc@0x380`，加载基址 `base=0x4000_0000`；
  符号**真实地址 = base + 偏移**（如 `puts`→`0x4000_0100`）。
- 建模常量：`LIB_CODE=1000`（一份库大小）、`OWN_CODE=200`（程序自身）、`GOT_SLOT=8`（一槽）。
- 未解析的 GOT 槽：Rust 用 `resolved: None`，C 用 `resolved=0`（库基址非 0，故 0 可作"未解析"哨兵）。
- E4 对照：`n=100, n_syms=3` → 静态 `100*(200+1000)=120000`，动态 `1000+100*(200+24)=23400`，动态必须 `<` 静态。
- E5：程序声明 3 个符号但只调用 `puts` → `now_bind` 返回 3、`lazy_bind` 返回 1，且 lazy 下未调用的符号仍未解析。
- E6：`dlsym` 缺符号/缺库不得 panic/崩溃，要返回 `Err` / `*err=1`。

## 3. 完成标准 (DoD)

- [ ] `STATIC_PASS`：静态各塞一份，总空间 `n*(own+lib)`，每程序自包含（从自己拷贝里查得到 `puts`）。
- [ ] `DYNSYM_PASS`：动态程序不塞库，只 `needed` + 空 GOT 槽（全未解析）。
- [ ] `RESOLVE_PASS`：`lookup` 按 key 查 `base+偏移`，`resolve` 把三个符号回填进私有 GOT。
- [ ] `SHARE_PASS`：动态 = 一份共享库 + 各自小 GOT，量化看到远小于静态、说清省在哪。
- [ ] `BIND_PASS`：now 解析全部、lazy 只解析被调用的；说清两者取舍。
- [ ] `DLOPEN_PASS` + `ALL_PASS`：运行时装库、查符号，缺失安全失败。
- [ ] C/Rust 任一条全过（必修）；另一条也过计辅助分。
- [ ] essay 思考题作答通过（`ESSAY_PASS`，独立辅助账）。

## 4. 引申（可扩展性：从建模 ld.so 走向真实 ld.so）

本课用 `HashMap`/数组**建模**了动态链接：按 key 查符号、回填 GOT、共享一份、lazy/now、dlopen。
想把它推到接近真实，按兴趣选：

1. **加 ASLR 随机基址**：让每个"进程"加载同一份库到**不同 base**，于是各进程 GOT 里同一个 `puts`
   填的值不同——亲手复现"代码共享、地址私有"。再加一个全局 page-table 模型说明只读代码页只一份。
2. **加重定位类型**：把 GOT 回填从"直接写地址"升级为按重定位项 `(offset, type, symbol)` 处理，
   区分 `JUMP_SLOT`(函数)/`GLOB_DAT`(全局变量)/`RELATIVE`(自身引用=base+addend)——对位 `readelf -r`。
3. **lazy 的真实跳板**：把 E5 的 lazy 做成"GOT 槽初值指向解析器、首次调用触发 `_dl_runtime_resolve`、
   回填后第二次直跳"，对应 PLT→GOT→ld.so 的经典懒绑定（§6 的 mermaid）。
4. **符号查找顺序 + 弱符号**：多个库都定义 `foo` 时，按依赖图顺序"第一个全局/弱定义胜出"；实现
   `STB_GLOBAL`/`STB_WEAK` 与重复定义报错——这是 `extern`/`static`/弱符号链接语义的来源。
5. **page cache 共享建模**：加一个全局 `inode→物理页` 表，多个进程 `mmap` 同名库 → 命中同一物理页，
   复现"跨进程共享是内核按 inode 白送的"（`ld.so` 全程不知道别的进程存在）。
6. **接力到 `S09b-linking`**：把本课每个概念用真 `gcc -c`/`ar`/`-shared -fPIC`/`readelf -r`/`ldd`/`dlopen`
   跑一遍——模型里的 `lookup` 就是 `ld.so` 的 `find_sym`，`resolve` 就是 `do_relocs`。

## 5. 思考题（`essay/THINKING.md` 作答即可）

1. 为什么静态链接「改一个 libc bug 要逐个重编」，动态只需「换一个 `.so`」？空间上各塞一份 vs 一份共享差在哪？
2. 为什么 GOT 每进程私有、而库代码可多进程共享？（提示：只读 PIC 代码页 / ASLR 基址不同 / GOT 在可写数据段 COW）
3. lazy vs now 绑定的取舍？现代发行版为什么默认 now + RELRO（GOT 只读）？
4. 本课 `lookup` 按 key 查表对应真实 `ld.so` 的什么步骤？模型还简化掉了哪些真实机制？
