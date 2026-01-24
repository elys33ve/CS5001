import os
import sys
from math import (ceil, floor)
from defines import *
import cocotb
import logging
#from cocotb.result import (TestFailure, TestSuccess, ReturnValue)
from cocotb.clock import Clock
import time
from array import array as Array
from random import (randrange, randint, sample)
from cocotb.triggers import (Timer, RisingEdge, FallingEdge, ClockCycles, ReadOnly)
from cocotb.task import Join
#from cocotb.binary import BinaryValue
from cocotb.types import LogicArray
from cocotb.utils import get_sim_time

# clocks / bitrate
AXICLK_PERIOD_PS = 3334
INTERNAL_SYNC = 0xEB
INTERNAL_HEADER_SIZE_BYTES=16

# globals
clk = None
dut_global = None
sim_end = False
data_out = []
data_in = []
channel_config = {}

arlen = 0
seq = 0
read_transaction_number = 0

#############################################
#############################################
#############################################
#############################################
#############################################
#############################################
############### coroutines ##################
#############################################
#############################################
#############################################
#############################################
#############################################
#############################################
async def init(dut, num_reset_clocks=10):
    global dut_global
    global sim_end
    global clk

    dut_global = dut

    sim_end = False
    data_in = []
    data_out = []
    
    dut._log.info("init: start clocks")
    cocotb.start_soon(Clock(dut.axiclk, AXICLK_PERIOD_PS, unit='ps').start())
    clk = dut.axiclk

    dut.axim_out_bvalid.value = 1
    dut.axim_out_bresp.value = 0
    dut.axim_in_bvalid.value = 1
    dut.axim_in_bresp.value = 0

    await RisingEdge(clk)
    # init axi buses 
    dut._log.info("init: init buses")
    init1 = cocotb.start_soon(axi_lite_init(num_reset_clocks))
    dut._log.info("init: wait for bus init")
    await init1

    await write_reg(CTRL_OFFSET, RST_MASK)

#############################################
#############################################
#############################################
#############################################
#############################################
############### bus routines ################
#############################################
#############################################
#############################################
#############################################
#############################################
### --------- axim slave read --------- ###
"""### Read Address Channel ###"""
async def axim_ar():
    cocotb.log.info("axim_ar: axim read address channel started.")
    global clk, sim_end
    global arlen
    dut = dut_global

    dut.axim_in_arready.value = 0
    frame_size_lines = channel_config["frame_size_lines"]

    while not sim_end:
        await RisingEdge(clk)
        if dut.axim_in_arvalid.value:
            dut.axim_in_arready.value = 1
            await RisingEdge(clk)
            araddr = dut.axim_in_araddr.value.to_unsigned()
            arlen = dut.axim_in_arlen.value.to_unsigned()
            #dut._log.info(f"araddr = {hex(araddr)} arlen = {arlen}")
            assert (arlen == 0) or (arlen == frame_size_lines-1)
            dut.axim_in_arready.value = 0
            await RisingEdge(clk)
            
"""### Read Data Channel ###"""
async def axim_r_variable_rvalid():
    global clk, sim_end, arlen
    global read_transaction_number
    global seq
    global data_out 
    global channel_config
    
    dut = dut_global
    data_width = 128
    test_output = ""

    frame_size_bytes = channel_config["frame_size_bytes"]
    frame_size_lines = channel_config["frame_size_lines"]

    dut.axim_in_rvalid.value = 0
    dut.axim_in_rdata.value = 0
    data_idx = 0
    transaction_idx = 0
    while not sim_end:
        await RisingEdge(clk)
        if dut.axim_in_rready.value:
            await ClockCycles(clk, 5)
            idx = arlen
            if len(data_out) - data_idx >= frame_size_lines: # there is enough data
                if (data_out[data_idx]>>(15*8)) & 0x0FF == INTERNAL_SYNC:
                    # format the internal header to what channel_out expects
                    dut.axim_in_rdata.value = reverse_bytes_128(hdr(frame_size_bytes))
                if arlen > 0:
                    data_idx += 1
            else:
                dut.axim_in_rdata.value = 0
            dut.axim_in_rvalid.value = 1
            if arlen == 0:
                dut.axim_in_rlast.value = 1
            await RisingEdge(clk)
            dut.axim_in_rvalid.value = 0
            dut.axim_in_rlast.value = 0
            await ClockCycles(clk, 5)
            skip = False
            if arlen > 0:
                while idx > 0:
                    if idx % 3 == 0 and skip:
                        await RisingEdge(clk)
                        skip = False
                    elif idx % 2 == 0:
                        skip = True
                    dut.axim_in_rdata.value = data_out[data_idx] #reverse_bytes_128(data_out[data_idx])
                    data_idx += 1
                    dut.axim_in_rvalid.value = 1
                    if idx == 1:
                        dut.axim_in_rlast.value = 1
                    await RisingEdge(clk)
                    dut.axim_in_rvalid.value = 0
                    dut.axim_in_rlast.value = 0
                    idx -= 1
                transaction_idx += 1
                seq += 1
                dut._log.info(f"transaction {transaction_idx} complete")
            read_transaction_number = transaction_idx

