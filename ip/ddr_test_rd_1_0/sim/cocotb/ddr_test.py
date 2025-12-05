import cocotb, logging, os
from defines import *
from helpers import *
from cocotb.clock import Clock
from cocotb.triggers import (RisingEdge, ClockCycles)
from cocotb.binary import BinaryValue

### ============================================================================= ###
cocotb.log.setLevel(logging.INFO)   # configure logging
### ============================================================================= ###
PRINT_TXN   = 1
CHECK_BURST = 1

N_TEST_CYCLES = 1

### ============================================================================= ###

SREAD_DONE = 8

dut_global = None
txn_done = None
clk = None

mem_rd = {}
mem_coe_list = {}

addr_offset_rd = 0
total_burst_count_rd = 0
total_bytes_rd = 0
rom_count_rd   = {1:0, 2:0}
rom_version_rd = 1
verify_rd_done = 0

axilite_data_width = 32
axim_id_width      = 1
axim_addr_width    = 32
axim_data_width    = 128

### ============================================================================= ###
### ================================= Init ====================================== ###
### ============================================================================= ###

@cocotb.coroutine
async def init(dut, num_reset_clocks=10):
    global dut_global, txn_done, clk
    global BASEADDR
    dut_global = dut
    txn_done = False

    # start clocks
    cocotb.log.info("init: start clocks")
    cocotb.start_soon(Clock(dut.axiclk, AXICLK_PERIOD_PS, units='ps').start())
    clk = dut.axiclk
    
    # init axi buses
    cocotb.log.info("init: init buses")
    init_lite_wr = cocotb.start_soon(axi_lite_init())
    init_axim = cocotb.start_soon(axim_init())
    cocotb.log.info("init: wait for bus init")
    await init_lite_wr.join()
    await init_axim.join()

    # reset
    dut.aresetn.value = 0   # hold reset
    await ClockCycles(clk, num_reset_clocks)
    dut.aresetn.value = 1   # release reset
    await ClockCycles(clk, 5)
    
    # set memory addr and burst len 
    await axi_lite_write_reg(BASEADDR_REG, BASEADDR)
    # leave in reset
    await ClockCycles(clk, 50)

### ----- nrst
@cocotb.coroutine
async def nrst_rd():
    global clk
    await write_reg(CTRL_OFFSET, 0)
    RisingEdge(clk)
    await write_reg(CTRL_OFFSET, 1)

### ============================================================================= ###
### ================================= AXI-Lite ================================== ###
### ============================================================================= ###

""" --------- axi lite init ---------- """
@cocotb.coroutine
async def axi_lite_init():
    dut = dut_global
    cocotb.log.info("axi_lite_init()")
    # -- ddr_test_wr
    dut.axi_lite_bready.value   = 1
    dut.axi_lite_awvalid.value  = 0
    dut.axi_lite_wvalid.value   = 0
    dut.axi_lite_awaddr.value   = 0
    dut.axi_lite_wdata.value    = 0
    dut.axi_lite_rready.value   = 0
    dut.axi_lite_arvalid.value  = 0
    dut.axi_lite_wstrb.value    = BinaryValue('1111')

""" ============== WR ================ """
### --------- axi-lite write --------- ###
@cocotb.coroutine
async def axi_lite_write(addr, data):
    #cocotb.log.info(f"axi_lite_write({hex(addr)}, {hex(data)})")
    global dut_global, clk
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

### --------- axi-lite write reg --------- ###
@cocotb.coroutine
async def axi_lite_write_reg(reg, data):
    await axi_lite_write(reg<<2, data)
@cocotb.coroutine
async def write_reg(offset, data):
    global clk
    await axi_lite_write_reg(offset, data)
    await RisingEdge(clk)
    await RisingEdge(clk)
    await RisingEdge(clk)

