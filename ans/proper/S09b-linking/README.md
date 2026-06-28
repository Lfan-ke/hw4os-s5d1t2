# 正经·S09b · 真工具链：链接的真实流水线（gcc -c→ET_REL / static vs shared / readelf / RV-PLT / dlopen）

> 正经赛道 · 承接 [S09 极简 libc](../S09-libc/)，并与不正经 [03b 动态链接模拟](../../improper/03b-dynlink-sim/) 一一对照。
> 一句话母题：**链接不是一个动作，而是一条真实的工具链流水线**——`gcc -c` 出 `ET_REL`（不能跑）→
> `ld` 链成可执行（静态=各塞一份拷贝 / 动态=只存名字引用）→ 运行期 `ld.so` 经 PLT/GOT 把外部符号地址补齐。

## 0. 做什么 / 为什么（与 03b 的关系）

不正经 [03b](../../improper/03b-dynlink-sim/) 用 `HashMap`/数组**软件建模**了动态链接（按 key 查符号、回填 GOT、
共享一份、lazy/now、dlopen）。本课把那套模型**逐一对到真产物**：用真 `gcc`/`ar`/`ld`/`readelf`/`nm`/`ldd`/
`objdump`/`qemu`/`dlopen` 跑一遍，亲眼看 ET_REL 跑不了、静态比动态大 ~49 倍、`readelf -r` 里真的 `JUMP_SLOT`、
RISC-V `puts@plt` 的 `auipc/ld/jalr` 三条指令经 GOT 间接跳、`dlopen` 缺符号当场失败。

> **"完全现实就是正经实验"**——03b 里 `lookup` 就是 `ld.so` 的 `find_sym`，`resolve` 就是 `do_relocs`；
> 本课把它们换成真工具命令 + 对真输出做断言。

## 1. 你要填的断言（`toolchain/Makefile` 的 e1..e5 目标）

每个子实验：**给定的编译命令保留**，你补 `if … grep … then echo *_PASS else echo *_FAIL fi` 断言。

| 子实验 | 真命令（已给） | 你要断言的事实 | 判据 |
| :-- | :-- | :-- | :-- |
| E1 ET_REL | `gcc -c hello.o` | `readelf -h` Type=REL；`nm` 'T main'+'U puts'；`chmod +x` 跑报 Exec format error | `ETREL_PASS` |
| E2 静/动 | `-static` / `-shared -fPIC` / `ar rcs` | 静态体积 >10× 动态；`ldd`/`file` 静动有别；`.a` 含成员、`.so` 是 shared object | `STATIC_PASS` |
| E3 解剖 | `readelf -d/-l/-r` | NEEDED libc / INTERP / LOAD≥2 / JUMP_SLO+GLOB_DAT+RELATIVE / BIND_NOW vs lazy | `READELF_PASS` |
| E4 RV-PLT | `riscv64-linux-gnu-gcc` / `objdump` / `qemu-riscv64` | `R_RISCV_JUMP_SLOT`；`puts@plt` 有 auipc/ld/jalr；qemu 实跑出 `hello` | `RVPLT_PASS` |
| E5 dlopen | `gcc -shared` + `host.c -ldl` | `answer=42`；缺符号 `dlsym`=NULL；坏插件 RTLD_NOW `undefined symbol` | `DLOPEN_PASS` |

五段皆过、`make` 再 echo `ALL_PASS`。

```
labctl run proper/S09b-linking          # 跑 make-host：make -s test，grep *_PASS 判题
make -C toolchain test                 # 手动直接跑（看完整流水线）
labctl hint proper/S09b-linking         # 卡住看提示
```

## 2. 关键约定 / 已编进判据的坑（照着填，别踩）

