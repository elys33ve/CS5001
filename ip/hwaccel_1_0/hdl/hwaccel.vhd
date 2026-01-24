library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity hwaccel is
  generic (
    C_axi_lite_DATA_WIDTH : integer := 32;
    C_axi_lite_ADDR_WIDTH : integer := 7;

    C_axim_ID_WIDTH     : integer := 1;
    C_axim_ADDR_WIDTH   : integer := 32;
    C_axim_DATA_WIDTH   : integer := 128;
    C_axim_AWUSER_WIDTH : integer := 16;
    C_axim_ARUSER_WIDTH : integer := 16;
    C_axim_WUSER_WIDTH  : integer := 16;
    C_axim_RUSER_WIDTH  : integer := 16;
    C_axim_BUSER_WIDTH  : integer := 16);
  port (
    axiclk  : in std_logic;
    aresetn : in std_logic;

    enet1_nrst : out std_logic;

    -------------------------------------
    -- test and verification
    -------------------------------------
    axim_in_araddr_scope  : out std_logic_vector(31 downto 0);
    axim_in_arready_scope : out std_logic;
    axim_in_arlen_scope   : out std_logic_vector(7 downto 0);
    axim_in_rready_scope  : out std_logic;
    axim_in_rvalid_scope  : out std_logic;
    axim_in_rlast_scope   : out std_logic;
    axim_in_bvalid_scope  : out std_logic;
    axim_in_bready_scope  : out std_logic;
    axim_in_rdata_scope   : out std_logic_vector(127 downto 0);
    axim_in_wdata_scope   : out std_logic_vector(127 downto 0);
    axim_in_wready_scope  : out std_logic;
    axim_in_wvalid_scope  : out std_logic;
    axim_in_wlast_scope   : out std_logic;
    axim_in_state_scope   : out unsigned(3 downto 0);

    axim_out_araddr_scope  : out std_logic_vector(31 downto 0);
    axim_out_arready_scope : out std_logic;
    axim_out_arlen_scope   : out std_logic_vector(7 downto 0);
    axim_out_rready_scope  : out std_logic;
    axim_out_rvalid_scope  : out std_logic;
    axim_out_rlast_scope   : out std_logic;
    axim_out_bvalid_scope  : out std_logic;
    axim_out_bready_scope  : out std_logic;
    axim_out_rdata_scope   : out std_logic_vector(127 downto 0);
    axim_out_wdata_scope   : out std_logic_vector(127 downto 0);
    axim_out_wready_scope  : out std_logic;
    axim_out_wvalid_scope  : out std_logic;
    axim_out_wlast_scope   : out std_logic;
    axim_out_state_scope   : out unsigned(3 downto 0);

    axim_in_fifo_rd_scope    : out std_logic; 
    axim_out_wr_ready_scope  : out std_logic; 
    axim_out_wr_scope        : out std_logic; 
    axim_in_fifo_empty_scope : out std_logic; 
    axim_in_fifo_dout_scope  : out unsigned(127 downto 0); 
    axim_out_din_scope       : out unsigned(127 downto 0);

    axim_out_dcount_scope : out unsigned(10 downto 0);

    -------------------------------------
    -- axi lite bus
    -------------------------------------
    axi_lite_awaddr  : in  std_logic_vector(C_axi_lite_ADDR_WIDTH-1 downto 0);
    axi_lite_awprot  : in  std_logic_vector(2 downto 0);
    axi_lite_awvalid : in  std_logic;
    axi_lite_awready : out std_logic;
    axi_lite_wdata   : in  std_logic_vector(C_axi_lite_DATA_WIDTH-1 downto 0);
    axi_lite_wstrb   : in  std_logic_vector((C_axi_lite_DATA_WIDTH/8)-1 downto 0);
    axi_lite_wvalid  : in  std_logic;
    axi_lite_wready  : out std_logic;
    axi_lite_bresp   : out std_logic_vector(1 downto 0);
    axi_lite_bvalid  : out std_logic;
    axi_lite_bready  : in  std_logic;
    axi_lite_araddr  : in  std_logic_vector(C_axi_lite_ADDR_WIDTH-1 downto 0);
    axi_lite_arprot  : in  std_logic_vector(2 downto 0);
    axi_lite_arvalid : in  std_logic;
    axi_lite_arready : out std_logic;
    axi_lite_rdata   : out std_logic_vector(C_axi_lite_DATA_WIDTH-1 downto 0);
    axi_lite_rresp   : out std_logic_vector(1 downto 0);
    axi_lite_rvalid  : out std_logic;
    axi_lite_rready  : in  std_logic;

    -------------------------------------
    -- axim in bus
    -------------------------------------
    axim_in_awid    : out std_logic_vector(C_axim_ID_WIDTH-1 downto 0);
    axim_in_awaddr  : out std_logic_vector(C_axim_ADDR_WIDTH-1 downto 0);
    axim_in_awlen   : out std_logic_vector(7 downto 0);
    axim_in_awsize  : out std_logic_vector(2 downto 0);
    axim_in_awburst : out std_logic_vector(1 downto 0);
    axim_in_awlock  : out std_logic;
    axim_in_awcache : out std_logic_vector(3 downto 0);
    axim_in_awprot  : out std_logic_vector(2 downto 0);
    axim_in_awqos   : out std_logic_vector(3 downto 0);
    axim_in_awuser  : out std_logic_vector(C_axim_AWUSER_WIDTH-1 downto 0);
    axim_in_awvalid : out std_logic;
    axim_in_awready : in  std_logic;
    axim_in_wdata   : out std_logic_vector(C_axim_DATA_WIDTH-1 downto 0);
    axim_in_wstrb   : out std_logic_vector(C_axim_DATA_WIDTH/8-1 downto 0);
    axim_in_wlast   : out std_logic;
    axim_in_wuser   : out std_logic_vector(C_axim_WUSER_WIDTH-1 downto 0);
    axim_in_wvalid  : out std_logic;
    axim_in_wready  : in  std_logic;
    axim_in_bid     : in  std_logic_vector(C_axim_ID_WIDTH-1 downto 0);
    axim_in_bresp   : in  std_logic_vector(1 downto 0);
    axim_in_buser   : in  std_logic_vector(C_axim_BUSER_WIDTH-1 downto 0);
    axim_in_bvalid  : in  std_logic;
    axim_in_bready  : out std_logic;
    axim_in_arid    : out std_logic_vector(C_axim_ID_WIDTH-1 downto 0);
    axim_in_araddr  : out std_logic_vector(C_axim_ADDR_WIDTH-1 downto 0);
    axim_in_arlen   : out std_logic_vector(7 downto 0);
    axim_in_arsize  : out std_logic_vector(2 downto 0);
    axim_in_arburst : out std_logic_vector(1 downto 0);
    axim_in_arlock  : out std_logic;
    axim_in_arcache : out std_logic_vector(3 downto 0);
    axim_in_arprot  : out std_logic_vector(2 downto 0);
    axim_in_arqos   : out std_logic_vector(3 downto 0);
    axim_in_aruser  : out std_logic_vector(C_axim_ARUSER_WIDTH-1 downto 0);
    axim_in_arvalid : out std_logic;
    axim_in_arready : in  std_logic;
    axim_in_rid     : in  std_logic_vector(C_axim_ID_WIDTH-1 downto 0);
    axim_in_rdata   : in  std_logic_vector(C_axim_DATA_WIDTH-1 downto 0);
    axim_in_rresp   : in  std_logic_vector(1 downto 0);
    axim_in_rlast   : in  std_logic;
    axim_in_ruser   : in  std_logic_vector(C_axim_RUSER_WIDTH-1 downto 0);
    axim_in_rvalid  : in  std_logic;
    axim_in_rready  : out std_logic;

    -------------------------------------
    -- axim out bus
    -------------------------------------
    axim_out_awid    : out std_logic_vector(C_axim_ID_WIDTH-1 downto 0);
    axim_out_awaddr  : out std_logic_vector(C_axim_ADDR_WIDTH-1 downto 0);
    axim_out_awlen   : out std_logic_vector(7 downto 0);
    axim_out_awsize  : out std_logic_vector(2 downto 0);
    axim_out_awburst : out std_logic_vector(1 downto 0);
    axim_out_awlock  : out std_logic;
    axim_out_awcache : out std_logic_vector(3 downto 0);
    axim_out_awprot  : out std_logic_vector(2 downto 0);
    axim_out_awqos   : out std_logic_vector(3 downto 0);
    axim_out_awuser  : out std_logic_vector(C_axim_AWUSER_WIDTH-1 downto 0);
    axim_out_awvalid : out std_logic;
    axim_out_awready : in  std_logic;
    axim_out_wdata   : out std_logic_vector(C_axim_DATA_WIDTH-1 downto 0);
    axim_out_wstrb   : out std_logic_vector(C_axim_DATA_WIDTH/8-1 downto 0);
    axim_out_wlast   : out std_logic;
    axim_out_wuser   : out std_logic_vector(C_axim_WUSER_WIDTH-1 downto 0);
    axim_out_wvalid  : out std_logic;
    axim_out_wready  : in  std_logic;
    axim_out_bid     : in  std_logic_vector(C_axim_ID_WIDTH-1 downto 0);
    axim_out_bresp   : in  std_logic_vector(1 downto 0);
    axim_out_buser   : in  std_logic_vector(C_axim_BUSER_WIDTH-1 downto 0);
    axim_out_bvalid  : in  std_logic;
    axim_out_bready  : out std_logic;
    axim_out_arid    : out std_logic_vector(C_axim_ID_WIDTH-1 downto 0);
    axim_out_araddr  : out std_logic_vector(C_axim_ADDR_WIDTH-1 downto 0);
    axim_out_arlen   : out std_logic_vector(7 downto 0);
    axim_out_arsize  : out std_logic_vector(2 downto 0);
    axim_out_arburst : out std_logic_vector(1 downto 0);
    axim_out_arlock  : out std_logic;
    axim_out_arcache : out std_logic_vector(3 downto 0);
    axim_out_arprot  : out std_logic_vector(2 downto 0);
    axim_out_arqos   : out std_logic_vector(3 downto 0);
    axim_out_aruser  : out std_logic_vector(C_axim_ARUSER_WIDTH-1 downto 0);
    axim_out_arvalid : out std_logic;
    axim_out_arready : in  std_logic;
    axim_out_rid     : in  std_logic_vector(C_axim_ID_WIDTH-1 downto 0);
    axim_out_rdata   : in  std_logic_vector(C_axim_DATA_WIDTH-1 downto 0);
    axim_out_rresp   : in  std_logic_vector(1 downto 0);
    axim_out_rlast   : in  std_logic;
    axim_out_ruser   : in  std_logic_vector(C_axim_RUSER_WIDTH-1 downto 0);
    axim_out_rvalid  : in  std_logic;
    axim_out_rready  : out std_logic);
