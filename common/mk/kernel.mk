# 正经赛道内核构建配方（C，S 态内核，OpenSBI 引导）。
# 实验目录 Makefile 设置 LABROOT 后 include 本文件；labctl qemu-virt 调 make kernel.elf + qemu。
#
#   LABROOT := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $(CURDIR)/../../..)
#   include $(LABROOT)/common/mk/kernel.mk

CC      := riscv64-unknown-elf-gcc
COMMONK := $(LABROOT)/common/kernel
CFLAGS  := -nostdlib -nostartfiles -mcmodel=medany -ffreestanding -fno-pic \
           -Wall -Wextra -O -I$(COMMONK)
# 入口 + 共享 console + 实验目录的 *.c
SRCS    := $(COMMONK)/entry.S $(COMMONK)/console.c $(wildcard *.c)
QEMU    := qemu-system-riscv64
QFLAGS  := -machine virt -nographic -bios default -kernel kernel.elf

.PHONY: run clean
kernel.elf: $(SRCS) $(COMMONK)/linker.ld
	$(CC) $(CFLAGS) -T $(COMMONK)/linker.ld $(SRCS) -o $@

run: kernel.elf
	$(QEMU) $(QFLAGS)

clean:
	rm -f kernel.elf
