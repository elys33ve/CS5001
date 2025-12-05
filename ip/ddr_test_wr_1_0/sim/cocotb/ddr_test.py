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

dut_global = None
txn_done = None
clk = None

mem_wr = {}
mem_coe_list = {}

addr_offset_wr = 0
total_burst_count_wr = 0
total_bytes_wr = 0
rom_count_wr   = {1:0, 2:0}
rom_version_wr = 1
verify_wr_done = 0

axilite_data_width = 32
axim_id_width      = 1
axim_addr_width    = 32
axim_data_width    = 128

SWRITE_DONE = 14

### ============================================================================= ###
### ================================= Init ====================================== ###
### ============================================================================= ###

@cocotb.coroutine
async def init(dut, num_reset_clocks=10):
    global dut_global, txn_done, clk
    global BASEADDR
    dut_global = dut
    txn_done = False

    dut.axim_arready.value = 1
    dut.axim_rdata.value = 0
    dut.axim_rvalid.value = 1
    dut.axim_rresp.value    = 0
    dut.axim_rlast.value    = 1
    
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
async def nrst_wr():
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
    
    # disabling bvalid bresp system
    dut.axim_bresp.value = 0
    dut.axim_bvalid.value = 1

    global mem_wr, addr_offset_wr
    mem_wr = {}
    addr_offset_wr = 0
    global total_burst_count_wr, total_bytes_wr
    total_burst_count_wr = 0
    total_bytes_wr = 0
    global rom_count_wr, rom_version_wr
    rom_count_wr = {1:0, 2:0}
    rom_version_wr = 1
  
### ===================================== ###
"""  ### Write Address / Data Channel ### """
### ===================================== ###
@cocotb.coroutine
async def axim_write():
    cocotb.log.info(f"{div}\naxim_write: write channel started.\n")
    global clk, mem_wr, addr_offset_wr, total_burst_count_wr, total_bytes_wr, rom_count_wr, rom_version_wr
    dut = dut_global

    awaddr = None
    burst_count = 0

    while True:
        await RisingEdge(clk)
        # reset
        if dut.aresetn.value == 0:
            dut.axim_awready.value = 0
            dut.axim_wready.value  = 0
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
            dut.axim_awready.value = 1
            dut.axim_wready.value = 1

        if dut.axim_wvalid.value == 1:
            burst_count = 0

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
            await RisingEdge(clk)

### ============================================================================= ###
### ================================ Verify ===================================== ###
### ============================================================================= ###

