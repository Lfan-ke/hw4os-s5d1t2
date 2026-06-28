# 重建静态 vvp(内嵌 system VPI)

1. 取 iverilog v12_0 源码:`curl -fsSL https://codeload.github.com/steveicarus/iverilog/tar.gz/refs/tags/v12_0 | tar xz`。
2. host 先 `sh autoconf.sh && ./configure && make`(生成 parse.cc/lexor.cc/tables.cc/sys_readmem_lex.c 等 + host `iverilog` 用于编 .vvp)。
3. 打补丁 `vpi_modules.static-system.patch` 到 `vvp/vpi_modules.cc`;把 `sys_table_static.c`、`readline_stub.c` 放进 `vpi/`;sed 关掉 config.h 里 `HAVE_LIBREADLINE`/`HAVE_READLINE_*`。
4. 跑 `xbuild-vvp.sh <arch>`(用 `/opt/<arch>-linux-musl-cross`)→ 产出静态 `/tmp/vvp-<arch>`。
5. 造 .vvp:host `iverilog -g2012 -o dut.vvp tb.v dut.v`,再把 `:vpi_module "...system.vpi";` 改成 `:vpi_module "system";` 并删其它 `:vpi_module` 行。
6. `vvp-<arch> dut.vvp` 即可在静态 musl(StarryOS)上跑,无需任何 .vpi 文件。