end hwaccel;

architecture arch of hwaccel is

  component hwaccel_axi_lite is
    generic (
      C_S_AXI_DATA_WIDTH : integer := 32;
      C_S_AXI_ADDR_WIDTH : integer := 7);
    port (
      rst        : out std_logic;
      en         : out std_logic;
      status_clr : out std_logic;
      shutdown   : out std_logic;
      enet1_nrst : out std_logic;

      -- ddr interface
      base_address_in    : out unsigned(31 downto 0);
      base_address_out   : out unsigned(31 downto 0);
      write_resp_error   : in  unsigned(3 downto 0);
      read_resp_error    : in  unsigned(3 downto 0);
      axim_nrst          : out std_logic;
      delay_cntto        : out unsigned(15 downto 0);
      axim_page_num      : in  unsigned(7 downto 0);
      frame_size_lines   : out unsigned(7 downto 0);
      payload_size_bytes : out unsigned(11 downto 0);

      S_AXI_ACLK    : in  std_logic;
      S_AXI_ARESETN : in  std_logic;
      S_AXI_AWADDR  : in  std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
      S_AXI_AWPROT  : in  std_logic_vector(2 downto 0);
      S_AXI_AWVALID : in  std_logic;
      S_AXI_AWREADY : out std_logic;
      S_AXI_WDATA   : in  std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
      S_AXI_WSTRB   : in  std_logic_vector((C_S_AXI_DATA_WIDTH/8)-1 downto 0);
      S_AXI_WVALID  : in  std_logic;
      S_AXI_WREADY  : out std_logic;
      S_AXI_BRESP   : out std_logic_vector(1 downto 0);
      S_AXI_BVALID  : out std_logic;
      S_AXI_BREADY  : in  std_logic;
      S_AXI_ARADDR  : in  std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
      S_AXI_ARPROT  : in  std_logic_vector(2 downto 0);
      S_AXI_ARVALID : in  std_logic;
      S_AXI_ARREADY : out std_logic;
      S_AXI_RDATA   : out std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
      S_AXI_RRESP   : out std_logic_vector(1 downto 0);
      S_AXI_RVALID  : out std_logic;
      S_AXI_RREADY  : in  std_logic);
  end component hwaccel_axi_lite;

  -- from cpu
  component hwaccel_axim_in is
    generic (
      C_M_AXI_ID_WIDTH     : integer := 1;
      C_M_AXI_ADDR_WIDTH   : integer := 32;
      C_M_AXI_DATA_WIDTH   : integer := 128;
      C_M_AXI_AWUSER_WIDTH : integer := 16;
      C_M_AXI_ARUSER_WIDTH : integer := 16;
      C_M_AXI_WUSER_WIDTH  : integer := 16;
      C_M_AXI_RUSER_WIDTH  : integer := 16;
      C_M_AXI_BUSER_WIDTH  : integer := 16);
    port (
      axiclk           : in  std_logic;
      aresetn          : in  std_logic;
      nrst             : in  std_logic;
      base_address     : in  unsigned(31 downto 0);
      write_resp_error : out unsigned(1 downto 0);
      read_resp_error  : out unsigned(1 downto 0);
      shutdown         : in  std_logic;

      frame_size_lines : in unsigned(7 downto 0);
      delay_cntto      : in unsigned(15 downto 0);

      frame_read_done  : out std_logic;
      frame_read_start : out std_logic;

      -- interface to acceleration core
      fifo_empty : out std_logic;
      fifo_dout  : out unsigned(127 downto 0);
      fifo_rd    : in  std_logic;

      -- test and verification
      state_out : out unsigned(3 downto 0);

      M_AXI_AWID    : out std_logic_vector(C_M_AXI_ID_WIDTH-1 downto 0);
      M_AXI_AWADDR  : out std_logic_vector(C_M_AXI_ADDR_WIDTH-1 downto 0);
      M_AXI_AWLEN   : out std_logic_vector(7 downto 0);
      M_AXI_AWSIZE  : out std_logic_vector(2 downto 0);
      M_AXI_AWBURST : out std_logic_vector(1 downto 0);
      M_AXI_AWLOCK  : out std_logic;
      M_AXI_AWCACHE : out std_logic_vector(3 downto 0);
      M_AXI_AWPROT  : out std_logic_vector(2 downto 0);
      M_AXI_AWQOS   : out std_logic_vector(3 downto 0);
      M_AXI_AWUSER  : out std_logic_vector(C_M_AXI_AWUSER_WIDTH-1 downto 0);
      M_AXI_AWVALID : out std_logic;
      M_AXI_AWREADY : in  std_logic;
      M_AXI_WDATA   : out std_logic_vector(C_M_AXI_DATA_WIDTH-1 downto 0);
      M_AXI_WSTRB   : out std_logic_vector(C_M_AXI_DATA_WIDTH/8-1 downto 0);
      M_AXI_WLAST   : out std_logic;
      M_AXI_WUSER   : out std_logic_vector(C_M_AXI_WUSER_WIDTH-1 downto 0);
      M_AXI_WVALID  : out std_logic;
      M_AXI_WREADY  : in  std_logic;
      M_AXI_BID     : in  std_logic_vector(C_M_AXI_ID_WIDTH-1 downto 0);
      M_AXI_BRESP   : in  std_logic_vector(1 downto 0);
      M_AXI_BUSER   : in  std_logic_vector(C_M_AXI_BUSER_WIDTH-1 downto 0);
      M_AXI_BVALID  : in  std_logic;
      M_AXI_BREADY  : out std_logic;
      M_AXI_ARID    : out std_logic_vector(C_M_AXI_ID_WIDTH-1 downto 0);
      M_AXI_ARADDR  : out std_logic_vector(C_M_AXI_ADDR_WIDTH-1 downto 0);
      M_AXI_ARLEN   : out std_logic_vector(7 downto 0);
      M_AXI_ARSIZE  : out std_logic_vector(2 downto 0);
      M_AXI_ARBURST : out std_logic_vector(1 downto 0);
      M_AXI_ARLOCK  : out std_logic;
      M_AXI_ARCACHE : out std_logic_vector(3 downto 0);
      M_AXI_ARPROT  : out std_logic_vector(2 downto 0);
      M_AXI_ARQOS   : out std_logic_vector(3 downto 0);
      M_AXI_ARUSER  : out std_logic_vector(C_M_AXI_ARUSER_WIDTH-1 downto 0);
      M_AXI_ARVALID : out std_logic;
      M_AXI_ARREADY : in  std_logic;
      M_AXI_RID     : in  std_logic_vector(C_M_AXI_ID_WIDTH-1 downto 0);
      M_AXI_RDATA   : in  std_logic_vector(C_M_AXI_DATA_WIDTH-1 downto 0);
      M_AXI_RRESP   : in  std_logic_vector(1 downto 0);
      M_AXI_RLAST   : in  std_logic;
      M_AXI_RUSER   : in  std_logic_vector(C_M_AXI_RUSER_WIDTH-1 downto 0);
      M_AXI_RVALID  : in  std_logic;
      M_AXI_RREADY  : out std_logic);
  end component hwaccel_axim_in;

  -- to cpu
  component hwaccel_axim_out is
    generic (
      C_M_AXI_ID_WIDTH     : integer := 1;
      C_M_AXI_ADDR_WIDTH   : integer := 32;
      C_M_AXI_DATA_WIDTH   : integer := 128;
      C_M_AXI_AWUSER_WIDTH : integer := 16;
      C_M_AXI_ARUSER_WIDTH : integer := 16;
      C_M_AXI_WUSER_WIDTH  : integer := 16;
      C_M_AXI_RUSER_WIDTH  : integer := 16;
      C_M_AXI_BUSER_WIDTH  : integer := 16);
    port (
      axiclk           : in  std_logic;
      rst              : in  std_logic;
      aresetn          : in  std_logic;
      nrst             : in  std_logic;
      base_address     : in  unsigned(31 downto 0);
      write_resp_error : out unsigned(1 downto 0);
      read_resp_error  : out unsigned(1 downto 0);
      shutdown         : in  std_logic;

      frame_size_lines : in unsigned(7 downto 0);
      delay_cntto      : in unsigned(15 downto 0);

      -- interface from acceleration core
      din      : in  unsigned(127 downto 0);
      wr       : in  std_logic;
      wr_ready : out std_logic;

      -- test and verification
      state_out : out unsigned(3 downto 0);
      dcount_out : out unsigned(10 downto 0);

      M_AXI_AWID    : out std_logic_vector(C_M_AXI_ID_WIDTH-1 downto 0);
      M_AXI_AWADDR  : out std_logic_vector(C_M_AXI_ADDR_WIDTH-1 downto 0);
      M_AXI_AWLEN   : out std_logic_vector(7 downto 0);
      M_AXI_AWSIZE  : out std_logic_vector(2 downto 0);
      M_AXI_AWBURST : out std_logic_vector(1 downto 0);
      M_AXI_AWLOCK  : out std_logic;
      M_AXI_AWCACHE : out std_logic_vector(3 downto 0);
      M_AXI_AWPROT  : out std_logic_vector(2 downto 0);
      M_AXI_AWQOS   : out std_logic_vector(3 downto 0);
      M_AXI_AWUSER  : out std_logic_vector(C_M_AXI_AWUSER_WIDTH-1 downto 0);
      M_AXI_AWVALID : out std_logic;
      M_AXI_AWREADY : in  std_logic;
      M_AXI_WDATA   : out std_logic_vector(C_M_AXI_DATA_WIDTH-1 downto 0);
      M_AXI_WSTRB   : out std_logic_vector(C_M_AXI_DATA_WIDTH/8-1 downto 0);
      M_AXI_WLAST   : out std_logic;
      M_AXI_WUSER   : out std_logic_vector(C_M_AXI_WUSER_WIDTH-1 downto 0);
      M_AXI_WVALID  : out std_logic;
      M_AXI_WREADY  : in  std_logic;
      M_AXI_BID     : in  std_logic_vector(C_M_AXI_ID_WIDTH-1 downto 0);
      M_AXI_BRESP   : in  std_logic_vector(1 downto 0);
      M_AXI_BUSER   : in  std_logic_vector(C_M_AXI_BUSER_WIDTH-1 downto 0);
      M_AXI_BVALID  : in  std_logic;
      M_AXI_BREADY  : out std_logic;
      M_AXI_ARID    : out std_logic_vector(C_M_AXI_ID_WIDTH-1 downto 0);
      M_AXI_ARADDR  : out std_logic_vector(C_M_AXI_ADDR_WIDTH-1 downto 0);
      M_AXI_ARLEN   : out std_logic_vector(7 downto 0);
      M_AXI_ARSIZE  : out std_logic_vector(2 downto 0);
      M_AXI_ARBURST : out std_logic_vector(1 downto 0);
      M_AXI_ARLOCK  : out std_logic;
      M_AXI_ARCACHE : out std_logic_vector(3 downto 0);
      M_AXI_ARPROT  : out std_logic_vector(2 downto 0);
      M_AXI_ARQOS   : out std_logic_vector(3 downto 0);
      M_AXI_ARUSER  : out std_logic_vector(C_M_AXI_ARUSER_WIDTH-1 downto 0);
      M_AXI_ARVALID : out std_logic;
      M_AXI_ARREADY : in  std_logic;
      M_AXI_RID     : in  std_logic_vector(C_M_AXI_ID_WIDTH-1 downto 0);
      M_AXI_RDATA   : in  std_logic_vector(C_M_AXI_DATA_WIDTH-1 downto 0);
      M_AXI_RRESP   : in  std_logic_vector(1 downto 0);
      M_AXI_RLAST   : in  std_logic;
      M_AXI_RUSER   : in  std_logic_vector(C_M_AXI_RUSER_WIDTH-1 downto 0);
      M_AXI_RVALID  : in  std_logic;
      M_AXI_RREADY  : out std_logic);
  end component hwaccel_axim_out;

  -- system
  signal rst        : std_logic;
  signal en         : std_logic;
  signal status_clr : std_logic;
  signal shutdown   : std_logic;

  -- ddr interface
  signal base_address_in    : unsigned(31 downto 0);
  signal base_address_out   : unsigned(31 downto 0);
  signal write_resp_error   : unsigned(3 downto 0);
  signal read_resp_error    : unsigned(3 downto 0);
  signal axim_nrst          : std_logic;
  signal delay_cntto        : unsigned(15 downto 0);
  signal axim_page_num      : unsigned(7 downto 0);
  signal frame_size_lines   : unsigned(7 downto 0);
  signal payload_size_bytes : unsigned(11 downto 0);

  -- from cpu (axim_in)
  signal axim_in_frame_read_done  : std_logic;
  signal axim_in_frame_read_start : std_logic;

  -- interface to acceleration core
  signal axim_in_fifo_empty : std_logic;
  signal axim_in_fifo_dout  : unsigned(127 downto 0);
  signal axim_in_fifo_rd    : std_logic;

  -- to cpu (axim_out)
  signal axim_out_din      : unsigned(127 downto 0);
  signal axim_out_wr       : std_logic;
  signal axim_out_wr_ready : std_logic;

  -- test and verification
  signal axim_in_araddr_int : std_logic_vector(31 downto 0);
  signal axim_in_arlen_int  : std_logic_vector(7 downto 0);
  signal axim_in_rready_int : std_logic;
  signal axim_in_bready_int : std_logic;
  signal axim_in_wdata_int  : std_logic_vector(127 downto 0);
  signal axim_in_wvalid_int : std_logic;
  signal axim_in_wlast_int  : std_logic;

  signal axim_out_araddr_int : std_logic_vector(31 downto 0);
  signal axim_out_arlen_int  : std_logic_vector(7 downto 0);
  signal axim_out_rready_int : std_logic;
  signal axim_out_bready_int : std_logic;
  signal axim_out_wdata_int  : std_logic_vector(127 downto 0);
  signal axim_out_wvalid_int : std_logic;
  signal axim_out_wlast_int  : std_logic;

