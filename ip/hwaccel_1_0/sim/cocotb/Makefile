TOPLEVEL_LANG ?= vhdl
PWD=$(shell pwd)
HDLDIR=$(PWD)/../../hdl
SRCDIR=../src
SIM ?= riviera
GUI ?= 0

TOPLEVEL = hwaccel

#DUT
VHDL_SOURCES = $(HDLDIR)/hwaccel_axim_in.vhd
VHDL_SOURCES += $(HDLDIR)/axim_out_buffer.vhd
VHDL_SOURCES += $(HDLDIR)/hwaccel_axim_out.vhd
VHDL_SOURCES += $(HDLDIR)/hwaccel_axi_lite.vhd
VHDL_SOURCES += $(HDLDIR)/hwaccel.vhd

VERILOG_SOURCES = $(SRCDIR)/axim_in_fifo.v
VERILOG_SOURCES += $(SRCDIR)/axim_out_fifo.v

#export TOPLEVEL_LANG
COCOTB_TEST_MODULES=hwaccel
MODULE=hwaccel

SIM_ARGS += -L fifo_generator_v13_2_13 -L xil_defaultlib -L unisims_ver -L unimacro_ver -L secureip -L xpm -ieee_nowarn 
include $(shell cocotb-config --makefiles)/Makefile.sim
GPI_EXTRA = $(shell cocotb-config --lib-name-path vhpi riviera):cocotbvhpi_entry_point


