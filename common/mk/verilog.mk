# 共享 Verilog 配方 —— 仅供人用（看波形/结构/lint）。
# 判题由 labctl 直接调 iverilog，不经此 Makefile。
#
# 实验目录的 Makefile 只需设置 TOP / DUT 后 include 本文件：
#   TOP := tb_vlan      # testbench 顶层
#   DUT := vlan_proc    # 被测模块（synth/lint 目标）
#   include $(LABROOT)/common/mk/verilog.mk

TOP   ?= tb
DUT   ?= dut
SRCS  ?= $(wildcard *.v)
BUILD ?= sim_build
WAVE  ?= $(TOP).vcd

.PHONY: sim wave synth lint clean help
help:
	@echo "make sim    跑仿真（iverilog -g2012 -Wall + vvp，与判题同口径）"
	@echo "make wave   gtkwave 看波形（$(WAVE)）"
	@echo "make synth  yosys 综合 + 看硬件结构（$(DUT)）"
	@echo "make lint   verilator 静态检查（0 warning）"
	@echo "make clean  清理"

$(BUILD):
	@mkdir -p $(BUILD)

sim: $(BUILD)/sim.vvp
	vvp $(BUILD)/sim.vvp

$(BUILD)/sim.vvp: $(SRCS) | $(BUILD)
	iverilog -g2012 -Wall -o $@ $(SRCS)

wave: sim
	gtkwave $(WAVE)

synth: | $(BUILD)
	yosys -p "read_verilog $(DUT).v; hierarchy -top $(DUT); proc; opt; stat; \
	          show -prefix $(BUILD)/$(DUT) -format svg -notitle" \
	  || yosys -p "read_verilog $(DUT).v; hierarchy -top $(DUT); proc; opt; stat"

lint:
	verilator --lint-only -Wall $(DUT).v

clean:
	rm -rf $(BUILD) *.vcd
