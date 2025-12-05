TOPLEVEL_LANG ?= vhdl
PWD=$(shell pwd)
HDLDIR=$(PWD)/../../hdl
SRCDIR=$(PWD)/../src
SIM ?= riviera
GUI ?= 0

# entity in vhdl file
TOPLEVEL = ddr_test_rd
# vhdl files
VHDL_SOURCES = $(HDLDIR)/ddr_test_rd_axim.vhd
VHDL_SOURCES += $(HDLDIR)/ddr_test_rd_axi_lite.vhd
VHDL_SOURCES += $(HDLDIR)/ddr_test_rd.vhd

# verilog
VERILOG_SOURCES += $(SRCDIR)/pr_rd_rom1.v
VERILOG_SOURCES += $(SRCDIR)/pr_rd_rom2.v

# python testbench
MODULE = ddr_test

SIM_ARGS += -L blk_mem_gen_v8_4_11 -L fifo_generator_v13_2_13 -L unisims_ver -L unimacro_ver -L secureip -L xpm -ieee_nowarn
include $(shell cocotb-config --makefiles)/Makefile.sim
GPI_EXTRA = $(shell cocotb-config --lib-name-path vhpi riviera):cocotbvhpi_entry_point

