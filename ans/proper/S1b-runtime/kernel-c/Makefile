LABROOT := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $(CURDIR)/../../../..)
CC := riscv64-unknown-elf-gcc
CFLAGS := -nostdlib -nostartfiles -mcmodel=medany -ffreestanding -fno-builtin -Wall -Wextra -O -I.

kernel.elf: entry.S syscalls.c minlibc.c main.c
	$(CC) $(CFLAGS) -T $(LABROOT)/common/kernel/linker.ld $^ -o $@

.PHONY: run clean
run: kernel.elf
	qemu-system-riscv64 -machine virt -nographic -bios default -kernel kernel.elf
clean:
	rm -f kernel.elf