### --------- axi-lite read --------- ###
@cocotb.coroutine
async def axi_lite_read(addr):
    #cocotb.log.info(f"axi_lite_read({hex(addr)})")
    global clk
    dut = dut_global
    dut.axi_lite_arvalid.value  = 1
    dut.axi_lite_rready.value   = 1
    dut.axi_lite_araddr.value   = addr
    await RisingEdge(clk)
    dut.axi_lite_arvalid.value  = 1
    dut.axi_lite_rready.value   = 1
    dut.axi_lite_araddr.value   = addr
    await RisingEdge(clk)
    dut.axi_lite_arvalid.value  = 0
    await RisingEdge(clk)
    dut.axi_lite_arvalid.value  = 0
    dut.axi_lite_rready.value   = 0
    dut.axi_lite_araddr.value   = 0
    await RisingEdge(clk)

### --------- axi-lite read reg --------- ###
@cocotb.coroutine
async def axi_lite_read_reg(reg):
    await axi_lite_read(reg<<2)
@cocotb.coroutine
async def read_reg(offset):
    global clk
    dut = dut_global
    await axi_lite_read_reg(offset)
    await RisingEdge(clk)
    await RisingEdge(clk)
    await RisingEdge(clk)
    return dut.axi_lite_rdata.value.integer

### ============================================================================= ###
### =================================== AXIM ==================================== ###
### ============================================================================= ###

""" ------------- axim init ------------- """
@cocotb.coroutine
async def axim_init():
    dut = dut_global
    cocotb.log.info("axim_init()")
    dut.axim_awready.value  = 0
    dut.axim_wready.value   = 0
    dut.axim_bresp.value    = 0
    dut.axim_bvalid.value   = 0
    dut.axim_arready.value  = 0
    dut.axim_rdata.value    = 0
    dut.axim_rresp.value    = 0
    dut.axim_rlast.value    = 0
    dut.axim_rvalid.value   = 0
    
    global mem_wr, mem_rd, addr_offset_wr, addr_offset_rd
    mem_wr = mem_rd = {}
    addr_offset_wr = addr_offset_rd = 0
    global total_burst_count_wr, total_burst_count_rd, total_bytes_wr, total_bytes_rd
    total_burst_count_wr = total_burst_count_rd = 0
    total_bytes_wr = total_bytes_rd = 0
    global rom_count_wr, rom_count_rd, rom_version_wr, rom_version_rd
    rom_count_wr = rom_count_rd = {1:0, 2:0}
    rom_version_wr = rom_version_rd = 1
  
### ===================================== ###
"""  ### Read Address / Data Channel ###  """
### ===================================== ###
@cocotb.coroutine
async def axim_read():
    cocotb.log.info(f"{div}\naxim_ar: axim read channel started.\n")
    global clk, mem_rd, addr_offset_rd, total_burst_count_rd, total_bytes_rd, rom_count_rd, rom_version_rd, verify_rd_done
    dut = dut_global

    araddr = None
    burst_count = read_cycles = 0
    
    while True:
        await RisingEdge(clk)
        # reset
        if dut.aresetn.value == 0:
            dut.axim_arready.value = 0
            dut.axim_rdata.value   = 0
            dut.axim_rresp.value   = 0
            dut.axim_rlast.value   = 0
            dut.axim_rvalid.value  = 0
            addr_offset_rd = total_burst_count_rd = total_bytes_rd = 0
            burst_count = read_cycles = 0
            mem_rd         = {}
            rom_count_rd   = {1:0, 2:0}
            rom_version_rd = 1
            araddr = None
            continue

        # de-assert ARREADY for ARADDR change
        if araddr != dut.axim_araddr.value.integer:
            dut.axim_arready.value = 0
            addr_offset_rd = 0
        # if ARVALID, check if read buffer ready, then assert ARREADY
        if dut.axim_arvalid.value == 1:
            if get_counter('rd') == rom_count_rd[rom_version_rd]:
                araddr = dut.axim_araddr.value.integer
                size_bytes = 2**dut.axim_arsize.value.integer
                burst_len = dut.axim_arlen.value.integer + 1
                dut.axim_arready.value = 1

        # if RREADY, read
        if dut.axim_rready.value == 1:
            cocotb.log.info(f"axim_read: ARADDR={hex(int(dut.axim_araddr.value))}, ARLEN={burst_len}\n")

            # simulate processing delay
            await ClockCycles(clk, 2)

            for i in range(burst_len):
                # retrieve data from the memory model
                addr = araddr + addr_offset_rd
                read_data = mem_rd.get(addr, 0)
                
                # assert RVALID and read data
                dut.axim_rdata.value  = BinaryValue(value=read_data, n_bits=axim_data_width, bigEndian=False)
                dut.axim_rresp.value  = 0  # OKAY response
                dut.axim_rvalid.value = 1
                dut.axim_rlast.value  = 1 if i+1 == burst_len else 0

                if PRINT_TXN:
                    cocotb.log.info(f"axim_rd: ({total_burst_count_rd}) addr={hex(addr)}, {hex(read_data)}")
                
                addr_offset_rd += size_bytes
                total_bytes_rd += size_bytes
                total_burst_count_rd += 1
                burst_count += 1
                await RisingEdge(clk)

            addr_offset_rd = burst_count = 0
            dut.axim_arready.value = 0
            dut.axim_rvalid.value = 0
            read_cycles += 1

            if CHECK_BURST:
                verify = cocotb.start_soon(verify_rd())
                await verify.join()
                while verify_rd_done == 1:
                    await RisingEdge(clk)

