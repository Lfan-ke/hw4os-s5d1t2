# 正经·S09b · 真工具链：把 link 全过程用真 gcc/ar/ld/readelf/objdump/dlopen 跑一遍。
# 头部变量、test/clean/$(B) 目标与每个 e? 目标的【给定编译命令】勿改；
# 你只需把每个 e? 目标里的 `# TODO(...)` + `@echo <TOKEN>_FAIL` 换成真断言：
#   用给定命令的产物喂 grep，全中则 `echo <TOKEN>_PASS`，否则 `echo <TOKEN>_FAIL`。
# 每个目标必须只 echo 一枚 token 且永远 exit 0（用 `if … then … else … fi`），make 才不中断。
RV      := riscv64-linux-gnu-gcc
RVOD    := riscv64-linux-gnu-objdump
SYSROOT := /usr/riscv64-linux-gnu
B       := build

.PHONY: test clean e1 e2 e3 e4 e5
test: e1 e2 e3 e4 e5
	@echo ALL_PASS

clean:
	@rm -rf $(B)

$(B):
	@mkdir -p $(B)

# ── E1：.o 是 ET_REL，不能直接跑 ─────────────────────────────────
e1: | $(B)
	@gcc -c hello.c -o $(B)/hello.o
	# TODO(E1): 用 `readelf -h` 断言 Type=REL、`nm` 断言 'T main'+'U puts'、
	#   再 `cp+chmod +x` 后执行断言报 'Exec format error'；全中则 echo ETREL_PASS。
	@echo ETREL_FAIL

# ── E2：静态 vs 动态：体积/file/ldd/ar.a vs -shared.so ───────────
e2: | $(B)
	@gcc -c hello.c -o $(B)/hello.o
	@gcc $(B)/hello.o -o $(B)/hello_dyn
	@gcc -static $(B)/hello.o -o $(B)/hello_static
	@gcc -c -fPIC mathlib.c -o $(B)/mathlib.o
	@ar rcs $(B)/libmath.a $(B)/mathlib.o
	@gcc -shared -fPIC mathlib.c -o $(B)/libmath.so
	# TODO(E2): 断言 静态体积 > 动态*10；`ldd` 静态 'not a dynamic executable'(注意 2>&1)、
	#   动态 'libc.so.6'；`file` 静/动有别；`ar t` 含 mathlib.o；`file` .so 是 shared object。
	@echo STATIC_FAIL

# ── E3：readelf 解剖动态 ELF：-d/-l/-r，lazy vs BIND_NOW ────────
e3: | $(B)
	@gcc -c hello.c -o $(B)/hello.o
	@gcc $(B)/hello.o -o $(B)/hello_dyn
	@gcc $(B)/hello.o -o $(B)/hello_lazy -Wl,-z,lazy -no-pie
	# TODO(E3): `readelf -d` 断言 NEEDED libc.so.6 + BIND_NOW；`readelf -l` 断言 INTERP +
	#   LOAD 段数 >=2；`readelf -r` 断言含 JUMP_SLO、GLOB_DAT、RELATIVE；且 hello_lazy 无 BIND_NOW。
	@echo READELF_FAIL

# ── E4：RISC-V 交叉：puts@plt auipc/ld/jalr + R_RISCV_JUMP_SLOT + qemu 实跑 ──
e4: | $(B)
	@$(RV) -no-pie -fno-pie -Wl,-z,lazy hello.c -o $(B)/hello_rv64
	# TODO(E4): `file` 断言 RISC-V；`readelf -r` 断言 R_RISCV_JUMP_SLOT；`$(RVOD) -d -j .plt`
	#   的 puts@plt 段断言含 auipc / ld / jalr；`qemu-riscv64 -L $(SYSROOT)` 实跑断言输出 hello。
	@echo RVPLT_FAIL

# ── E5：dlopen/dlsym/dlclose + 缺符号失败 ────────────────────────
e5: | $(B)
	@gcc -shared -fPIC plugin.c -o $(B)/plugin.so
	@gcc -shared -fPIC badplugin.c -o $(B)/badplugin.so
	@gcc host.c -o $(B)/host -ldl
	# TODO(E5): 跑 `./$(B)/host plugin.so badplugin.so`，断言输出含 'answer=42'、
	#   'nonexistent=NULL'、'undefined symbol: missing_extern_symbol'；全中则 echo DLOPEN_PASS。
	@echo DLOPEN_FAIL
