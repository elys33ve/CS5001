-- TODO: check rresp on every rvalid

-- hard coding burst len (awlen) to 255
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity ddr_test_wr_axim is
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
    axiclk     : in std_logic;
    aresetn    : in std_logic;
    nrst       : in std_logic;
    stats_clr  : in std_logic;
    insert_err : in std_logic;

    master_ctr : in unsigned(31 downto 0);

    base_address          : in  unsigned(31 downto 0);
    write_resp_error      : out std_logic;
    delay_cntto           : in  unsigned(31 downto 0);
    interframe_clks_cntto : in  unsigned(31 downto 0);

    -- test and verification
    rom_addr_out  : out unsigned(7 downto 0);
    rom_dout_out  : out unsigned(127 downto 0);
    burst_cnt_out : out unsigned(9 downto 0);
    rom_sel_out   : out std_logic;
    state_out     : out unsigned(3 downto 0);

    max_clkcnt_wait : out unsigned(31 downto 0);

    -- AW
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
    -- W
    M_AXI_WDATA   : out std_logic_vector(C_M_AXI_DATA_WIDTH-1 downto 0);
    M_AXI_WSTRB   : out std_logic_vector(C_M_AXI_DATA_WIDTH/8-1 downto 0);
    M_AXI_WLAST   : out std_logic;
    M_AXI_WUSER   : out std_logic_vector(C_M_AXI_WUSER_WIDTH-1 downto 0);
    M_AXI_WVALID  : out std_logic;
    M_AXI_WREADY  : in  std_logic;
    -- B
    M_AXI_BID     : in  std_logic_vector(C_M_AXI_ID_WIDTH-1 downto 0);
    M_AXI_BRESP   : in  std_logic_vector(1 downto 0);
    M_AXI_BUSER   : in  std_logic_vector(C_M_AXI_BUSER_WIDTH-1 downto 0);
    M_AXI_BVALID  : in  std_logic;
    M_AXI_BREADY  : out std_logic;
    -- AR
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
    -- R
    M_AXI_RID     : in  std_logic_vector(C_M_AXI_ID_WIDTH-1 downto 0);
    M_AXI_RDATA   : in  std_logic_vector(C_M_AXI_DATA_WIDTH-1 downto 0);
    M_AXI_RRESP   : in  std_logic_vector(1 downto 0);
    M_AXI_RLAST   : in  std_logic;
    M_AXI_RUSER   : in  std_logic_vector(C_M_AXI_RUSER_WIDTH-1 downto 0);
    M_AXI_RVALID  : in  std_logic;
    M_AXI_RREADY  : out std_logic);
end ddr_test_wr_axim;