def get_cnt():
    global d_cnt

    d = d_cnt
    d_tmp = ((d_cnt + 0x010000000000000000000000000000000) & 0x0FF000000000000000000000000000000)
    d_tmp |= ((d_cnt + 0x0100000000000000000000000000000) & 0x0FF0000000000000000000000000000)
    d_tmp |= ((d_cnt + 0x01000000000000000000000000000) & 0x0FF00000000000000000000000000)
    d_tmp |= ((d_cnt + 0x010000000000000000000000000) & 0x0FF000000000000000000000000)
    d_tmp |= ((d_cnt + 0x0100000000000000000000000) & 0x0FF0000000000000000000000)
    d_tmp |= ((d_cnt + 0x01000000000000000000000) & 0x0FF00000000000000000000)
    d_tmp |= ((d_cnt + 0x010000000000000000000) & 0x0FF000000000000000000)
    d_tmp |= ((d_cnt + 0x0100000000000000000) & 0x0FF0000000000000000)
    d_tmp |= ((d_cnt + 0x01000000000000000) & 0x0FF00000000000000)
    d_tmp |= ((d_cnt + 0x010000000000000) & 0x0FF000000000000)
    d_tmp |= ((d_cnt + 0x0100000000000) & 0x0FF0000000000)
    d_tmp |= ((d_cnt + 0x01000000000) & 0x0FF00000000)
    d_tmp |= ((d_cnt + 0x010000000) & 0x0FF000000)
    d_tmp |= ((d_cnt + 0x0100000) & 0x0FF0000)
    d_tmp |= ((d_cnt + 0x01000) & 0x0FF00)
    d_tmp |= ((d_cnt + 0x010) & 0x0FF)
    d_cnt = d_tmp
    return d
            
async def axi_read_bus_ctrl():
    """
    handle signals such as wready during axim transactions
    """
    global dut_global
    global sim_end
    global clk
    dut = dut_global
    transaction_done = False

    dut._log.info("axi_read_bus_ctrl")
    dut.axim_out_wready.value = 1
    while not sim_end:
        while dut.axim_out_wvalid.value == 0:
            await RisingEdge(clk)
        # release wready after some cycles
        await RisingEdge(clk)
        if dut.axim_out_wlast.value:
            transaction_done = True
        dut.axim_out_wready.value = 0
        await RisingEdge(clk)
        await RisingEdge(clk)
        await RisingEdge(clk)
        dut.axim_out_wready.value = 1
        while not transaction_done:
            if dut.axim_out_wlast.value:
                transaction_done = True
            await RisingEdge(clk)
    
async def axim_out_capture():
    """
    capture the channel_in bus writes to data_in
    """
    global dut_global
    global sim_end
    global clk
    global data_in
    dut = dut_global
    transaction_done = False

    dut._log.info("axim_out_capture")
    while not sim_end:
        await FallingEdge(clk) # accounts for sim delta delays
        if (dut.axim_out_wvalid.value and dut.axim_out_wready.value and
            dut.axim_out.state.value != dut.axim_out.SDONE_DATA.value):
            data_in.append(dut.axim_out_wdata.value.to_unsigned())

async def axi_lite_init(num_reset_clocks):
    global dut_global
    global clk
    global axi_lite_device_list
    dut = dut_global
    dut._log.info("axi_lite_init()")

    # init
    dut.axi_lite_rready.value = 0
    dut.axi_lite_bready.value = 1
    dut.axi_lite_awvalid.value = 0
    dut.axi_lite_wvalid.value = 0
    dut.axi_lite_wvalid.value = 0
    dut.axi_lite_awaddr.value = 0
    dut.axi_lite_wdata.value = 0
    dut.axi_lite_arvalid.value = 0
    dut.axi_lite_wstrb.value = LogicArray('1111')
    # Reset
    dut.aresetn.value =  0
    await ClockCycles(clk, num_reset_clocks)
    dut.aresetn.value =  1
    dut._log.info("axi_lite_init done")

