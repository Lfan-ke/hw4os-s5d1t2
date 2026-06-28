# Simple GNU Make feature test for StarryOS (#764 verilog <!-- ...,gnumake -->).
# Exercises: variables (:=), functions ($(addprefix)/$(words)/$(sort)), pattern
# rules with automatic vars ($*), prerequisites/ordering, and recipe shell-exec
# (each recipe line forks /bin/sh in the rootfs). Deterministic output -> golden.
NAMES := gamma alpha beta
SORTED := $(sort $(NAMES))
OUT    := $(addprefix built-,$(SORTED))

all: $(OUT) summary

built-%:
	@echo "  make: building $*"

summary: $(OUT)
	@echo "MAKE_NAMES=$(words $(NAMES))"
	@echo "MAKE_SORTED=$(SORTED)"
	@echo "GNUMAKE_BUILD_OK"
