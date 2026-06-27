# 正经赛道内核构建配方（C，S 态内核，OpenSBI 引导）。
# 实验目录 Makefile 设置 LABROOT 后 include 本文件；labctl qemu-virt 调 make kernel.elf + qemu。
#
#   LABROOT := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $(CURDIR)/../../..)
#   include $(LABROOT)/common/mk/kernel.mk

CC      := riscv64-unknown-elf-gcc
COMMONK := $(LABROOT)/common/kernel
CFLAGS  := -nostdlib -nostartfiles -mcmodel=medany -ffreestanding -fno-pic \
           -Wall -Wextra -O -I$(COMMONK)
# 共享源（实验 Makefile 可覆盖 KSRC 选取所需共享组件）+ 实验目录的 *.c/*.S
KSRC    ?= $(COMMONK)/entry.S $(COMMONK)/console.c
SRCS    := $(KSRC) $(wildcard *.c) $(wildcard *.S)
QEMU    := qemu-system-riscv64
QFLAGS  := -machine virt -nographic -bios default -kernel kernel.elf

.PHONY: run clean
kernel.elf: $(SRCS) $(COMMONK)/linker.ld
	$(CC) $(CFLAGS) -T $(COMMONK)/linker.ld $(SRCS) -o $@

run: kernel.elf
	$(QEMU) $(QFLAGS)

clean:
	rm -f kernel.elf
