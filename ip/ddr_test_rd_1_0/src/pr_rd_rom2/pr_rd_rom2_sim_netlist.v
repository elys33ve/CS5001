// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// --------------------------------------------------------------------------------
// Tool Version: Vivado v.2025.1 (lin64) Build 6140274 Wed May 21 22:58:25 MDT 2025
// Date        : Tue Aug 26 11:53:49 2025
// Host        : mega running 64-bit Ubuntu 24.04.3 LTS
// Command     : write_verilog -force -mode funcsim
//               /home/toconnel/projects/ip/ddr_test_rd_1_0/src/pr_rd_rom2/pr_rd_rom2_sim_netlist.v
// Design      : pr_rd_rom2
// Purpose     : This verilog netlist is a functional simulation representation of the design and should not be modified
//               or synthesized. This netlist cannot be used for SDF annotated simulation.
// Device      : xczu3eg-sfva625-1-i
// --------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

(* CHECK_LICENSE_TYPE = "pr_rd_rom2,blk_mem_gen_v8_4_11,{}" *) (* downgradeipidentifiedwarnings = "yes" *) (* x_core_info = "blk_mem_gen_v8_4_11,Vivado 2025.1" *) 
(* NotValidForBitStream *)
module pr_rd_rom2
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
  (* C_INIT_FILE = "pr_rd_rom2.mem" *) 
  (* C_INIT_FILE_NAME = "pr_rd_rom2.mif" *) 
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
  pr_rd_rom2_blk_mem_gen_v8_4_11 U0
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
2Ft4q51FwzvrbsrdnNrVm0wPRmzXI+rc4V1KAQ76RvK7PAeeNznPWBeMak0Y7xfYNfqtus0ps8VJ
F9btXcam/DloTZ6krgujhNmC3+jDBRxoXOwEVBoPJBZVvviFWiR28jUkiLfdypQ0tpQfa7QYgJ49
OXdgPjdQM4CcNwPFvn5FSj6JWGHw988rXyH3rHu5uTdY7QvgHFxnKTbnTVpIhTNPB+IGH0ehEKy4
hP2lgwl/NxkNHCtIeeNcXP5EAuyUqNILTou2ymtR33gbKUiNDG4eY8zAOjXgJ1N21swPKnM8pQ1q
DHFcCFJBOulJUiLfte2hQ0ExJixkZhJiyZAWZYA+WwtzyjKaTZjg2XMNf3ER8IdkBT2foYb8LJSX
xwcPn8STZZ6tbm7uuWsll6X4QVEq8stUyohiEh79UWpzBb5JcJsDKxYoM3tF6wmJ9dycec3vaHiG
MwFUSZco8ZZXNM7NeS4fWpUo0SVFUbCZKK2tdI91GmIqzAddice9cUlJF+CbzXJpLO6PkqUXrkps
t/jKJFeWusEXfd35XzFoagcrxua3prwtanJcopPUbGiQOuUuIwv59eBRkB4BnOI9Ch1xWWpjkudk
44UBNeyeLRPcRPgJePoqvvyicO3Ak3XhFxjimyUgggUAQ6t1C4zo7gXRH1KvGLx2xbxLVCbKwfvx
QUs87op4dGz8/1/zPsph/nb7D354Cy8V8I62b1/ZlHuL/ldMkh+Xa+I2tOpsu9fv8f1QlmMniMzl
oBkyVPEaDX+n/BeP/24P3xhusYzjXxeT6stMyc7v2ZSnIJC38DkZfjwI3q1tcf4/HmY20WznN2Io
YYBzzP8ySxh6JOsAouSZgCYUDdh4Oq0gepLiENltbqHPxbLgCLNck1R34+tBRL/llvLjoB/aPgJs
tjP6a4ZnXbVsgU1Vahp90AqKej+MwG9j/EV/lxfVio5zT4tHQy6g1ZfKwo+zNaBeJAWj4gEbG3yk
2EoXtJSIetL6nGZQ4u79DkjD5uep97K0OoBcdEmPAKDsMjIxpNWdXGDYnB5zqj1w1Jt682Tho+67
zJeHCQnwKpJGHmpEWBin81n2NibJVNhvniyPCMhua/JNFs9Sti9ypMP+Bt9qAzbuxsMjg1BGnrBz
qTBTCNQhr5yfqD47uOHcTYOLYd50hLAfuoqevTdYUkwuW1Bxuyn4/9PE9cgNXT1X7TNjzPX0yfrE
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