async def axi_lite_write(addr, data):
    global dut_global
    global clk
    dut = dut_global

    awvalid = dut.axi_lite_awvalid
    wvalid = dut.axi_lite_wvalid

    dut.axi_lite_wdata.value = data
    dut.axi_lite_awaddr.value = addr
    awvalid.value = 0
    wvalid.value = 0

    await RisingEdge(clk)
    awvalid.value = 1
    wvalid.value = 1
    await RisingEdge(clk)
    while dut.axi_lite_awready.value == 0:
        await RisingEdge(clk)
    await RisingEdge(clk)
    awvalid.value = 0
    wvalid.value = 0

    await ClockCycles(clk, 2)

async def axi_write_reg(reg, data):
    await axi_lite_write(reg<<2, data)

async def set_ctrl_bit(mask):
    global clk
    dut = dut_global
    r = int(dut.axi_lite.slv_reg0.value)
    r |= mask
    await write_reg(CTRL_OFFSET, r)
    
async def clr_ctrl_bit( mask):
    global clk
    dut = dut_global
    r = int(dut.axi_lite.slv_reg0.value)
    r &= ~mask
    await write_reg(CTRL_OFFSET, r)

async def axi_read(addr):
    global dut_global
    global clk
    dut = dut_global

    await RisingEdge(clk)
    dut.axi_lite_arvalid.value = 1
    dut.axi_lite_rready.value = 1
    dut.axi_lite_araddr.value = addr
    await RisingEdge(clk)
    dut.axi_lite_arvalid.value = 1
    dut.axi_lite_rready.value = 1
    dut.axi_lite_araddr.value = addr
    await RisingEdge(clk)
    dut.axi_lite_arvalid.value = 0
    await RisingEdge(clk)
    dut.axi_lite_arvalid.value = 0
    dut.axi_lite_rready.value = 0
    dut.axi_lite_araddr.value = 0
    await RisingEdge(clk)

async def axi_read_reg(reg):
    await axi_read(reg<<2)

async def read_reg(offset):
    global dut_global
    global clk
    dut=dut_global

    await axi_read_reg(offset)
    await RisingEdge(clk)
    await RisingEdge(clk)
    await RisingEdge(clk)
    return dut.axi_lite_rdata.value.to_unsigned()

async def write_reg(offset, data):
    global clk
    await axi_write_reg(offset, data)
    await RisingEdge(clk)
    await RisingEdge(clk)
    await RisingEdge(clk)
    
async def bitset(reg, bitmask):
    global clk
    
    regval = await read_reg(reg)
    await RisingEdge(clk)
    regval |= bitmask
    await axi_write_reg(reg, regval)
    await RisingEdge(clk)
    await RisingEdge(clk)
    
async def bitclr(reg, bitmask):
    global clk
    
    regval = await read_reg(reg)
    await RisingEdge(clk)
    regval &= ~bitmask
    await axi_write_reg(reg, regval)
    await RisingEdge(clk)
    await RisingEdge(clk)

#############################################
#############################################
#############################################
#############################################
#############################################
############### helper functions ############
#############################################
#############################################
#############################################
#############################################
#############################################
def hdr(frame_size_bytes):
    """ form a header for the axim side """
    global seq
    bitrate = 1000000
    step = 0x20000000
    r = INTERNAL_SYNC<<120
    r |= frame_size_bytes<<104
    r |= (seq<<64)
    r |= 100000<<32
    r |= step
    return r

def reverse_bytes_128(number: int) -> int:
    """
    Takes a 128-bit integer and returns its byte-reversed version.

    Args:
        number: An integer that can be represented by 128 bits (16 bytes).

    Returns:
        A new integer representing the byte-reversed value.
    """
    # 1. Convert the integer to a 16-byte sequence in big-endian order.
    original_bytes = number.to_bytes(16, byteorder='big')
    
    # 2. Reverse the sequence of bytes.
    reversed_bytes = original_bytes[::-1]
    
    # 3. Convert the reversed sequence of bytes back into an integer.
    reversed_number = int.from_bytes(reversed_bytes, byteorder='big')
    
    return reversed_number

