library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

entity gpio_v1_0 is
  generic (
    -- Parameters of Axi Slave Bus Interface axi_lite
    C_axi_lite_DATA_WIDTH : integer := 32;
    C_axi_lite_ADDR_WIDTH : integer := 6
    );
  port (
    regout0 : out std_logic_vector(31 downto 0);
    regout1 : out std_logic_vector(31 downto 0);
    regout2 : out std_logic_vector(31 downto 0);
    regout3 : out std_logic_vector(31 downto 0);

    regin4 : in std_logic_vector(31 downto 0);
    regin5 : in std_logic_vector(31 downto 0);
    regin6 : in std_logic_vector(31 downto 0);
    regin7 : in std_logic_vector(31 downto 0);

    regout8  : out std_logic_vector(31 downto 0);
    regout9  : out std_logic_vector(31 downto 0);
    regout10 : out std_logic_vector(31 downto 0);
    regout11 : out std_logic_vector(31 downto 0);

    regin12 : in std_logic_vector(31 downto 0);
    regin13 : in std_logic_vector(31 downto 0);
    regin14 : in std_logic_vector(31 downto 0);
    regin15 : in std_logic_vector(31 downto 0);

    -- Ports of Axi Slave Bus Interface axi_lite
    axi_lite_aclk    : in  std_logic;
    axi_lite_aresetn : in  std_logic;
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
    axi_lite_rready  : in  std_logic
    );
end gpio_v1_0;

architecture arch_imp of gpio_v1_0 is

  -- component declaration
  component gpio_v1_0_axi_lite is
    generic (
      C_S_AXI_DATA_WIDTH : integer := 32;
      C_S_AXI_ADDR_WIDTH : integer := 6
      );
    port (
      regout0 : out std_logic_vector(31 downto 0);
      regout1 : out std_logic_vector(31 downto 0);
      regout2 : out std_logic_vector(31 downto 0);
      regout3 : out std_logic_vector(31 downto 0);

      regin4 : in std_logic_vector(31 downto 0);
      regin5 : in std_logic_vector(31 downto 0);
      regin6 : in std_logic_vector(31 downto 0);
      regin7 : in std_logic_vector(31 downto 0);

      regout8  : out std_logic_vector(31 downto 0);
      regout9  : out std_logic_vector(31 downto 0);
      regout10 : out std_logic_vector(31 downto 0);
      regout11 : out std_logic_vector(31 downto 0);

      regin12 : in std_logic_vector(31 downto 0);
      regin13 : in std_logic_vector(31 downto 0);
      regin14 : in std_logic_vector(31 downto 0);
      regin15 : in std_logic_vector(31 downto 0);

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
      S_AXI_RREADY  : in  std_logic
      );
  end component gpio_v1_0_axi_lite;

begin

-- Instantiation of Axi Bus Interface axi_lite
  gpio_v1_0_axi_lite_inst : gpio_v1_0_axi_lite
    generic map (
      C_S_AXI_DATA_WIDTH => C_axi_lite_DATA_WIDTH,
      C_S_AXI_ADDR_WIDTH => C_axi_lite_ADDR_WIDTH
      )
    port map (
      regout0 => regout0,
      regout1 => regout1,
      regout2 => regout2,
      regout3 => regout3,

      regin4 => regin4,
      regin5 => regin5,
      regin6 => regin6,
      regin7 => regin7,

      regout8  => regout8,
      regout9  => regout9,
      regout10 => regout10,
      regout11 => regout11,

      regin12 => regin12,
      regin13 => regin13,
      regin14 => regin14,
      regin15 => regin15,

      S_AXI_ACLK    => axi_lite_aclk,
      S_AXI_ARESETN => axi_lite_aresetn,
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

end arch_imp;
