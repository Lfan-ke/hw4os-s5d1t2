# S09b · 真工具链链接 思考题（在每题下作答即可）

> 判据：答案非空且命中关键字（如 `ET_REL` / `程序头` / `INTERP` / `重定位` / `共享` /
> `拷贝` / `GOT` / `PC 相对` / `ASLR`）即过 `ESSAY_PASS`。用自己的话写清楚即可。

## 1. `gcc -c` 出的 `.o` 即便 `chmod +x` 也跑不了，根因是什么？

<!-- TODO: 在此作答。提示：ET_REL / 无程序头(program header) / 无 PT_LOAD / 无 INTERP / 符号 U 未重定位 / 需 ld 链成 EXEC 或 PIE。 -->

## 2. `-static` 比动态大 ~49 倍，省下的去哪了？静/动各自的取舍？

<!-- TODO: 在此作答。提示：拷贝 vs 共享 / glibc 代码 / page cache 多进程共享 / 可移植 vs 省空间+可升级+换 .so 打补丁。 -->

## 3. RISC-V `puts@plt` 的 `auipc/ld/jalr` 三条各做什么？为何必须经 GOT 间接、而不是一条 `jal`？

<!-- TODO: 在此作答。提示：auipc 算 GOT 项地址 / ld 装入真实地址 / jalr 间接跳；链接期地址未知 + jal ±1MB PC 相对范围 + PIC/ASLR 代码只读。 -->

<!-- LABCTL_ESSAY_TODO：在上方写下你的作答，然后删除本行即视为完成 -->
