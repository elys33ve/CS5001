library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity ddr_test_rd is
  generic (
    C_axi_lite_DATA_WIDTH : integer := 32;
    C_axi_lite_ADDR_WIDTH : integer := 6;

    C_axim_ID_WIDTH     : integer := 1;
    C_axim_ADDR_WIDTH   : integer := 32;
    C_axim_DATA_WIDTH   : integer := 128;
    C_axim_AWUSER_WIDTH : integer := 0;
    C_axim_ARUSER_WIDTH : integer := 0;
    C_axim_WUSER_WIDTH  : integer := 0;
    C_axim_RUSER_WIDTH  : integer := 0;
    C_axim_BUSER_WIDTH  : integer := 0);
  port (
    axiclk  : in std_logic;
    aresetn : in std_logic;

    master_ctr : in unsigned(31 downto 0);

    -- test and verification
    frame_cnt_diff_scope : out unsigned(31 downto 0);
    max_frame_cnt_diff_scope : out unsigned(31 downto 0);

    hdr_err_scope        : out std_logic;
    sel_err_scope        : out std_logic;
    continuity_err_scope : out std_logic;
    num_lines_err_scope  : out std_logic;

    read_resp_error_scope  : out std_logic;
    write_resp_error_scope : out std_logic;

    rom_addr_scope     : out unsigned(7 downto 0);
    rom_dout_scope     : out unsigned(127 downto 0);
    rom_dout_reg_scope : out unsigned(127 downto 0);
    line_cnt_scope     : out unsigned(7 downto 0);
    rom_sel_scope      : out std_logic;
    state_scope        : out unsigned(3 downto 0);

    baseaddr_scope : out unsigned(31 downto 0);

    err_cnt_scope : out unsigned(31 downto 0);
    err_scope     : out std_logic;

    max_clkcnt_wait_scope : out unsigned(31 downto 0);

    axim_rdata_scope   : out std_logic_vector(127 downto 0);
    axim_rvalid_scope  : out std_logic;
    axim_rlast_scope   : out std_logic;
    axim_rready_scope  : out std_logic;
    axim_araddr_scope  : out std_logic_vector(31 downto 0);
    axim_arvalid_scope : out std_logic;
    axim_arready_scope : out std_logic;

    axim_bresp_scope  : out std_logic_vector(1 downto 0);
    axim_bready_scope : out std_logic;
    axim_bvalid_scope : out std_logic;

    axim_wdata_scope   : out std_logic_vector(127 downto 0);
    axim_wvalid_scope  : out std_logic;
    axim_wlast_scope   : out std_logic;
    axim_wready_scope  : out std_logic;
    axim_awaddr_scope  : out std_logic_vector(31 downto 0);
    axim_awvalid_scope : out std_logic;
    axim_awready_scope : out std_logic;

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

    axim_awid    : out std_logic_vector(C_axim_ID_WIDTH-1 downto 0);
    axim_awaddr  : out std_logic_vector(C_axim_ADDR_WIDTH-1 downto 0);
    axim_awlen   : out std_logic_vector(7 downto 0);
    axim_awsize  : out std_logic_vector(2 downto 0);
    axim_awburst : out std_logic_vector(1 downto 0);
    axim_awlock  : out std_logic;
    axim_awcache : out std_logic_vector(3 downto 0);
    axim_awprot  : out std_logic_vector(2 downto 0);
    axim_awqos   : out std_logic_vector(3 downto 0);
    axim_awuser  : out std_logic_vector(C_axim_AWUSER_WIDTH-1 downto 0);
    axim_awvalid : out std_logic;
    axim_awready : in  std_logic;
    axim_wdata   : out std_logic_vector(C_axim_DATA_WIDTH-1 downto 0);
    axim_wstrb   : out std_logic_vector(C_axim_DATA_WIDTH/8-1 downto 0);
    axim_wlast   : out std_logic;
    axim_wuser   : out std_logic_vector(C_axim_WUSER_WIDTH-1 downto 0);
    axim_wvalid  : out std_logic;
    axim_wready  : in  std_logic;
    axim_bid     : in  std_logic_vector(C_axim_ID_WIDTH-1 downto 0);
    axim_bresp   : in  std_logic_vector(1 downto 0);
    axim_buser   : in  std_logic_vector(C_axim_BUSER_WIDTH-1 downto 0);
    axim_bvalid  : in  std_logic;
    axim_bready  : out std_logic;
    axim_arid    : out std_logic_vector(C_axim_ID_WIDTH-1 downto 0);
    axim_araddr  : out std_logic_vector(C_axim_ADDR_WIDTH-1 downto 0);
    axim_arlen   : out std_logic_vector(7 downto 0);
    axim_arsize  : out std_logic_vector(2 downto 0);
    axim_arburst : out std_logic_vector(1 downto 0);
    axim_arlock  : out std_logic;
    axim_arcache : out std_logic_vector(3 downto 0);
    axim_arprot  : out std_logic_vector(2 downto 0);
    axim_arqos   : out std_logic_vector(3 downto 0);
    axim_aruser  : out std_logic_vector(C_axim_ARUSER_WIDTH-1 downto 0);
    axim_arvalid : out std_logic;
    axim_arready : in  std_logic;
    axim_rid     : in  std_logic_vector(C_axim_ID_WIDTH-1 downto 0);
    axim_rdata   : in  std_logic_vector(C_axim_DATA_WIDTH-1 downto 0);
    axim_rresp   : in  std_logic_vector(1 downto 0);
    axim_rlast   : in  std_logic;
    axim_ruser   : in  std_logic_vector(C_axim_RUSER_WIDTH-1 downto 0);
    axim_rvalid  : in  std_logic;
    axim_rready  : out std_logic);
