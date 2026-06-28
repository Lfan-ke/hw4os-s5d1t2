LABROOT := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $(CURDIR)/../../../..)

kernel.elf: src/main.rs Cargo.toml linker.ld .cargo/config.toml
	cargo build --release
	cp target/riscv64gc-unknown-none-elf/release/rt-kernel kernel.elf

.PHONY: run clean
run: kernel.elf
	qemu-system-riscv64 -machine virt -nographic -bios default -kernel kernel.elf
clean:
	cargo clean 2>/dev/null || true
	rm -f kernel.elf