architecture arch of ddr_test_wr_axim is

  -- ROM1
  component pr_wr_rom1 is
    port (
      clka  : in  std_logic;
      ena   : in  std_logic;
      addra : in  std_logic_vector(7 downto 0);
      douta : out std_logic_vector(127 downto 0));
  end component pr_wr_rom1;
  -- ROM2
  component pr_wr_rom2 is
    port (
      clka  : in  std_logic;
      ena   : in  std_logic;
      addra : in  std_logic_vector(7 downto 0);
      douta : out std_logic_vector(127 downto 0));
  end component pr_wr_rom2;

  signal rom_sel      : std_logic;      -- 0=ROM1, 1=ROM2
  signal rom_addr     : unsigned(7 downto 0);
  signal rom1_dout    : std_logic_vector(127 downto 0);
  signal rom2_dout    : std_logic_vector(127 downto 0);
  signal rom_dout     : unsigned(127 downto 0);
  signal rom_dout_reg : unsigned(127 downto 0);
  signal rom1_cnt     : unsigned(63 downto 0);
  signal rom2_cnt     : unsigned(63 downto 0);
  constant ROM1_ID    : std_logic_vector(63 downto 0) := X"ABCD123401020301";
  constant ROM2_ID    : std_logic_vector(63 downto 0) := X"567811F2DE77764F";

  signal awaddr   : unsigned(C_M_AXI_ADDR_WIDTH-1 downto 0);
  signal awvalid  : std_logic;
  signal awlen    : std_logic_vector(7 downto 0);
  signal wdata    : std_logic_vector(C_M_AXI_DATA_WIDTH-1 downto 0);
  signal wlast    : std_logic;
  signal wvalid   : std_logic;
  signal bready   : std_logic;
  signal wready_1 : std_logic;

  signal araddr  : unsigned(C_M_AXI_ADDR_WIDTH-1 downto 0);
  signal rready  : std_logic;
  signal arvalid : std_logic;

  -- continuous write
  signal burst_cnt     : unsigned(9 downto 0);
  constant burst_cntto : unsigned(9 downto 0) := to_unsigned(255, 10);

  signal delay_cnt           : unsigned(31 downto 0);
  signal interframe_clks_cnt : unsigned(31 downto 0);

  -- stats
  signal clkcnt_wait         : unsigned(31 downto 0);
  signal max_clkcnt_wait_int : unsigned(31 downto 0);

  signal master_ctr_reg : unsigned(31 downto 0);
  signal insert_err_en : std_logic;
  signal insert_err_clr : std_logic;

  -- state machine
  signal state              : unsigned(3 downto 0);
  constant SRST             : unsigned(3 downto 0) := "0000";
  constant SWAIT_HDR_ADDR   : unsigned(3 downto 0) := "0001";
  constant SWAIT_HDR_DATA   : unsigned(3 downto 0) := "0010";
  constant SSTART_BURST     : unsigned(3 downto 0) := "0011";  -- Write Address Request
  constant SWRITE_HDR       : unsigned(3 downto 0) := "0100";  -- Write ID and count
  constant SWRITE_DATA      : unsigned(3 downto 0) := "0101";  -- Write Data
  constant SBURST_DONE      : unsigned(3 downto 0) := "0110";  -- Await Write Response
  constant SWRITE_DONE      : unsigned(3 downto 0) := "0111";
  constant SINTERFRAME_CLKS : unsigned(3 downto 0) := "1000";
  constant SDELAY           : unsigned(3 downto 0) := "1111";

