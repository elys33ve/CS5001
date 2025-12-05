import random
from defines import *

### ============================================================================= ###
div = "------------------------------------"
states_wr = {'0000':'SRST', '0001':'SSTART_BURST', '0010':'SWRITE_DATA', '0011':'SBURST_DONE', '0100':'SWRITE_DONE'}
states_rd = {'0000':'SRST', '0001':'SSTART_BURST', '0010':'SREADING', '0011':'SBURST_DONE', '0100':'SREAD_DONE'}
states_lite_wr = {'00':'Idle', '10':'Waddr', '11':'Waddr'}
states_lite_rd = {'00':'Idle', '10':'Raddr', '11':'Rdata'}
responses = ['OKAY', 'EXOKAY', 'SLVERR', 'DECERR']
bursts = ['FIXED', 'INCR', 'WRAP', '']
### ============================================================================= ###

###--- print mem dictionary
def show_mem(mem):
    addresses = list(mem.keys())
    for addr in addresses:
        print(f"\t{hex(addr)}: {mem[addr]}")

###--- fill a dictionary for memory model with incrementing data
def fill_mem_data_incr(size_bytes, start_addr=0x70000000, data_bits=8):
    mem = {}
    for i in range(size_bytes):
        addr = int(start_addr + ((data_bits/8)*i))
        mem[addr] = i
    return mem

###--- fill a dictionary for memory model with ROM data (coe file)
def fill_mem_data_rom(filename, bit_width, data_size, base_addr=0x70000000, cntr_width_bits=32):
    mem = {}
    idx = 0
    data_width = bit_width // 8
    with open(filename, 'r') as rom:
        for line in rom:
            if "memory_initialization" not in line:
                data = line.replace(',','').replace(';','').strip()
                mem[base_addr+idx] = int(data, 16)
                idx += (data_width)
    # set last counter bits to 0
    mem[base_addr+data_size-data_width] &= int('FF'*(2*data_width-(cntr_width_bits//8)),16)<<cntr_width_bits
    return mem

###--- read COE file into list
def get_coe_list(filename):
    hex_list = []
    with open(filename, 'r') as f:
        for line in f:
            if "memory_initialization" not in line:
                data = line.replace(',','').replace(';','').strip()
                hex_list.append(int(data, 16))
    return hex_list

###--- copies write data mem model (mem) and returns read buffer mem             
def copy_mem_buffer(mem1, mem2, base_addr1, base_addr2):
    mem2 = {}
    for i in range(len(mem1)):
        mem2[base_addr2+i] = mem1[base_addr1+i]
    return mem2

### ============================================================================= ###
