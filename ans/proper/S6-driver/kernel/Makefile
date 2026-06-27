LABROOT := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $(CURDIR)/../../../..)
K := $(LABROOT)/common/kernel
KSRC := $(K)/entry.S $(K)/console.c $(K)/sbi.c
include $(LABROOT)/common/mk/kernel.mk

# 设备树：dtc 把 device.dts 编成 device.dtb；dtb_blob.S 在汇编时 .incbin 嵌入内核镜像。
device.dtb: device.dts
	dtc -I dts -O dtb -o $@ $<

# kernel.elf 需要 device.dtb 先就位（dtb_blob.S 汇编时读取它）。
kernel.elf: device.dtb

clean: clean-dtb
clean-dtb:
	rm -f device.dtb