begin

  axi_lite : hwaccel_axi_lite
    generic map (
      C_S_AXI_DATA_WIDTH => C_axi_lite_DATA_WIDTH,
      C_S_AXI_ADDR_WIDTH => C_axi_lite_ADDR_WIDTH)
    port map (
      rst        => rst,
      en         => en,
      status_clr => status_clr,
      shutdown   => shutdown,
      enet1_nrst => enet1_nrst,

      base_address_in    => base_address_in,
      base_address_out   => base_address_out,
      write_resp_error   => write_resp_error,
      read_resp_error    => read_resp_error,
      axim_nrst          => axim_nrst,
      delay_cntto        => delay_cntto,
      axim_page_num      => axim_page_num,
      frame_size_lines   => frame_size_lines,
      payload_size_bytes => payload_size_bytes,

      S_AXI_ACLK    => axiclk,
      S_AXI_ARESETN => aresetn,
      S_AXI_AWADDR  => axi_lite_awaddr,
      S_AXI_AWPROT  => axi_lite_awprot,
      S_AXI_AWVALID => axi_lite_awvalid,
      S_AXI_AWREADY => axi_lite_awready,
      S_AXI_WDATA   => axi_lite_wdata,
      S_AXI_WSTRB   => axi_lite_wstrb,
      S_AXI_WVALID  => axi_lite_wvalid,
      S_AXI_WREADY  => axi_lite_wready,
      S_AXI_BRESP   => axi_lite_bresp,
      S_AXI_BVALID  => axi_lite_bvalid,
      S_AXI_BREADY  => axi_lite_bready,
      S_AXI_ARADDR  => axi_lite_araddr,
      S_AXI_ARPROT  => axi_lite_arprot,
      S_AXI_ARVALID => axi_lite_arvalid,
      S_AXI_ARREADY => axi_lite_arready,
      S_AXI_RDATA   => axi_lite_rdata,
      S_AXI_RRESP   => axi_lite_rresp,
      S_AXI_RVALID  => axi_lite_rvalid,
      S_AXI_RREADY  => axi_lite_rready);

  axim_in : hwaccel_axim_in
    generic map (
      C_M_AXI_ID_WIDTH     => C_axim_ID_WIDTH,
      C_M_AXI_ADDR_WIDTH   => C_axim_ADDR_WIDTH,
      C_M_AXI_DATA_WIDTH   => C_axim_DATA_WIDTH,
      C_M_AXI_AWUSER_WIDTH => C_axim_AWUSER_WIDTH,
      C_M_AXI_ARUSER_WIDTH => C_axim_ARUSER_WIDTH,
      C_M_AXI_WUSER_WIDTH  => C_axim_WUSER_WIDTH,
      C_M_AXI_RUSER_WIDTH  => C_axim_RUSER_WIDTH,
      C_M_AXI_BUSER_WIDTH  => C_axim_BUSER_WIDTH)
    port map (
      axiclk           => axiclk,
      aresetn          => aresetn,
      nrst             => axim_nrst,
      base_address     => base_address_in,
      write_resp_error => write_resp_error(1 downto 0),
      read_resp_error  => read_resp_error(1 downto 0),
      shutdown         => shutdown,

      frame_size_lines => frame_size_lines,
      delay_cntto      => delay_cntto,

      frame_read_done  => axim_in_frame_read_done,
      frame_read_start => axim_in_frame_read_start,

      fifo_empty => axim_in_fifo_empty,
      fifo_dout  => axim_in_fifo_dout,
      fifo_rd    => axim_in_fifo_rd,

      -- test and verification
      state_out => axim_in_state_scope,

      M_AXI_AWID    => axim_in_awid,
      M_AXI_AWADDR  => axim_in_awaddr,
      M_AXI_AWLEN   => axim_in_awlen,
      M_AXI_AWSIZE  => axim_in_awsize,
      M_AXI_AWBURST => axim_in_awburst,
      M_AXI_AWLOCK  => axim_in_awlock,
      M_AXI_AWCACHE => axim_in_awcache,
      M_AXI_AWPROT  => axim_in_awprot,
      M_AXI_AWQOS   => axim_in_awqos,
      M_AXI_AWUSER  => axim_in_awuser,
      M_AXI_AWVALID => axim_in_awvalid,
      M_AXI_AWREADY => axim_in_awready,
      M_AXI_WDATA   => axim_in_wdata_int,
      M_AXI_WSTRB   => axim_in_wstrb,
      M_AXI_WLAST   => axim_in_wlast_int,
      M_AXI_WUSER   => axim_in_wuser,
      M_AXI_WVALID  => axim_in_wvalid_int,
      M_AXI_WREADY  => axim_in_wready,
      M_AXI_BID     => axim_in_bid,
      M_AXI_BRESP   => axim_in_bresp,
      M_AXI_BUSER   => axim_in_buser,
      M_AXI_BVALID  => axim_in_bvalid,
      M_AXI_BREADY  => axim_in_bready_int,
      M_AXI_ARID    => axim_in_arid,
      M_AXI_ARADDR  => axim_in_araddr_int,
      M_AXI_ARLEN   => axim_in_arlen_int,
      M_AXI_ARSIZE  => axim_in_arsize,
      M_AXI_ARBURST => axim_in_arburst,
      M_AXI_ARLOCK  => axim_in_arlock,
      M_AXI_ARCACHE => axim_in_arcache,
      M_AXI_ARPROT  => axim_in_arprot,
      M_AXI_ARQOS   => axim_in_arqos,
      M_AXI_ARUSER  => axim_in_aruser,
      M_AXI_ARVALID => axim_in_arvalid,
      M_AXI_ARREADY => axim_in_arready,
      M_AXI_RID     => axim_in_rid,
      M_AXI_RDATA   => axim_in_rdata,
      M_AXI_RRESP   => axim_in_rresp,
      M_AXI_RLAST   => axim_in_rlast,
      M_AXI_RUSER   => axim_in_ruser,
      M_AXI_RVALID  => axim_in_rvalid,
      M_AXI_RREADY  => axim_in_rready_int);

  axim_out : hwaccel_axim_out
    generic map (
      C_M_AXI_ID_WIDTH     => C_axim_ID_WIDTH,
      C_M_AXI_ADDR_WIDTH   => C_axim_ADDR_WIDTH,
      C_M_AXI_DATA_WIDTH   => C_axim_DATA_WIDTH,
      C_M_AXI_AWUSER_WIDTH => C_axim_AWUSER_WIDTH,
      C_M_AXI_ARUSER_WIDTH => C_axim_ARUSER_WIDTH,
      C_M_AXI_WUSER_WIDTH  => C_axim_WUSER_WIDTH,
      C_M_AXI_RUSER_WIDTH  => C_axim_RUSER_WIDTH,
      C_M_AXI_BUSER_WIDTH  => C_axim_BUSER_WIDTH)
    port map (
      axiclk           => axiclk,
      rst              => rst,
      aresetn          => aresetn,
      nrst             => axim_nrst,
      base_address     => base_address_out,
      write_resp_error => write_resp_error(3 downto 2),
      read_resp_error  => read_resp_error(3 downto 2),
      frame_size_lines => frame_size_lines,
      delay_cntto      => delay_cntto,
      shutdown         => shutdown,

      din      => axim_out_din,
      wr       => axim_out_wr,
      wr_ready => axim_out_wr_ready,

      -- test and verification
      state_out => axim_out_state_scope,
      dcount_out => axim_out_dcount_scope;

      M_AXI_AWID    => axim_out_awid,
      M_AXI_AWADDR  => axim_out_awaddr,
      M_AXI_AWLEN   => axim_out_awlen,
      M_AXI_AWSIZE  => axim_out_awsize,
      M_AXI_AWBURST => axim_out_awburst,
      M_AXI_AWLOCK  => axim_out_awlock,
      M_AXI_AWCACHE => axim_out_awcache,
      M_AXI_AWPROT  => axim_out_awprot,
      M_AXI_AWQOS   => axim_out_awqos,
      M_AXI_AWUSER  => axim_out_awuser,
      M_AXI_AWVALID => axim_out_awvalid,
      M_AXI_AWREADY => axim_out_awready,
      M_AXI_WDATA   => axim_out_wdata_int,
      M_AXI_WSTRB   => axim_out_wstrb,
      M_AXI_WLAST   => axim_out_wlast_int,
      M_AXI_WUSER   => axim_out_wuser,
      M_AXI_WVALID  => axim_out_wvalid_int,
      M_AXI_WREADY  => axim_out_wready,
      M_AXI_BID     => axim_out_bid,
      M_AXI_BRESP   => axim_out_bresp,
      M_AXI_BUSER   => axim_out_buser,
      M_AXI_BVALID  => axim_out_bvalid,
      M_AXI_BREADY  => axim_out_bready_int,
      M_AXI_ARID    => axim_out_arid,
      M_AXI_ARADDR  => axim_out_araddr_int,
      M_AXI_ARLEN   => axim_out_arlen_int,
      M_AXI_ARSIZE  => axim_out_arsize,
      M_AXI_ARBURST => axim_out_arburst,
      M_AXI_ARLOCK  => axim_out_arlock,
      M_AXI_ARCACHE => axim_out_arcache,
      M_AXI_ARPROT  => axim_out_arprot,
      M_AXI_ARQOS   => axim_out_arqos,
      M_AXI_ARUSER  => axim_out_aruser,
      M_AXI_ARVALID => axim_out_arvalid,
      M_AXI_ARREADY => axim_out_arready,
      M_AXI_RID     => axim_out_rid,
      M_AXI_RDATA   => axim_out_rdata,
      M_AXI_RRESP   => axim_out_rresp,
      M_AXI_RLAST   => axim_out_rlast,
      M_AXI_RUSER   => axim_out_ruser,
      M_AXI_RVALID  => axim_out_rvalid,
      M_AXI_RREADY  => axim_out_rready_int);

  -- axim_in data in goes to axim_out
  axim_out_din <= axim_in_fifo_dout;
  process(axiclk)
  begin
    if rising_edge(axiclk) then
      axim_in_fifo_rd <= '0';
      axim_out_wr     <= '0';
      if rst = '0' and axim_in_fifo_empty = '0' then
        if axim_out_wr_ready = '1' and axim_out_wr = '0' then
          axim_out_wr     <= '1';
          axim_in_fifo_rd <= '1';
        end if;
      end if;
    end if;
  end process;

  -- output mappings
  axim_in_araddr <= axim_in_araddr_int;
  axim_in_arlen  <= axim_in_arlen_int;
  axim_in_rready <= axim_in_rready_int;
  axim_in_bready <= axim_in_bready_int;
  axim_in_wdata  <= axim_in_wdata_int;
  axim_in_wvalid <= axim_in_wvalid_int;
  axim_in_wlast  <= axim_in_wlast_int;

  axim_out_araddr <= axim_out_araddr_int;
  axim_out_arlen  <= axim_out_arlen_int;
  axim_out_rready <= axim_out_rready_int;
  axim_out_bready <= axim_out_bready_int;
  axim_out_wdata  <= axim_out_wdata_int;
  axim_out_wvalid <= axim_out_wvalid_int;
  axim_out_wlast  <= axim_out_wlast_int;

  -- test and verification
  axim_in_rdata_scope   <= axim_in_rdata;
  axim_in_rvalid_scope  <= axim_in_rvalid;
  axim_in_rlast_scope   <= axim_in_rlast;
  axim_in_bvalid_scope  <= axim_in_bvalid;
  axim_in_wready_scope  <= axim_in_wready;
  axim_in_arready_scope <= axim_in_arready;

  axim_in_araddr_scope <= axim_in_araddr_int;
  axim_in_arlen_scope  <= axim_in_arlen_int;
  axim_in_rready_scope <= axim_in_rready_int;
  axim_in_bready_scope <= axim_in_bready_int;
  axim_in_wdata_scope  <= axim_in_wdata_int;
  axim_in_wvalid_scope <= axim_in_wvalid_int;
  axim_in_wlast_scope  <= axim_in_wlast_int;

  axim_out_rdata_scope   <= axim_out_rdata;
  axim_out_rvalid_scope  <= axim_out_rvalid;
  axim_out_rlast_scope   <= axim_out_rlast;
  axim_out_bvalid_scope  <= axim_out_bvalid;
  axim_out_wready_scope  <= axim_out_wready;
  axim_out_arready_scope <= axim_out_arready;

  axim_out_araddr_scope <= axim_out_araddr_int;
  axim_out_arlen_scope  <= axim_out_arlen_int;
  axim_out_rready_scope <= axim_out_rready_int;
  axim_out_bready_scope <= axim_out_bready_int;
  axim_out_wdata_scope  <= axim_out_wdata_int;
  axim_out_wvalid_scope <= axim_out_wvalid_int;
  axim_out_wlast_scope  <= axim_out_wlast_int;

  axim_in_fifo_rd_scope    <= axim_in_fifo_rd;
  axim_out_wr_scope        <= axim_out_wr;
  axim_out_wr_ready_scope  <= axim_out_wr_ready;
  axim_in_fifo_empty_scope <= axim_in_fifo_empty;
  axim_in_fifo_dout_scope  <= axim_in_fifo_dout;
  axim_out_din_scope       <= axim_out_din;

end arch;