async def config_channel():
    global clk
    global channel_config
    global dut_global

    dut = dut_global

    payload_size_bytes = channel_config["payload_size_bytes"]
    payload_size_bits = payload_size_bytes<<3
    frame_size_bytes = payload_size_bytes + INTERNAL_HEADER_SIZE_BYTES
    if frame_size_bytes & 0x0F != 0:
        frame_size_bytes &= 0xFFFFFFF0
        frame_size_bytes += 16
    frame_size_lines = frame_size_bytes>>4
    channel_config["frame_size_bytes"] = frame_size_bytes
    channel_config["frame_size_lines"] = frame_size_lines

    reg = (frame_size_lines << LINES_PER_FRAME_SHIFT) | (payload_size_bytes & PAYLOAD_SIZE_BYTES_MASK)
    await write_reg(FRAME_SIZE_OFFSET, reg)

#############################################
#############################################
#############################################
#############################################
#############################################
#############################################
#############################################
################### TESTS ###################
#############################################
#############################################
#############################################
#############################################
#############################################
#############################################
#############################################
@cocotb.test()
async def test_init(dut):
    """
    get the system into a stable starting position
    """
    global clk

    i = cocotb.start_soon(init(dut))
    await i
    await ClockCycles(clk, 100)

    # make sure reset is asserted
    assert dut.rst.value == 1
    
    # read version
    version = await read_reg(VERSION)
    dut._log.info("version = {}".format(version))

    assert version == CURRENT_VERSION

    # done
    await ClockCycles(clk, 100)
    dut._log.info("Done")

@cocotb.test()
async def test_loop(dut):
    """
    write data in, see it come out
    """
    global clk
    global channel_config
    global data_in
    global data_out

    ############################################
    # init (init resets channel_config)
    ############################################
    i = cocotb.start_soon(init(dut))
    await i
    await ClockCycles(clk, 100)

    ############################################
    # parameters
    ############################################
    PAYLOAD_SIZE_BYTES = 2048
    channel_config = {
        "payload_size_bytes" : PAYLOAD_SIZE_BYTES
    }
    NUM_FRAMES = 6
    PER_FRAME_DELAY_CLKS = 1000

    ############################################
    # configure the datapath
    ############################################
    dut._log.info("configuring the system")
    await config_channel()
    # a line is 128 bits / 16 bytes
    num_bytes = NUM_FRAMES * (channel_config["frame_size_lines"] * 16)

    ############################################
    # axim
    ############################################
    cocotb.start_soon(axi_read_bus_ctrl())
    cocotb.start_soon(axim_out_capture())

    dut.axim_out_awready.value = 1
    dut.axim_in_awready.value = 1
    dut.axim_in_wready.value = 1
    dut.axim_in_bvalid.value = 1
    dut.axim_in_arready.value = 1
    dut.axim_out_arready.value = 1
    dut.axim_out_rvalid.value = 1
    dut.axim_out_rdata.value = 0
    dut.axim_out_rlast.value = 1

    await ClockCycles(clk, 100)

    ############################################
    # start process to write data to axim_in 
    # and capture data from axim_out
    ############################################
    cocotb.start_soon(axim_ar())
    cocotb.start_soon(axim_r_variable_rvalid())
    
    ############################################
    # Take devices out of reset and start the
    # test
    ############################################
    dut._log.info("starting the system")
    await bitclr(CTRL_OFFSET, RST_MASK)
    await bitset(CTRL_OFFSET, AXIM_NRST_MASK)

    idx = 0
    seq = 1
    for f in range(NUM_FRAMES):
        dut._log.info(f"Frame {seq-1}")
        for x in range(channel_config["frame_size_lines"]):
            data_out.append(randint(0, 2**128))
        data_out[idx] = (INTERNAL_SYNC<<(15*8)) | (PAYLOAD_SIZE_BYTES<<(13*8)) | (seq<<(8*8))
        idx += channel_config["frame_size_lines"]
        seq += 1
        await ClockCycles(clk, PER_FRAME_DELAY_CLKS)
        while not dut.axim_in_fifo_empty.value:
            await RisingEdge(clk)

    ############################################
    # final processing and reporting
    ############################################
    dut._log.info(f"len(data_in) = {len(data_in)}")
    dut._log.info(f"len(data_out) = {len(data_out)}")

    for x in range(10):
        dut._log.info(f"{hex(data_out[x])}, {hex(data_in[x])}")
        

    ############################################
    # done
    ############################################
    await ClockCycles(clk, 100)
    dut._log.info("Done")