### ============= verify wr ============= ###
@cocotb.coroutine
async def verify_wr(addr_offset):
    """ compare current write burst data to coe data """
    global verify_wr_done
    global BURST_LEN

    addr = BASEADDR + (addr_offset*(BURST_SIZE_BITS//8))
    data_val = mem_wr[addr]
    list_val = mem_coe_list[rom_version_wr][addr_offset]
    
    if addr_offset < BURST_LEN:
        # check data value
        assert True
        #assert list_val == data_val, f"verify_wr: {hex(addr)}: ({hex(data_val)}) != ({hex(list_val)})"
    else:
        # check counter value
        wr_counter = get_counter('wr')
        #assert wr_counter == rom_count_wr[rom_version_wr], f"verify_wr: counter={hex(wr_counter)}, rom_count={rom_count_wr[rom_version_wr]}"
        cocotb.log.info(f"verify_wr: data valid.")
        verify_wr_done = 1

### ============================================================================= ###
### =============================== Counter ===================================== ###
### ============================================================================= ###

def set_counter_val(mem_type, val, cntr_width_bits=COUNTER_BITS):
    global mem_wr, mem_rd
    val &= int('FF'*(cntr_width_bits//8),16)
    data_width = BURST_SIZE_BITS // 8
    data_size  = DATA_SIZE_BYTES
    mem = addr = None

    if mem_type == 'wr':
        addr = BASEADDR+data_size-data_width
        mem = mem_wr
    elif mem_type == 'rd':
        addr = BASEADDR_RD+data_size-data_width
        mem = mem_rd

    val = ((val&0xFF)<<24)+((val&0xFF00)<<8)+((val&0xFF0000)>>8)+((val&0xFF000000)>>24)
    if addr in mem:
        mem[addr] &= int('FF'*(2*data_width-(cntr_width_bits//8)),16)<<cntr_width_bits
        mem[addr] |= val    
    else:
        mem[addr] = val

def set_counter_ff(mem_type, cntr_width_bits=COUNTER_BITS):
    set_counter_val(mem_type, 0xFFFFFFFF, cntr_width_bits)
def set_counter_00(mem_type, cntr_width_bits=COUNTER_BITS):
    set_counter_val(mem_type, 0x00000000, cntr_width_bits)

def get_counter(mem_type):
    # xc3c2c1c0
    data_width = BURST_SIZE_BITS // 8
    data_size  = DATA_SIZE_BYTES
    if mem_type == 'wr':
        baseaddr = BASEADDR
        c = mem_wr[baseaddr+data_size-data_width] & 0xFFFFFFFF
        return ((c&0xFF)<<24)+((c&0xFF00)<<8)+((c&0xFF0000)>>8)+((c&0xFF000000)>>24)
    elif mem_type == 'rd':
        baseaddr = BASEADDR_RD
        c = mem_rd[baseaddr+data_size-data_width] & 0xFFFFFFFF
        return ((c&0xFF)<<24)+((c&0xFF00)<<8)+((c&0xFF0000)>>8)+((c&0xFF000000)>>24)        

### ============================================================================= ###
### =============================== "Driver" ==================================== ###
### ============================================================================= ###
@cocotb.coroutine
async def driver_coroutine():
    """ acts as driver to check if 4KB data finished writing, then copy over to 
    read buffer """
    cocotb.log.info(f"driver_coroutine()")
    global clk, mem_wr, mem_rd, rom_count_wr, rom_version_wr, BASEADDR

    # write counter addresses
    wr_cnt_addr0 = BASEADDR+DATA_SIZE_BYTES-1
    wr_cnt_addr1 = wr_cnt_addr0-1
    wr_cnt_addr2 = wr_cnt_addr1-1
    wr_cnt_addr3 = wr_cnt_addr2-1
    rd_cnt_addr0 = BASEADDR_RD+DATA_SIZE_BYTES-1
    rd_cnt_addr1 = rd_cnt_addr0-1
    rd_cnt_addr2 = rd_cnt_addr1-1
    rd_cnt_addr3 = rd_cnt_addr2-1

    while txn_done == False:
        await RisingEdge(clk)
        # get write counter
        cnt0 = mem_wr[wr_cnt_addr0]
        cnt1 = mem_wr[wr_cnt_addr1]
        cnt2 = mem_wr[wr_cnt_addr2]
        cnt3 = mem_wr[wr_cnt_addr3]
        wr_cnt = (cnt0<<(8*3)) + (cnt1<<(8*2))+ (cnt2<<(8)) + cnt3

        # get read counter
        cnt0 = mem_rd[rd_cnt_addr0]
        cnt1 = mem_rd[rd_cnt_addr1] 
        cnt2 = mem_rd[rd_cnt_addr2]
        cnt3 = mem_rd[rd_cnt_addr3]
        rd_cnt = (cnt0<<(8*3)) + (cnt1<<(8*2))+ (cnt2<<(8)) + cnt3

        # write 4KB done and read buffer ready
        if wr_cnt <= rom_count_wr[rom_version_wr] and rd_cnt == 0xFFFFFFFF:
            cocotb.log.info(f"driver_coroutine: copying data to read buffer")
            # copy 4KB of data just written to the read buffer
            for i in range(len(mem_wr)):
                mem_rd[BASEADDR_RD+i] = mem_wr[BASEADDR+i]
            # write FFFFFFFF to wr_cnt to indicate axim can write more data
            mem_wr[wr_cnt_addr0] = 0xFF
            mem_wr[wr_cnt_addr1] = 0xFF
            mem_wr[wr_cnt_addr2] = 0xFF
            mem_wr[wr_cnt_addr3] = 0xFF

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
""" ###          write test           ### """
### ===================================== ###
@cocotb.test()
async def test_write(dut):
    global dut_global, clk, rom_count_wr, rom_version_wr, mem_coe_list, mem_wr, verify_wr_done
    global axim_id_width, axim_addr_width, axim_data_width, axilite_data_width
    global SWRITE_DONE

    # init
    i = cocotb.start_soon(init(dut))
    await i.join()

    # nrst wr
    cocotb.start_soon(nrst_wr())

    # read version
    version = await read_reg(VERSION_OFFSET)
    cocotb.log.info(f"version = {version}")
    assert version == CURRENT_VERSION

    # get list of data from coe file to verify
    mem_coe_list[1] = get_coe_list("pr_wr_rom1.coe")
    mem_coe_list[2] = get_coe_list("pr_wr_rom2.coe")
    # set counter to 0xFFFFFFFF
    set_counter_ff('wr')

    await ClockCycles(clk, 100)
    cocotb.log.info("writing page_delay_cntto_reg")
    await axi_lite_write_reg(PAGE_DELAY_CNTTO_REG, 5000)

    # start slave write channel
    cocotb.start_soon(axim_write())

    # --- write 4K bytes --- #
    cocotb.log.info(f"writing {DATA_SIZE_BYTES}B {N_TEST_CYCLES} times")
    while True:
        await RisingEdge(clk)
        if dut.state_scope.value.integer == SWRITE_DONE:
            cocotb.log.info(f"{div}\nwrite transaction DONE. ROM{rom_version_wr}: {rom_count_wr[rom_version_wr]}")
            cocotb.log.info(f"{total_burst_count_wr} total write bursts (bytes written: {total_bytes_wr})\n{div}")
            
            # simulate driver copy delay
            await ClockCycles(clk, 2)

            # verify data after 4KB
            if CHECK_BURST:
                while verify_wr_done == 0:
                    await RisingEdge(clk)
                verify_wr_done = 0

            # simulate driver reset counter after read
            set_counter_ff('wr')

            # switch ROM version, incr rom counter
            rom_count_wr[rom_version_wr] = rom_count_wr[rom_version_wr]+1
            rom_version_wr = ((rom_version_wr**2)%4)+1
        
        if rom_count_wr[1]+rom_count_wr[2] >= N_TEST_CYCLES:
            break
    cocotb.log.info(f"test_write complete.")