### ===================================== ###
"""  ### Write Address / Data Channel ### """
### ===================================== ###
@cocotb.coroutine
async def axim_write():
    cocotb.log.info(f"{div}\naxim_write: write channel started.\n")
    global clk, mem_wr, addr_offset_wr, total_burst_count_wr, total_bytes_wr, rom_count_wr, rom_version_wr
    dut = dut_global
    axim_wr = getattr(dut, "axim_wr")

    awaddr = None
    burst_count = 0

    while True:
        await RisingEdge(clk)
        # reset
        if dut.aresetn.value == 0:
            dut.axim_awready.value = 0
            dut.axim_wready.value  = 0
            dut.axim_bresp.value   = 0
            dut.axim_bvalid.value  = 0
            addr_offset_wr = total_burst_count_wr = total_bytes_wr = 0
            mem_wr         = {}
            rom_count_wr   = {1:0, 2:0}
            rom_version_wr = 1
            awaddr = None

        # de-assert AWREADY for AWADDR change
        if awaddr != dut.axim_awaddr.value.integer:
            dut.axim_awready.value = 0
            addr_offset_wr = 0
        # wait for AWVALID and buffer ready to assert AWREADY
        if dut.axim_awvalid.value == 1 and get_counter('wr') == 0xFFFFFFFF:
            awaddr = dut.axim_awaddr.value.integer
            size_bytes = 2**dut.axim_awsize.value.integer
            burst_len = dut.axim_awlen.value.integer + 1
            dut.axim_awready.value = 1
            dut.axim_wready.value = 1

        #cocotb.log.info(f"axim_wr: awvalid={dut.axim_awvalid.value.integer}, awready={dut.axim_awready.value.integer}, wvalid={dut.axim_awready.value.integer}, wready={dut.axim_wready.value.integer}")
        #cocotb.log.info(f"axim_wr: rom_ver={axim_wr.rom_ver.value}, state={states_wr[str(axim_wr.state.value)]}")

        if dut.axim_wvalid.value == 1:
            burst_count = 0
            cocotb.log.info(f"axim_wr: awaddr={hex(awaddr)}, burst_len={burst_len}, size={size_bytes}B\n")

            while True:
                await RisingEdge(clk)
                # write data to memory model
                addr = awaddr + addr_offset_wr
                write_data = dut.axim_wdata.value.integer
                mem_wr[addr] = write_data

                if PRINT_TXN:
                    cocotb.log.info(f"axim_wr: ({total_burst_count_wr}) addr={hex(addr)}, {hex(write_data)}")
                if CHECK_BURST:
                    verify = cocotb.start_soon(verify_wr(burst_count))
                    await verify.join()

                addr_offset_wr += size_bytes
                total_bytes_wr += size_bytes
                total_burst_count_wr += 1
                burst_count += 1

                if dut.axim_wlast.value == 1:
                    cocotb.log.info(f"axim_wr: burst complete.")
                    break
            
            dut.axim_awready.value = 0
            dut.axim_wready.value = 0
            addr_offset_wr = 0
            burst_count = 0
            cocotb.start_soon(axim_b())
            await RisingEdge(clk)

