LABROOT := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $(CURDIR)/../../../..)
K := $(LABROOT)/common/kernel
# SMP 实验只需最小共享件：入口 + 控制台 + SBI（HSM hart_start / shutdown）。
KSRC := $(K)/entry.S $(K)/console.c $(K)/sbi.c
include $(LABROOT)/common/mk/kernel.mk
