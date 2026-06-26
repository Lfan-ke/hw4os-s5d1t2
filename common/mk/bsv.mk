# 共享 BlueSpec 配方 —— 仅供人用（仿真/看波形）。
# 判题由 labctl 直接调 bsc，不经此 Makefile。
#
# 实验目录的 Makefile 只需设置 TOP / ENTRY 后 include 本文件：
#   TOP   := mkTbVlan     # 仿真顶层模块
#   ENTRY := VlanProc.bsv # 顶层 .bsv
#   include $(LABROOT)/common/mk/bsv.mk

TOP   ?= mkTb
ENTRY ?= $(firstword $(wildcard *.bsv))
BUILD ?= sim_build
WAVE  ?= $(TOP).vcd

.PHONY: sim wave clean help
help:
	@echo "make sim    Bluesim 仿真（与判题同口径）"
	@echo "make wave   产生 VCD 并用 gtkwave 查看"
	@echo "make clean  清理"

$(BUILD)/sim: $(ENTRY)
	@mkdir -p $(BUILD)
	bsc -sim -bdir $(BUILD) -simdir $(BUILD) -info-dir $(BUILD) -u -g $(TOP) $(ENTRY)
	bsc -sim -bdir $(BUILD) -simdir $(BUILD) -e $(TOP) -o $(BUILD)/sim

sim: $(BUILD)/sim
	$(BUILD)/sim

wave: $(BUILD)/sim
	$(BUILD)/sim -V $(WAVE)
	gtkwave $(WAVE)

clean:
	rm -rf $(BUILD) *.vcd