"""### Write Response Channel ###"""
@cocotb.coroutine
async def axim_b():
    global clk
    dut = dut_global
    while dut.axim_bready.value == 1:
        dut.axim_bresp.value = 0
        dut.axim_bvalid.value = 1
        await RisingEdge(clk)
    dut.axim_bvalid.value = 0


### ============================================================================= ###
### ================================ Tests ====================================== ###
### ============================================================================= ###

### ===================================== ###
""" ###           init test           ### """
### ===================================== ###
@cocotb.test()
async def test_init(dut):
    global dut_global, clk
    global axim_id_width, axim_addr_width, axim_data_width, axilite_data_width

    # init
    i = cocotb.start_soon(init(dut))
    await i.join()

    # read version
    version = await read_reg(VERSION_OFFSET)
    cocotb.log.info(f"version = {version}")
    assert version == CURRENT_VERSION

    # done
    await ClockCycles(clk, 100)
    cocotb.log.info("test_init finished.")


### ===================================== ###
""" ###           read test           ### """
### ===================================== ###
@cocotb.test()
async def test_read(dut):
    global dut_global, clk, rom_count_rd, rom_version_rd, mem_coe_list, mem_rd, verify_rd_done
    global axim_id_width, axim_addr_width, axim_data_width, axilite_data_width
    global SREAD_DONE

    # init
    i = cocotb.start_soon(init(dut))
    await i.join()

    # nrst rd
    cocotb.start_soon(nrst_rd())

    # read version
    version = await read_reg(VERSION_OFFSET)
    cocotb.log.info(f"version = {version}")
    assert version == CURRENT_VERSION

    # get list of data from coe file to verify
    mem_coe_list[1] = get_coe_list("pr_wr_rom1.coe")
    mem_coe_list[2] = get_coe_list("pr_wr_rom2.coe")
    # create mem buffer and fill with rom data
    mem_buffer    = {}
    mem_buffer[1] = fill_mem_data_rom("pr_wr_rom1.coe", bit_width=BURST_SIZE_BITS, data_size=DATA_SIZE_BYTES)
    mem_buffer[2] = fill_mem_data_rom("pr_wr_rom2.coe", bit_width=BURST_SIZE_BITS, data_size=DATA_SIZE_BYTES)
    mem_rd = mem_buffer[1]
    print(f"mem_buffer[1]")

    # start slave read channel
    cocotb.start_soon(axim_read())

    # --- read 4K bytes --- #
    cocotb.log.info(f"reading {DATA_SIZE_BYTES}B {N_TEST_CYCLES} times")
    while True:
        await RisingEdge(clk)
        if dut.state_scope.value.integer == SREAD_DONE:
            cocotb.log.info(f"{div}\nread transaction DONE. ROM{rom_version_rd}: {rom_count_rd[rom_version_rd]}")
            cocotb.log.info(f"{total_burst_count_rd} total read bursts (bytes read: {total_bytes_rd})\n{div}")

            # verify data
            if CHECK_BURST:
                await RisingEdge(clk)
                while verify_rd_done == 1:
                    await RisingEdge(clk)

            # set counter buffer back to 0xFFFFFFFF
            set_counter_ff('rd')

            # switch ROM version, incr rom counter
            rom_count_rd[rom_version_rd] = rom_count_rd[rom_version_rd]+1
            rom_version_rd = ((rom_version_rd**2)%4)+1
            mem_rd = mem_buffer[rom_version_rd]     # change memory buffer rom data

            # simulate copy 4KB of data just written to the read buffer
            await ClockCycles(clk, 2)
            set_counter_val('rd', rom_count_rd[rom_version_rd])

        if rom_count_rd[1]+rom_count_rd[2] >= N_TEST_CYCLES:
            break
    cocotb.log.info(f"test_read complete.")
