# 正经·S09b · 真工具链：把 link 全过程用真 gcc/ar/ld/readelf/objdump/dlopen 跑一遍，
# 每个子实验把断言结果 echo 成 *_PASS / *_FAIL，由 labctl(make-host) grep 判题。
# test/clean 骨架与命令行勿改；学生在各 e? 目标填命令与 grep 断言（见 exercises 骨架）。
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
	@if readelf -h $(B)/hello.o | grep -q 'Type:.*REL' \
	 && nm $(B)/hello.o | grep -q ' T main' && nm $(B)/hello.o | grep -q ' U puts' \
	 && { cp $(B)/hello.o $(B)/notrun; chmod +x $(B)/notrun; ! ./$(B)/notrun 2>$(B)/e1.err; grep -q 'Exec format error' $(B)/e1.err; }; \
	 then echo ETREL_PASS; else echo ETREL_FAIL; fi

# ── E2：静态 vs 动态：体积/file/ldd/ar.a vs -shared.so ───────────
e2: | $(B)
	@gcc -c hello.c -o $(B)/hello.o
	@gcc $(B)/hello.o -o $(B)/hello_dyn
	@gcc -static $(B)/hello.o -o $(B)/hello_static
	@gcc -c -fPIC mathlib.c -o $(B)/mathlib.o
	@ar rcs $(B)/libmath.a $(B)/mathlib.o
	@gcc -shared -fPIC mathlib.c -o $(B)/libmath.so
	@if [ $$(stat -c%s $(B)/hello_static) -gt $$(( $$(stat -c%s $(B)/hello_dyn) * 10 )) ] \
	 && ldd $(B)/hello_static 2>&1 | grep -q 'not a dynamic executable' && ldd $(B)/hello_dyn | grep -q 'libc.so.6' \
	 && file $(B)/hello_static | grep -q 'statically linked' && file $(B)/hello_dyn | grep -q 'dynamically linked' \
	 && ar t $(B)/libmath.a | grep -q 'mathlib.o' && file $(B)/libmath.so | grep -q 'shared object'; \
	 then echo STATIC_PASS; else echo STATIC_FAIL; fi

# ── E3：readelf 解剖动态 ELF：-d/-l/-r，lazy vs BIND_NOW ────────
e3: | $(B)
	@gcc -c hello.c -o $(B)/hello.o
	@gcc $(B)/hello.o -o $(B)/hello_dyn
	@gcc $(B)/hello.o -o $(B)/hello_lazy -Wl,-z,lazy -no-pie
	@if readelf -d $(B)/hello_dyn | grep -q 'NEEDED.*libc.so.6' \
	 && readelf -l $(B)/hello_dyn | grep -q 'ld-linux-x86-64.so.2' && [ $$(readelf -l $(B)/hello_dyn | grep -c 'LOAD') -ge 2 ] \
	 && readelf -r $(B)/hello_dyn | grep -q 'JUMP_SLO' && readelf -r $(B)/hello_dyn | grep -q 'GLOB_DAT' && readelf -r $(B)/hello_dyn | grep -q 'RELATIVE' \
	 && readelf -d $(B)/hello_dyn | grep -q 'BIND_NOW' && ! readelf -d $(B)/hello_lazy | grep -q 'BIND_NOW'; \
	 then echo READELF_PASS; else echo READELF_FAIL; fi

# ── E4：RISC-V 交叉：puts@plt auipc/ld/jalr + R_RISCV_JUMP_SLOT + qemu 实跑 ──
e4: | $(B)
	@$(RV) -no-pie -fno-pie -Wl,-z,lazy hello.c -o $(B)/hello_rv64
	@if file $(B)/hello_rv64 | grep -q 'RISC-V' \
	 && readelf -r $(B)/hello_rv64 | grep -q 'R_RISCV_JUMP_SLOT' \
	 && $(RVOD) -d -j .plt $(B)/hello_rv64 | grep -A3 'puts@plt' | grep -q auipc \
	 && $(RVOD) -d -j .plt $(B)/hello_rv64 | grep -A3 'puts@plt' | grep -qP '\tld\t' \
	 && $(RVOD) -d -j .plt $(B)/hello_rv64 | grep -A3 'puts@plt' | grep -q jalr \
	 && qemu-riscv64 -L $(SYSROOT) $(B)/hello_rv64 | grep -q '^hello$$'; \
	 then echo RVPLT_PASS; else echo RVPLT_FAIL; fi

# ── E5：dlopen/dlsym/dlclose + 缺符号失败 ────────────────────────
e5: | $(B)
	@gcc -shared -fPIC plugin.c -o $(B)/plugin.so
	@gcc -shared -fPIC badplugin.c -o $(B)/badplugin.so
	@gcc host.c -o $(B)/host -ldl
	@./$(B)/host $(B)/plugin.so $(B)/badplugin.so > $(B)/host.out 2>&1; \
	 if grep -q 'answer=42' $(B)/host.out && grep -q 'nonexistent=NULL' $(B)/host.out \
	 && grep -q 'undefined symbol: missing_extern_symbol' $(B)/host.out; \
	 then echo DLOPEN_PASS; else echo DLOPEN_FAIL; fi
