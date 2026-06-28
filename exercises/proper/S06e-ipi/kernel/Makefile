LABROOT := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $(CURDIR)/../../../..)
K := $(LABROOT)/common/kernel
# IPI 走 S 态软件中断，需复用 common 的 trap 入口(__alltraps)；多核启核用 SBI(HSM)。
KSRC := $(K)/entry.S $(K)/console.c $(K)/sbi.c $(K)/trap.S
include $(LABROOT)/common/mk/kernel.mk