end ddr_test_rd;

architecture arch of ddr_test_rd is

  component ddr_test_rd_axi_lite is
    generic (
      C_S_AXI_DATA_WIDTH : integer := 32;
      C_S_AXI_ADDR_WIDTH : integer := 6);
    port (
      axiclk  : in std_logic;
      aresetn :    std_logic;

      nrst             : out std_logic;
      base_address     : out unsigned(31 downto 0);
      read_resp_error  : in  std_logic;
      write_resp_error : in  std_logic;

      stats_clr : out std_logic;

      err_cnt            : in unsigned(31 downto 0);
      continuity_err_cnt : in unsigned(31 downto 0);
      max_clkcnt_wait    : in unsigned(31 downto 0);
      max_frame_cnt_diff    : in unsigned(31 downto 0);

      -- AW
      S_AXI_AWADDR  : in  std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
      S_AXI_AWPROT  : in  std_logic_vector(2 downto 0);
      S_AXI_AWVALID : in  std_logic;
      S_AXI_AWREADY : out std_logic;
      -- W
      S_AXI_WDATA   : in  std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
      S_AXI_WSTRB   : in  std_logic_vector((C_S_AXI_DATA_WIDTH/8)-1 downto 0);
      S_AXI_WVALID  : in  std_logic;
      S_AXI_WREADY  : out std_logic;
      -- B
      S_AXI_BRESP   : out std_logic_vector(1 downto 0);
      S_AXI_BVALID  : out std_logic;
      S_AXI_BREADY  : in  std_logic;
      -- AR
      S_AXI_ARADDR  : in  std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
      S_AXI_ARPROT  : in  std_logic_vector(2 downto 0);
      S_AXI_ARVALID : in  std_logic;
      S_AXI_ARREADY : out std_logic;
      -- R
      S_AXI_RDATA   : out std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
      S_AXI_RRESP   : out std_logic_vector(1 downto 0);
      S_AXI_RVALID  : out std_logic;
      S_AXI_RREADY  : in  std_logic);
  end component ddr_test_rd_axi_lite;

  component ddr_test_rd_axim is
    generic (
      C_M_AXI_ID_WIDTH     : integer := 1;
      C_M_AXI_ADDR_WIDTH   : integer := 32;
      C_M_AXI_DATA_WIDTH   : integer := 128;
      C_M_AXI_AWUSER_WIDTH : integer := 0;
      C_M_AXI_ARUSER_WIDTH : integer := 0;
      C_M_AXI_WUSER_WIDTH  : integer := 0;
      C_M_AXI_RUSER_WIDTH  : integer := 0;
      C_M_AXI_BUSER_WIDTH  : integer := 0);
    port (
      axiclk    : in std_logic;
      aresetn   : in std_logic;
      stats_clr : in std_logic;

      master_ctr : in unsigned(31 downto 0);

      nrst             : in  std_logic;
      base_address     : in  unsigned(31 downto 0);
      read_resp_error  : out std_logic;
      write_resp_error : out std_logic;

      hdr_err        : out std_logic;
      sel_err        : out std_logic;
      continuity_err : out std_logic;
      num_lines_err  : out std_logic;

      -- test and verification
      rom_addr_out     : out unsigned(7 downto 0);
      rom_dout_out     : out unsigned(127 downto 0);
      rom_dout_reg_out : out unsigned(127 downto 0);
      line_cnt_out     : out unsigned(7 downto 0);
      rom_sel_out      : out std_logic;
      state_out        : out unsigned(3 downto 0);

      frame_cnt_diff : out unsigned(31 downto 0);
      max_frame_cnt_diff : out unsigned(31 downto 0);

      err : out std_logic;

      max_clkcnt_wait : out unsigned(31 downto 0);

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
  end component ddr_test_rd_axim;

  signal nrst : std_logic;

  signal base_address     : unsigned(31 downto 0);
  signal read_resp_error  : std_logic;
  signal write_resp_error : std_logic;

  -- error indicators and stats
  signal hdr_err        : std_logic;
  signal sel_err        : std_logic;
  signal continuity_err : std_logic;
  signal num_lines_err  : std_logic;

  signal err : std_logic;

  signal max_clkcnt_wait : unsigned(31 downto 0);

  -- stats
  signal stats_clr          : std_logic;
  signal err_cnt            : unsigned(31 downto 0);
  signal continuity_err_cnt : unsigned(31 downto 0);
  signal frame_cnt_diff            : unsigned(31 downto 0);
  signal max_frame_cnt_diff            : unsigned(31 downto 0);

  -- test and verification
  signal rom_addr_out     : unsigned(7 downto 0);
  signal rom_dout_out     : unsigned(127 downto 0);
  signal rom_dout_reg_out : unsigned(127 downto 0);
  signal line_cnt_out     : unsigned(7 downto 0);
  signal rom_sel_out      : std_logic;
  signal state_out        : unsigned(3 downto 0);

  signal axim_rready_int  : std_logic;
  signal axim_araddr_int  : std_logic_vector(31 downto 0);
  signal axim_arvalid_int : std_logic;

  signal axim_awvalid_int : std_logic;
  signal axim_awaddr_int  : std_logic_vector(31 downto 0);
  signal axim_wvalid_int  : std_logic;
  signal axim_wdata_int   : std_logic_vector(127 downto 0);
  signal axim_wlast_int   : std_logic;

  signal axim_bready_int : std_logic;

begin

  axilite : ddr_test_rd_axi_lite
    generic map (
      C_S_AXI_DATA_WIDTH => C_axi_lite_DATA_WIDTH,
      C_S_AXI_ADDR_WIDTH => C_axi_lite_ADDR_WIDTH)
    port map (
      axiclk  => axiclk,
      aresetn => aresetn,

      nrst             => nrst,
      base_address     => base_address,
      read_resp_error  => read_resp_error,
      write_resp_error => write_resp_error,

      stats_clr => stats_clr,

      err_cnt            => err_cnt,
      continuity_err_cnt => continuity_err_cnt,
      max_clkcnt_wait    => max_clkcnt_wait,
      max_frame_cnt_diff => max_frame_cnt_diff,

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

  axim : ddr_test_rd_axim
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
      axiclk    => axiclk,
      aresetn   => aresetn,
      stats_clr => stats_clr,

      master_ctr => master_ctr,

      nrst             => nrst,
      base_address     => base_address,
      read_resp_error  => read_resp_error,
      write_resp_error => write_resp_error,

      hdr_err        => hdr_err,
      sel_err        => sel_err,
      continuity_err => continuity_err,
      num_lines_err  => num_lines_err,

      frame_cnt_diff => frame_cnt_diff,
      max_frame_cnt_diff => max_frame_cnt_diff,

      -- test and verification
      rom_addr_out     => rom_addr_out,
      rom_dout_out     => rom_dout_out,
      rom_dout_reg_out => rom_dout_reg_out,
      line_cnt_out     => line_cnt_out,
      rom_sel_out      => rom_sel_out,
      state_out        => state_out,

      err => err,

      max_clkcnt_wait => max_clkcnt_wait,

      M_AXI_AWID    => axim_awid,
      M_AXI_AWADDR  => axim_awaddr_int,
      M_AXI_AWLEN   => axim_awlen,
      M_AXI_AWSIZE  => axim_awsize,
      M_AXI_AWBURST => axim_awburst,
      M_AXI_AWLOCK  => axim_awlock,
      M_AXI_AWCACHE => axim_awcache,
      M_AXI_AWPROT  => axim_awprot,
      M_AXI_AWQOS   => axim_awqos,
      M_AXI_AWUSER  => axim_awuser,
      M_AXI_AWVALID => axim_awvalid_int,
      M_AXI_AWREADY => axim_awready,
      M_AXI_WDATA   => axim_wdata_int,
      M_AXI_WSTRB   => axim_wstrb,
      M_AXI_WLAST   => axim_wlast_int,
      M_AXI_WUSER   => axim_wuser,
      M_AXI_WVALID  => axim_wvalid_int,
      M_AXI_WREADY  => axim_wready,
      M_AXI_BID     => axim_bid,
      M_AXI_BRESP   => axim_bresp,
      M_AXI_BUSER   => axim_buser,
      M_AXI_BVALID  => axim_bvalid,
      M_AXI_BREADY  => axim_bready_int,
      M_AXI_ARID    => axim_arid,
      M_AXI_ARADDR  => axim_araddr_int,
      M_AXI_ARLEN   => axim_arlen,
      M_AXI_ARSIZE  => axim_arsize,
      M_AXI_ARBURST => axim_arburst,
      M_AXI_ARLOCK  => axim_arlock,
      M_AXI_ARCACHE => axim_arcache,
      M_AXI_ARPROT  => axim_arprot,
      M_AXI_ARQOS   => axim_arqos,
      M_AXI_ARUSER  => axim_aruser,
      M_AXI_ARVALID => axim_arvalid_int,
      M_AXI_ARREADY => axim_arready,
      M_AXI_RID     => axim_rid,
      M_AXI_RDATA   => axim_rdata,
      M_AXI_RRESP   => axim_rresp,
      M_AXI_RLAST   => axim_rlast,
      M_AXI_RUSER   => axim_ruser,
      M_AXI_RVALID  => axim_rvalid,
      M_AXI_RREADY  => axim_rready_int);

  ------------------------------------
  -- stats
  ------------------------------------
  process(axiclk)
  begin
    if rising_edge(axiclk) then
      if nrst = '0' or stats_clr = '1' then
        err_cnt <= to_unsigned(0, 32);
      elsif err = '1' then
        err_cnt <= err_cnt + 1;
      end if;
    end if;
  end process;

  process(axiclk)
  begin
    if rising_edge(axiclk) then
      if nrst = '0' or stats_clr = '1' then
        continuity_err_cnt <= to_unsigned(0, 32);
      elsif continuity_err = '1' then
        continuity_err_cnt <= continuity_err_cnt + 1;
      end if;
    end if;
  end process;

  ------------------------------------
  -- mappings
  ------------------------------------
  rom_addr_scope     <= rom_addr_out;
  rom_dout_scope     <= rom_dout_out;
  rom_dout_reg_scope <= rom_dout_reg_out;
  line_cnt_scope     <= line_cnt_out;
  rom_sel_scope      <= rom_sel_out;
  state_scope        <= state_out;

  axim_rready  <= axim_rready_int;
  axim_araddr  <= axim_araddr_int;
  axim_arvalid <= axim_arvalid_int;

  axim_awvalid <= axim_awvalid_int;
  axim_awaddr  <= axim_awaddr_int;
  axim_wvalid  <= axim_wvalid_int;
  axim_wdata   <= axim_wdata_int;
  axim_wlast   <= axim_wlast_int;

  axim_bready <= axim_bready_int;

  axim_rdata_scope   <= axim_rdata;
  axim_rvalid_scope  <= axim_rvalid;
  axim_rlast_scope   <= axim_rlast;
  axim_rready_scope  <= axim_rready_int;
  axim_araddr_scope  <= axim_araddr_int;
  axim_arvalid_scope <= axim_arvalid_int;
  axim_arready_scope <= axim_arready;

  axim_awvalid_scope <= axim_awvalid_int;
  axim_awaddr_scope  <= axim_awaddr_int;
  axim_wvalid_scope  <= axim_wvalid_int;
  axim_wdata_scope   <= axim_wdata_int;
  axim_wlast_scope   <= axim_wlast_int;

  axim_wready_scope  <= axim_wready;
  axim_awready_scope <= axim_awready;

  axim_bready_scope <= axim_bready_int;
  axim_bresp_scope  <= axim_bresp;
  axim_bvalid_scope <= axim_bvalid;

  baseaddr_scope <= base_address;

  hdr_err_scope        <= hdr_err;
  sel_err_scope        <= sel_err;
  continuity_err_scope <= continuity_err;
  num_lines_err_scope  <= num_lines_err;

  err_cnt_scope <= err_cnt;
  err_scope     <= err;

  max_clkcnt_wait_scope <= max_clkcnt_wait;

  read_resp_error_scope  <= read_resp_error;
  write_resp_error_scope <= write_resp_error;

  max_frame_cnt_diff_scope <= max_frame_cnt_diff;
  frame_cnt_diff_scope <= frame_cnt_diff;

end arch;