begin

  M_AXI_AWID    <= (others => '0');
  M_AXI_AWADDR  <= std_logic_vector(awaddr);
  M_AXI_AWLEN   <= awlen;
  M_AXI_AWSIZE  <= "100";               -- 16  bytes, hard-coded
  M_AXI_AWBURST <= "01";
  M_AXI_AWLOCK  <= '0';
  M_AXI_AWCACHE <= "0010";
  M_AXI_AWPROT  <= "000";
  M_AXI_AWQOS   <= x"0";
  M_AXI_AWUSER  <= (others => '1');
  M_AXI_AWVALID <= awvalid;
  M_AXI_WDATA   <= wdata;
  M_AXI_WSTRB   <= (others => '1');
  M_AXI_WLAST   <= wlast;
  M_AXI_WUSER   <= (others => '0');
  M_AXI_WVALID  <= wvalid;
  M_AXI_BREADY  <= bready;

  M_AXI_ARADDR  <= std_logic_vector(araddr);
  M_AXI_ARID    <= (others => '0');
  M_AXI_ARVALID <= arvalid;
  M_AXI_RREADY  <= rready;
  M_AXI_ARUSER  <= (others => '0');
  M_AXI_ARQOS   <= (others => '0');
  M_AXI_ARCACHE <= "0010";
  M_AXI_ARPROT  <= "000";
  M_AXI_ARSIZE  <= "100";               -- 16 byte bus width
  M_AXI_ARBURST <= "01";                -- increment
  M_AXI_ARLOCK  <= '0';
  M_AXI_ARLEN   <= X"00";

  awlen <= X"FF";

  ---------------------------------
  -- ROM interface 
  ---------------------------------
  rom1 : pr_wr_rom1 port map (
    clka  => axiclk,
    ena   => '1',
    addra => std_logic_vector(rom_addr),
    douta => rom1_dout);
  rom2 : pr_wr_rom2 port map (
    clka  => axiclk,
    ena   => '1',
    addra => std_logic_vector(rom_addr),
    douta => rom2_dout);

  rom_dout <= unsigned(rom1_dout) when rom_sel = '0' else unsigned(rom2_dout);

  ---------------------------------
  -- insert err
  ---------------------------------
  process(axiclk)
  begin
    if rising_edge(axiclk) then
      if nrst = '0' then
        insert_err_en <= '0';
      elsif insert_err_clr = '1' then
        insert_err_en <= '0';
      elsif insert_err = '1' then
        insert_err_en <= '1';
      end if;
    end if;
  end process;
    
  ---------------------------------
  -- main process
  ---------------------------------
  process(axiclk)
  begin
    if rising_edge (axiclk) then
      wready_1        <= M_AXI_WREADY;
      max_clkcnt_wait <= max_clkcnt_wait_int;
      master_ctr_reg  <= master_ctr;
      insert_err_clr <= '0';
      -- reset
      if aresetn = '0' or nrst = '0' then
        state               <= SRST;
        awaddr              <= base_address;
        araddr              <= base_address;
        awvalid             <= '0';
        wvalid              <= '0';
        bready              <= '0';
        rready              <= '0';
        arvalid             <= '0';
        burst_cnt           <= to_unsigned(0, 10);
        write_resp_error    <= '0';
        rom_sel             <= '0';
        rom_addr            <= to_unsigned(0, 8);
        rom_dout_reg        <= to_unsigned(0, 128);
        delay_cnt           <= to_unsigned(0, 32);
        clkcnt_wait         <= to_unsigned(0, 32);
        max_clkcnt_wait_int <= to_unsigned(0, 32);
      ---------------------------------
      -- SRST       -- reset
      ---------------------------------
      elsif state = SRST then
        state     <= SWAIT_HDR_ADDR;
        awaddr    <= base_address;
        burst_cnt <= to_unsigned(0, 10);
        rom_sel   <= '0';
        arvalid   <= '1';
      ---------------------------------
      -- SWAIT_HDR_ADDR
      ---------------------------------
      elsif state = SWAIT_HDR_ADDR then
        state <= SWAIT_HDR_ADDR;
        if stats_clr = '1' then
          clkcnt_wait <= to_unsigned(0, 32);
        else
          clkcnt_wait <= clkcnt_wait + 1;
        end if;
        if M_AXI_ARREADY = '1' then
          arvalid <= '0';
          rready  <= '1';
          state   <= SWAIT_HDR_DATA;
        end if;
      ---------------------------------
      -- SWAIT_HDR_DATA
      ---------------------------------
      elsif state = SWAIT_HDR_DATA then
        state       <= SWAIT_HDR_DATA;
        rom_addr    <= to_unsigned(0, 8);
        clkcnt_wait <= clkcnt_wait + 1;
        if M_AXI_RVALID = '1' then
          arvalid <= '0';
          rready  <= '0';
          if M_AXI_RDATA(127 downto 120) = X"00" then
            state       <= SSTART_BURST;
            clkcnt_wait <= to_unsigned(0, 32);
            if clkcnt_wait > max_clkcnt_wait_int then
              max_clkcnt_wait_int <= clkcnt_wait;
            end if;
          else                          -- invalid or no space
            delay_cnt <= to_unsigned(0, 32);
            state     <= SDELAY;
          end if;
        end if;
      ---------------------------------
      -- SSTART_BURST  
      ---------------------------------
      elsif state = SSTART_BURST then
        state   <= SSTART_BURST;
        awvalid <= '1';
        if awvalid = '1' and M_AXI_AWREADY = '1' then
          state        <= SWRITE_HDR;
          rom_addr     <= to_unsigned(1, 8);
          rom_dout_reg <= rom_dout;
          if rom_sel = '0' then
            wdata <= ROM1_ID & std_logic_vector(master_ctr_reg) &
                     std_logic_vector(rom1_cnt(31 downto 0));
          else
            wdata <= ROM2_ID & std_logic_vector(master_ctr_reg) &
                     std_logic_vector(rom2_cnt(31 downto 0));
          end if;
          awvalid <= '0';
          wvalid  <= '1';
        end if;
      ----------------------------------
      -- SWRITE_HDR
      ----------------------------------
      elsif state = SWRITE_HDR then
        state <= SWRITE_HDR;
        if M_AXI_WREADY = '1' then
          state               <= SWRITE_DATA;
          wdata(127 downto 1) <= std_logic_vector(rom_dout_reg(127 downto 1));
          if insert_err_en = '0' then
            wdata(0) <= rom_dout_reg(0);
          else
            wdata(0) <= not rom_dout_reg(0);
            insert_err_clr <= '1';
          end if;
          rom_addr  <= to_unsigned(2, 8);
          burst_cnt <= to_unsigned(1, 10);
        end if;
      ----------------------------------
      -- SWRITE_DATA        
      ----------------------------------
      elsif state = SWRITE_DATA then
        state <= SWRITE_DATA;
        if M_AXI_WREADY = '1' then
          burst_cnt    <= burst_cnt + 1;
          rom_addr     <= rom_addr + 1;
          rom_dout_reg <= rom_dout;
          if wready_1 = '1' then
            wdata(127 downto 1) <= std_logic_vector(rom_dout(127 downto 1));
            if insert_err_en = '0' then
              wdata(0)            <= rom_dout(0);
            else
              wdata(0)            <= not rom_dout(0);
              insert_err_clr <= '1';
            end if;
          else
            wdata(127 downto 1) <= std_logic_vector(rom_dout_reg(127 downto 1));
            if insert_err_en = '0' then
              wdata(0)            <= rom_dout_reg(0);
            else
              wdata(0) <= not rom_dout_reg(0);
              insert_err_clr <= '1';
            end if;
          end if;
          -- check if last burst
          if burst_cnt = burst_cntto then
            state  <= SBURST_DONE;
            wvalid <= '0';
            bready <= '1';
          end if;
        elsif wready_1 = '1' then
          rom_dout_reg <= rom_dout;
        end if;
      ----------------------------------
      --SBURST_DONE    
      ----------------------------------
      elsif state = SBURST_DONE then
        if M_AXI_BVALID = '1' then
          bready <= '0';
          state  <= SWRITE_DONE;
        end if;
      ---------------------------------
      --SWRITE_DONE
      ---------------------------------
      elsif state = SWRITE_DONE then
        state               <= SINTERFRAME_CLKS;
        arvalid             <= '0';
        burst_cnt           <= to_unsigned(0, 10);
        interframe_clks_cnt <= to_unsigned(0, 32);
        rom_sel             <= not rom_sel;
      ---------------------------------
      --SINTERFRAME_CLKS
      ---------------------------------
      elsif state = SINTERFRAME_CLKS then
        state               <= SINTERFRAME_CLKS;
        interframe_clks_cnt <= interframe_clks_cnt + 1;
        if interframe_clks_cnt = interframe_clks_cntto then
          arvalid <= '1';
          state   <= SWAIT_HDR_ADDR;
        end if;
      ---------------------------------
      --SDELAY
      ---------------------------------
      elsif state = SDELAY then
        state       <= SDELAY;
        delay_cnt   <= delay_cnt + 1;
        clkcnt_wait <= clkcnt_wait + 1;
        if delay_cnt = delay_cntto then
          arvalid <= '1';
          state   <= SWAIT_HDR_ADDR;
        end if;
      ---------------------------------
      --INVALID
      ---------------------------------
      else
        state <= SRST;
      end if;
    end if;
  end process;

  wlast <= '1' when wvalid = '1' and burst_cnt = X"FF" else '0';

  ---------------------------------
  -- write errors
  ---------------------------------
  process(axiclk)
  begin
    if rising_edge(axiclk) then
      if aresetn = '0' or nrst = '0' then
        write_resp_error <= '0';
      elsif bready = '1' and M_AXI_BVALID = '1' and M_AXI_BRESP(1) = '1' then
        write_resp_error <= '1';
      end if;
    end if;
  end process;

  ----------------------------------
  -- rom
  ----------------------------------
  -- rom cnt for hdr
  process(axiclk)
  begin
    if rising_edge (axiclk) then
      if state = SRST then
        rom1_cnt <= (others => '0');
        rom2_cnt <= (others => '0');
      elsif state = SWRITE_DONE then
        if rom_sel = '0' then
          rom1_cnt <= rom1_cnt + 1;
        else
          rom2_cnt <= rom2_cnt + 1;
        end if;
      end if;
    end if;
  end process;

  ----------------------------------
  -- test and verification
  ----------------------------------
  rom_addr_out  <= rom_addr;
  rom_dout_out  <= rom_dout;
  burst_cnt_out <= burst_cnt;
  rom_sel_out   <= rom_sel;
  state_out     <= state;
end arch;