- `hello.c` **必须**保持 `printf("hello\n")`——gcc 把它优化成 `puts`，故 `nm` 出 `U puts`（不是 `U printf`）。
- `ldd` 对**静态**二进制把 "not a dynamic executable" 写到 **stderr**，断言要 `2>&1`。
- x86 `readelf -r` 把 `JUMP_SLOT` **截断成 `JUMP_SLO`**，用子串 grep；RISC-V 不截断（`R_RISCV_JUMP_SLOT`）。
- 现代 Ubuntu 默认 **BIND_NOW + PIE**；要看经典 lazy 绑定须 `-Wl,-z,lazy -no-pie`。
- `readelf -l` 的 `LOAD` 段实测 **4 个**（binutils 2.42），判 `>=2` 不要写 `==2`。
- RISC-V 反汇编必须用 `riscv64-linux-gnu-objdump`（不是 host objdump）；qemu 跑 RV **动态**版必须 `-L /usr/riscv64-linux-gnu`（找 INTERP）。
- `.a` 不是 ELF，用 `ar t` 列成员，别 `readelf` 它。
- 每个 `e?` 目标都**只 echo 一枚 token 且永远 exit 0**（`if…then…else…fi`），保证 `make` 不中断、token 完整。

## 3. 完成标准 (DoD)

- [ ] `ETREL_PASS`：`.o` 是 ET_REL、`nm` 'T main'+'U puts'、`chmod +x` 跑报 Exec format error。
- [ ] `STATIC_PASS`：静态 >10× 动态、`ldd`/`file` 静动有别、`ar t` 列成员、`.so` 是 shared object。
- [ ] `READELF_PASS`：NEEDED/INTERP/LOAD≥2/三类重定位/BIND_NOW vs lazy 都对。
- [ ] `RVPLT_PASS`：`R_RISCV_JUMP_SLOT`、`puts@plt` auipc/ld/jalr、qemu 实跑出 `hello`。
- [ ] `DLOPEN_PASS` + `ALL_PASS`：dlopen 正常 + 缺符号失败。
- [ ] essay 思考题作答通过（`ESSAY_PASS`，独立辅助账）。

> **env=host 例外说明**：proper 赛道惯例 `env=qemu-virt`（boot S 态内核）。本课是**真 host 工具链** + E4 走 **qemu-user**（非 qemu-virt 内核），故 `meta.toml` 置 `env=host`。`env` 字段当前是 `dead_code`、不参与 build 选择，真正驱动执行的是 `build="make-host"`（变体目录跑 `make -s test`）。这是有意例外。

## 4. 引申（继续逼近真实链接器/加载器）

1. **自写 `linker.ld`**：手控段地址（`.text` 钉某地址），`readelf -l` 验证你的布局生效——对照 03b 引申①的 ASLR 基址。
2. **RELRO 三档**：`-z lazy` / `-z relro,now`（部分）/ Full RELRO 对 GOT 可写性的影响，`readelf -l` 看 `GNU_RELRO`——对照 03b 的 lazy/now（E5）。
3. **版本符号**：`--version-script` 给 `.so` 导出符号打版本（`puts@GLIBC_2.27`），`readelf -V` 观察。
4. **找不到库**：删/改 `.so` 路径触发 `cannot open shared object file`，用 `LD_LIBRARY_PATH` / `-rpath` / `ldconfig` 修复——对应 03b 的 `dlsym` 缺库失败。
5. **静态库成员选择**：`.a` 里多个 `.o`，链接器只挑用到的；`--gc-sections` 进一步裁剪——对照"静态各塞一份"其实可裁。
6. **逐题对照 03b**：把本课每个真产物连回 03b 的软件模型（`lookup`↔`find_sym`、GOT 回填↔`do_relocs`、共享一份↔page cache）。

## 5. 思考题（`essay/THINKING.md` 作答即可）

1. `gcc -c` 出的 `.o` 即便 `chmod +x` 也跑不了，根因是什么？（提示：ET_REL/无程序头/无 PT_LOAD/无 INTERP/符号 U 未重定位/需 ld 链成 EXEC 或 PIE）
2. `-static` 比动态大 ~49 倍，省下的去哪了？静/动各自的取舍？（提示：拷贝 vs 共享、可移植 vs 省空间/可升级/换 .so 打补丁）
3. RISC-V `puts@plt` 的 `auipc/ld/jalr` 三条各做什么？为何必须经 GOT 间接、而不是一条 `jal` 直跳？（提示：链接期地址未知 + `jal` ±1MB PC 相对范围 + PIC/ASLR）
