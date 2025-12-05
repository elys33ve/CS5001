// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// --------------------------------------------------------------------------------
// Tool Version: Vivado v.2025.1 (lin64) Build 6140274 Wed May 21 22:58:25 MDT 2025
// Date        : Thu Aug 21 11:37:31 2025
// Host        : thelio3 running 64-bit Ubuntu 22.04.5 LTS
// Command     : write_verilog -force -mode funcsim
//               /home/toconnel/projects/ip/ddr_test_wr_1_0/src/pr_wr_rom1/pr_wr_rom1_sim_netlist.v
// Design      : pr_wr_rom1
// Purpose     : This verilog netlist is a functional simulation representation of the design and should not be modified
//               or synthesized. This netlist cannot be used for SDF annotated simulation.
// Device      : xczu3eg-sfva625-1-i
// --------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

(* CHECK_LICENSE_TYPE = "pr_wr_rom1,blk_mem_gen_v8_4_11,{}" *) (* downgradeipidentifiedwarnings = "yes" *) (* x_core_info = "blk_mem_gen_v8_4_11,Vivado 2025.1" *) 
(* NotValidForBitStream *)
module pr_wr_rom1
   (clka,
    ena,
    addra,
    douta);
  (* x_interface_info = "xilinx.com:interface:bram:1.0 BRAM_PORTA CLK" *) (* x_interface_mode = "slave BRAM_PORTA" *) (* x_interface_parameter = "XIL_INTERFACENAME BRAM_PORTA, MEM_ADDRESS_MODE BYTE_ADDRESS, MEM_SIZE 8192, MEM_WIDTH 32, MEM_ECC NONE, MASTER_TYPE OTHER, READ_LATENCY 1" *) input clka;
  (* x_interface_info = "xilinx.com:interface:bram:1.0 BRAM_PORTA EN" *) input ena;
  (* x_interface_info = "xilinx.com:interface:bram:1.0 BRAM_PORTA ADDR" *) input [7:0]addra;
  (* x_interface_info = "xilinx.com:interface:bram:1.0 BRAM_PORTA DOUT" *) output [127:0]douta;

  wire [7:0]addra;
  wire clka;
  wire [127:0]douta;
  wire ena;
  wire NLW_U0_dbiterr_UNCONNECTED;
  wire NLW_U0_rsta_busy_UNCONNECTED;
  wire NLW_U0_rstb_busy_UNCONNECTED;
  wire NLW_U0_s_axi_arready_UNCONNECTED;
  wire NLW_U0_s_axi_awready_UNCONNECTED;
  wire NLW_U0_s_axi_bvalid_UNCONNECTED;
  wire NLW_U0_s_axi_dbiterr_UNCONNECTED;
  wire NLW_U0_s_axi_rlast_UNCONNECTED;
  wire NLW_U0_s_axi_rvalid_UNCONNECTED;
  wire NLW_U0_s_axi_sbiterr_UNCONNECTED;
  wire NLW_U0_s_axi_wready_UNCONNECTED;
  wire NLW_U0_sbiterr_UNCONNECTED;
  wire [127:0]NLW_U0_doutb_UNCONNECTED;
  wire [7:0]NLW_U0_rdaddrecc_UNCONNECTED;
  wire [3:0]NLW_U0_s_axi_bid_UNCONNECTED;
  wire [1:0]NLW_U0_s_axi_bresp_UNCONNECTED;
  wire [7:0]NLW_U0_s_axi_rdaddrecc_UNCONNECTED;
  wire [127:0]NLW_U0_s_axi_rdata_UNCONNECTED;
  wire [3:0]NLW_U0_s_axi_rid_UNCONNECTED;
  wire [1:0]NLW_U0_s_axi_rresp_UNCONNECTED;

  (* C_ADDRA_WIDTH = "8" *) 
  (* C_ADDRB_WIDTH = "8" *) 
  (* C_ALGORITHM = "1" *) 
  (* C_AXI_ID_WIDTH = "4" *) 
  (* C_AXI_SLAVE_TYPE = "0" *) 
  (* C_AXI_TYPE = "1" *) 
  (* C_BYTE_SIZE = "9" *) 
  (* C_COMMON_CLK = "0" *) 
  (* C_COUNT_18K_BRAM = "0" *) 
  (* C_COUNT_36K_BRAM = "2" *) 
  (* C_CTRL_ECC_ALGO = "NONE" *) 
  (* C_DEFAULT_DATA = "0" *) 
  (* C_DISABLE_WARN_BHV_COLL = "0" *) 
  (* C_DISABLE_WARN_BHV_RANGE = "0" *) 
  (* C_ELABORATION_DIR = "./" *) 
  (* C_ENABLE_32BIT_ADDRESS = "0" *) 
  (* C_EN_DEEPSLEEP_PIN = "0" *) 
  (* C_EN_ECC_PIPE = "0" *) 
  (* C_EN_RDADDRA_CHG = "0" *) 
  (* C_EN_RDADDRB_CHG = "0" *) 
  (* C_EN_SAFETY_CKT = "0" *) 
  (* C_EN_SHUTDOWN_PIN = "0" *) 
  (* C_EN_SLEEP_PIN = "0" *) 
  (* C_EST_POWER_SUMMARY = "Estimated Power for IP     :     11.66353 mW" *) 
  (* C_FAMILY = "zynquplus" *) 
  (* C_HAS_AXI_ID = "0" *) 
  (* C_HAS_ENA = "1" *) 
  (* C_HAS_ENB = "0" *) 
  (* C_HAS_INJECTERR = "0" *) 
  (* C_HAS_MEM_OUTPUT_REGS_A = "0" *) 
  (* C_HAS_MEM_OUTPUT_REGS_B = "0" *) 
  (* C_HAS_MUX_OUTPUT_REGS_A = "0" *) 
  (* C_HAS_MUX_OUTPUT_REGS_B = "0" *) 
  (* C_HAS_REGCEA = "0" *) 
  (* C_HAS_REGCEB = "0" *) 
  (* C_HAS_RSTA = "0" *) 
  (* C_HAS_RSTB = "0" *) 
  (* C_HAS_SOFTECC_INPUT_REGS_A = "0" *) 
  (* C_HAS_SOFTECC_OUTPUT_REGS_B = "0" *) 
  (* C_INITA_VAL = "0" *) 
  (* C_INITB_VAL = "0" *) 
  (* C_INIT_FILE = "pr_wr_rom1.mem" *) 
  (* C_INIT_FILE_NAME = "pr_wr_rom1.mif" *) 
  (* C_INTERFACE_TYPE = "0" *) 
  (* C_LOAD_INIT_FILE = "1" *) 
  (* C_MEM_TYPE = "3" *) 
  (* C_MUX_PIPELINE_STAGES = "0" *) 
  (* C_PRIM_TYPE = "1" *) 
  (* C_READ_DEPTH_A = "256" *) 
  (* C_READ_DEPTH_B = "256" *) 
  (* C_READ_LATENCY_A = "1" *) 
  (* C_READ_LATENCY_B = "1" *) 
  (* C_READ_WIDTH_A = "128" *) 
  (* C_READ_WIDTH_B = "128" *) 
  (* C_RSTRAM_A = "0" *) 
  (* C_RSTRAM_B = "0" *) 
  (* C_RST_PRIORITY_A = "CE" *) 
  (* C_RST_PRIORITY_B = "CE" *) 
  (* C_SIM_COLLISION_CHECK = "ALL" *) 
  (* C_USE_BRAM_BLOCK = "0" *) 
  (* C_USE_BYTE_WEA = "0" *) 
  (* C_USE_BYTE_WEB = "0" *) 
  (* C_USE_DEFAULT_DATA = "0" *) 
  (* C_USE_ECC = "0" *) 
  (* C_USE_SOFTECC = "0" *) 
  (* C_USE_URAM = "0" *) 
  (* C_WEA_WIDTH = "1" *) 
  (* C_WEB_WIDTH = "1" *) 
  (* C_WRITE_DEPTH_A = "256" *) 
  (* C_WRITE_DEPTH_B = "256" *) 
  (* C_WRITE_MODE_A = "WRITE_FIRST" *) 
  (* C_WRITE_MODE_B = "WRITE_FIRST" *) 
  (* C_WRITE_WIDTH_A = "128" *) 
  (* C_WRITE_WIDTH_B = "128" *) 
  (* C_XDEVICEFAMILY = "zynquplus" *) 
  (* downgradeipidentifiedwarnings = "yes" *) 
  (* is_du_within_envelope = "true" *) 
  pr_wr_rom1_blk_mem_gen_v8_4_11 U0
       (.addra(addra),
        .addrb({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .clka(clka),
        .clkb(1'b0),
        .dbiterr(NLW_U0_dbiterr_UNCONNECTED),
        .deepsleep(1'b0),
        .dina({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .dinb({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .douta(douta),
        .doutb(NLW_U0_doutb_UNCONNECTED[127:0]),
        .eccpipece(1'b0),
        .ena(ena),
        .enb(1'b0),
        .injectdbiterr(1'b0),
        .injectsbiterr(1'b0),
        .rdaddrecc(NLW_U0_rdaddrecc_UNCONNECTED[7:0]),
        .regcea(1'b1),
        .regceb(1'b1),
        .rsta(1'b0),
        .rsta_busy(NLW_U0_rsta_busy_UNCONNECTED),
        .rstb(1'b0),
        .rstb_busy(NLW_U0_rstb_busy_UNCONNECTED),
        .s_aclk(1'b0),
        .s_aresetn(1'b0),
        .s_axi_araddr({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_arburst({1'b0,1'b0}),
        .s_axi_arid({1'b0,1'b0,1'b0,1'b0}),
        .s_axi_arlen({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_arready(NLW_U0_s_axi_arready_UNCONNECTED),
        .s_axi_arsize({1'b0,1'b0,1'b0}),
        .s_axi_arvalid(1'b0),
        .s_axi_awaddr({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_awburst({1'b0,1'b0}),
        .s_axi_awid({1'b0,1'b0,1'b0,1'b0}),
        .s_axi_awlen({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_awready(NLW_U0_s_axi_awready_UNCONNECTED),
        .s_axi_awsize({1'b0,1'b0,1'b0}),
        .s_axi_awvalid(1'b0),
        .s_axi_bid(NLW_U0_s_axi_bid_UNCONNECTED[3:0]),
        .s_axi_bready(1'b0),
        .s_axi_bresp(NLW_U0_s_axi_bresp_UNCONNECTED[1:0]),
        .s_axi_bvalid(NLW_U0_s_axi_bvalid_UNCONNECTED),
        .s_axi_dbiterr(NLW_U0_s_axi_dbiterr_UNCONNECTED),
        .s_axi_injectdbiterr(1'b0),
        .s_axi_injectsbiterr(1'b0),
        .s_axi_rdaddrecc(NLW_U0_s_axi_rdaddrecc_UNCONNECTED[7:0]),
        .s_axi_rdata(NLW_U0_s_axi_rdata_UNCONNECTED[127:0]),
        .s_axi_rid(NLW_U0_s_axi_rid_UNCONNECTED[3:0]),
        .s_axi_rlast(NLW_U0_s_axi_rlast_UNCONNECTED),
        .s_axi_rready(1'b0),
        .s_axi_rresp(NLW_U0_s_axi_rresp_UNCONNECTED[1:0]),
        .s_axi_rvalid(NLW_U0_s_axi_rvalid_UNCONNECTED),
        .s_axi_sbiterr(NLW_U0_s_axi_sbiterr_UNCONNECTED),
        .s_axi_wdata({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_wlast(1'b0),
        .s_axi_wready(NLW_U0_s_axi_wready_UNCONNECTED),
        .s_axi_wstrb(1'b0),
        .s_axi_wvalid(1'b0),
        .sbiterr(NLW_U0_sbiterr_UNCONNECTED),
        .shutdown(1'b0),
        .sleep(1'b0),
        .wea(1'b0),
        .web(1'b0));
endmodule
`pragma protect begin_protected
`pragma protect version = 1
`pragma protect encrypt_agent = "XILINX"
`pragma protect encrypt_agent_info = "Xilinx Encryption Tool 2025.1"
`pragma protect key_keyowner="Synopsys", key_keyname="SNPS-VCS-RSA-2", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=128)
`pragma protect key_block
gydSV72FvW4hnoyUt6yZFJHfJqjRQWPUfYIuDKP0fpjrPOkLRbJGBr4Z9msYTvoIHRlYtXJ2YMY0
d1TIQb+FK4gKsTRru9wr397OxuFBsTRf4e+ZjpYZEdsnqYWcgMSzhN4yhPvO06GyZO15y/LKBxa8
3OKwxVlOLYXhv+sxdXg=

`pragma protect key_keyowner="Aldec", key_keyname="ALDEC15_001", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
WHB6Zbfa5Qi47krP9T4L8UnPOlr881dWx7UcYaZfNGIQQM0gadcoXbhucIpRaUuyOKxv6yhKveRN
h0l+N9+KX6rbZ6+TRhP9JAMuPhlpI7T42QtRv5zx9+m3ct5S0NMszbFaK8zeTAYra5BGP7BHmtkr
MpKfLK5sFyaTE/A7ACtAace9MwFTHDZdl9uUs4aY6KJlm6GaypKduiqkNugukJp5vlFPX/ZapJqG
KMtMhI6grhcuYb1FJrwRZ4jW7hs9HxddSdGLzsZ0HsBcO/qaCPTst+ZA0YIQfd5ULlFmPqq39FfO
p1P+2hEH2n+LycbMj5cn4Dxfqv2R8eucM78R3w==

`pragma protect key_keyowner="Mentor Graphics Corporation", key_keyname="MGC-VELOCE-RSA", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=128)
`pragma protect key_block
SmAzQA1VEuJXtJi5vXa2Jg7YvRqAJs6PX9HTZ1YqrJw4VfonBW3726gJ81BjlizpMkcf/Uk5sFIK
aPedVhEs4xCIZylz7gXYDshtytOA/pXUID2qV9nXr8qfI+FydSADUF3ScYDZmlkclFqlZrGq6DQ7
da3lJAzt2h/iR+cczrA=

`pragma protect key_keyowner="Mentor Graphics Corporation", key_keyname="MGC-VERIF-SIM-RSA-2", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
iAph5JWb/chMQpLPX1UoLjQDxN5l2I8McM/k2xN5wRht7HXoE6F5yV8luDjn3zkI6vnfUYo7BaI1
mogRRx+R3XcwxvhHr+lngh4+/YLVex1TFncl+kiUMAsu3M/FjFSiqGMVMdKTNLDqr35DuZJVyuiF
lTwXob/KkbQDJiJjBEoxbt+968rKRKRyJGcqIjm4mqRBdqMcgo3HOJFG74SFsWAQrxvXfBhdLSG3
OfoLfls9XDojBjp7G83k0h82g1eeWgBfydm/OcX9o48Pst93NvI4ua8WShZL8MCvRWYqWZrrjrWi
cfUjXAF5SDACjq1/OU6arz/Idz6/a7AP/jmexw==

`pragma protect key_keyowner="Real Intent", key_keyname="RI-RSA-KEY-1", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
BY49GZBxBT/gjZDPyaSWlti/sctckoR7jK6NuWdhnF9tiyNfVU7BqjjwxSnyMi0Uucv1BKHXC18h
8hQbFWnNtrq71ilURotXux7sssHlVJ2i1CsJWU18DOcBWxm2ai89uwvxDJh3TJkBJixB5KPvsDhL
lWOjTvZWPoR+Ixy+Tzo+U5Vx7z7SOakRwTrn3u7+c3vmCEBphE+HKeJExhBAoOEd0SXK5iwXaByW
D7Wb7zq6NNUmnCyaJ2BG9kGxLVsf+md7SlocuaFsYyaRZhwPyTucxIlz1tLYwcytKzx0ovoax3no
nYgzlzP/F0/PDWk9BqXgr/tuclc4EZYX0cf4ng==

`pragma protect key_keyowner="Xilinx", key_keyname="xilinxt_2025.1-2029.x", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
qGnCvL35qO7cbUEKCL50yDv1UvezcqBz601zctKop1954QlcjemzZWZHg1zJ00nJaToNdH2S8AKX
n8hNJvbQ+x5HEGL5DoSU9m5qjXd8xxocnZ0yzuZX/dGCT8kDn3gWJR2Gz13pT+w2LQUno1fX+MsC
ehgwvjBBT6GeYjdxHi+aybQUP9AblSxX/z3vh857SGCPohEWvghOgORCHAe45YD+ZWnL62FLxMM2
c+Ozq/Au/Q4q1Yzlzcfv8Mnsvg7OqOeEamQHbuYOfdkJUuYqOwsskEWW348u7FXtsf8m7P3pZyyz
IWyTDAW4igGguMPLHfbtK/twZx8ScJQmOKzglg==

`pragma protect key_keyowner="Metrics Technologies Inc.", key_keyname="DSim", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
Hz+6K8+wh5/fukU4ZWNDXGsq6hreSVCSPP67nA6kUz9Vpjy4TtTnOrrl1BWY0ivEC7Ldyw8VI60A
VO/WPlt409LdAZdMZGsEZ1JuTZ0m9LPcgu9CPCyoMECctmd8LHE+otY6etTmYABB9syY61rk2hrv
RgbcyT/HCK9TzWxSm+XMqvx2nvagCLkMDPh/JZv51fj2zcKaBPnxsz8rnDipaeo0fEyVRC3Y1F/V
U3RmXojBjIumPHSJkQ537dENJEIA0Ra65u8EM/+ItUn1bcryLcIbKy1xGadrHmHdHRUoRcAodO2C
B48bNVeL0VnGg8P9ACIB04lMNzn5p6A1tPOb4Q==

`pragma protect key_keyowner="Atrenta", key_keyname="ATR-SG-RSA-1", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=384)
`pragma protect key_block
YDpb+UeT0rJ543Q8wCo2xSS3gpVAT+JoStgBlV5IMjJoUOWkiOPn691FGChmDi3BTq5NxC73KHHR
1galACCjeTGq6cv+0Zc2Ocm1oobdrnSPHp7TMDr5Zle8FX6WywJCiGdoWBODggZSlbOASIK/PVfY
cZM2z60M6RSvzsi3TnYHiKYHpju8THVoSgRd6r31GcbiSy9TjjARERXan0OVc79jGuAg90mmDEEq
91eqmn6NZ9yLI2fgBjFUZbtFCpmJ8WGxOL1h39niWnRK3ZXnk8jcpnZUlxLbYTPO0Z3vVr1zrvcn
RVQloU0OLqg7M95zSs7NtX5Vzvb6jGbMehWV+WMMyxWmxL2XOwsAwPSeX2dI2r77pioY7X6VzH7f
/JxMAnq9udra3WGPsUkD1G0CvPkCC3zdxjpVaflY37ztX9UONhKtzMQa8lJc1IL8GhXRY3R9Lg2c
HIeXSGkpNNuFDqKT6Khe/6Casq+SjFJq+IH9IUtz6RUZTkbFb0Xhgm2P

`pragma protect key_keyowner="Cadence Design Systems.", key_keyname="CDS_RSA_KEY_VER_1", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
Q+63zFEYw/LeMgxa7g8g79GGvSyIKDKD8RvvC4DHDQuGObf6n9OGZX4e17v/E/+EDEwUhsWQHFDI
Lp/aH+6fNRmhu9BEWVjxq2WRrQSl4eQjfIaSOXu2dlYh3JjRJwiUp4LteVh8RFAf5t5sRQO4dRIK
x+h28yliSgibaWEAv5FaJQ1EFbNwmgedAaSYjgf2A3afBUcBh5Uy9VHbW/zRzdhhJdsVNBjZYcFy
CVLOcf1toCRp8J4U5FlnFMOzFegUbdXFQhq2VmIhPRxWjrfTk6iR4BcMEN9UMij/5IHRAeBdksyD
CqEKsyFxosbI5KVMRZ1Ln75Zipn0JdsGekHkxg==

`pragma protect key_keyowner="Synplicity", key_keyname="SYNP15_1", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
DPUa5DLPYRWvbPnX0U412yoWvvvHyuq43DrYmDJGTK0cR5U4U6th8icYgizC1/hUAEzt19kM/hVa
zZh7bXSWACYLpcfhPY8dRTVGDZVjpbkraw0ceBryLP7jc6Jt5JdNw88tZtZpprCB7nQ25lUL82Hf
WTwL1ZqgGIvtfHhxO0JF5L5ES5giedwQ6u5ffXG3UB6ELcpQD1NvpW5lAz4mfXyvVDCAPZN581TF
tlAy79iKbPKlJ2zFn1BS2cuRIHHe2JRxwPo+0n5VD5CXVgg+lCYxTnCxI8CdyFaTumbs4IfAKwVI
wSN/btbwDUhW9hAHWHIRo+BpdJ4qeGcTDPKtsA==

`pragma protect key_keyowner="Mentor Graphics Corporation", key_keyname="MGC-PREC-RSA", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
mf5hcf6JE6yLm0jNCQnHMVmogjLlPz6re0FwG67yvOJ3FuEorru0emIeAKEwgOoxjUYNWvcM7QAH
/UEeB2EIdjLl6glPAUda0HjtaCU2rdncVdM8k6DSMBggc4yo18Qx5F+1TD/RoBgoo0jNkMdDy6wJ
JHjqlN+R01z3yYIMQ9f2z6ZaYncbBYEp4+YAb7g1D7CSMxP5cFRpQznRpYp0JwqJfT9CHzlKgdab
8B288NxeLM66iYodiTS+GSRGLGtDWXpz9yeiuiPe6kJxae2GJyHIMSfluO/0Slc3m24DQNdbojf8
jdc0G2UnrDe5mCUTfYiDmpOWTUJOdYo0FK0N2g==

`pragma protect data_method = "AES128-CBC"
`pragma protect encoding = (enctype = "BASE64", line_length = 76, bytes = 55392)
`pragma protect data_block
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
`pragma protect end_protected
`ifndef GLBL
`define GLBL
`timescale  1 ps / 1 ps

module glbl ();

    parameter ROC_WIDTH = 100000;
    parameter TOC_WIDTH = 0;
    parameter GRES_WIDTH = 10000;
    parameter GRES_START = 10000;

//--------   STARTUP Globals --------------
    wire GSR;
    wire GTS;
    wire GWE;
    wire PRLD;
    wire GRESTORE;
    tri1 p_up_tmp;
    tri (weak1, strong0) PLL_LOCKG = p_up_tmp;

    wire PROGB_GLBL;
    wire CCLKO_GLBL;
    wire FCSBO_GLBL;
    wire [3:0] DO_GLBL;
    wire [3:0] DI_GLBL;
   
    reg GSR_int;
    reg GTS_int;
    reg PRLD_int;
    reg GRESTORE_int;

//--------   JTAG Globals --------------
    wire JTAG_TDO_GLBL;
    wire JTAG_TCK_GLBL;
    wire JTAG_TDI_GLBL;
    wire JTAG_TMS_GLBL;
    wire JTAG_TRST_GLBL;

    reg JTAG_CAPTURE_GLBL;
    reg JTAG_RESET_GLBL;
    reg JTAG_SHIFT_GLBL;
    reg JTAG_UPDATE_GLBL;
    reg JTAG_RUNTEST_GLBL;

    reg JTAG_SEL1_GLBL = 0;
    reg JTAG_SEL2_GLBL = 0 ;
    reg JTAG_SEL3_GLBL = 0;
    reg JTAG_SEL4_GLBL = 0;

    reg JTAG_USER_TDO1_GLBL = 1'bz;
    reg JTAG_USER_TDO2_GLBL = 1'bz;
    reg JTAG_USER_TDO3_GLBL = 1'bz;
    reg JTAG_USER_TDO4_GLBL = 1'bz;

    assign (strong1, weak0) GSR = GSR_int;
    assign (strong1, weak0) GTS = GTS_int;
    assign (weak1, weak0) PRLD = PRLD_int;
    assign (strong1, weak0) GRESTORE = GRESTORE_int;

    initial begin
	GSR_int = 1'b1;
	PRLD_int = 1'b1;
	#(ROC_WIDTH)
	GSR_int = 1'b0;
	PRLD_int = 1'b0;
    end

    initial begin
	GTS_int = 1'b1;
	#(TOC_WIDTH)
	GTS_int = 1'b0;
    end

    initial begin 
	GRESTORE_int = 1'b0;
	#(GRES_START);
	GRESTORE_int = 1'b1;
	#(GRES_WIDTH);
	GRESTORE_int = 1'b0;
    end

endmodule
`endif
