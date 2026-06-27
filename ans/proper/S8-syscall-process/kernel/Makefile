LABROOT := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $(CURDIR)/../../../..)
K := $(LABROOT)/common/kernel
KSRC := $(K)/entry.S $(K)/console.c $(K)/sbi.c $(K)/trap.S
include $(LABROOT)/common/mk/kernel.mk
