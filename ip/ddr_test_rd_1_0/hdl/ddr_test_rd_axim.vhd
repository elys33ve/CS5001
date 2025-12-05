-- TODO: check rresp on every rvalid

-- hard coding burst len (awlen) to 255
-- first test. only supports a burst of 1. continuously reads over 16kbytes
-- (1024 reads), starting at base_address
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity ddr_test_rd_axim is
  generic (
    -- Thread ID Width
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

    -- test/verification
    rom_addr_out     : out unsigned(7 downto 0);
    rom_dout_out     : out unsigned(127 downto 0);
    rom_dout_reg_out : out unsigned(127 downto 0);
    line_cnt_out     : out unsigned(7 downto 0);
    rom_sel_out      : out std_logic;
    state_out        : out unsigned(3 downto 0);

    frame_cnt_diff     : out unsigned(31 downto 0);
    max_frame_cnt_diff : out unsigned(31 downto 0);

    err : out std_logic;

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
end ddr_test_rd_axim;

architecture arch of ddr_test_rd_axim is

  -- ROM1
  component pr_rd_rom1 is
    port (
      clka  : in  std_logic;
      ena   : in  std_logic;
      addra : in  std_logic_vector(7 downto 0);
      douta : out std_logic_vector(127 downto 0));
  end component pr_rd_rom1;
  -- ROM2
  component pr_rd_rom2 is
    port (
      clka  : in  std_logic;
      ena   : in  std_logic;
      addra : in  std_logic_vector(7 downto 0);
      douta : out std_logic_vector(127 downto 0));
  end component pr_rd_rom2;

  -- memory interface
  signal rd_ptr         : unsigned(31 downto 0);
  signal max_rd_ptr     : unsigned(31 downto 0);
  signal rd_ptr_wrap    : std_logic := '0';
  signal hdr_base       : unsigned(63 downto 0);
  signal hdr_master_cnt : unsigned(31 downto 0);
  signal hdr_cnt        : unsigned(31 downto 0);
  signal hdr_cnt_reg    : unsigned(31 downto 0);
  signal not_ready_cnt  : unsigned(15 downto 0);
  signal line_cnt       : unsigned(7 downto 0);

  -- rom if
  signal rom_sel      : std_logic;      -- 0=ROM1, 1=ROM2
  signal rom_addr     : unsigned(7 downto 0);
  signal rom1_dout    : std_logic_vector(127 downto 0);
  signal rom2_dout    : std_logic_vector(127 downto 0);
  signal rom_dout     : unsigned(127 downto 0);
  signal rom_dout_reg : unsigned(127 downto 0);
  signal frame1_cnt   : unsigned(31 downto 0);
  signal frame2_cnt   : unsigned(31 downto 0);
  constant ROM1_ID    : unsigned(63 downto 0) := X"ABCD123401020301";
  constant ROM2_ID    : unsigned(63 downto 0) := X"567811F2DE77764F";

  -- axi signals
  signal araddr   : std_logic_vector(C_M_AXI_ADDR_WIDTH-1 downto 0);
  signal arvalid  : std_logic;
  signal rready   : std_logic;
  signal rvalid_1 : std_logic;
  signal arlen    : std_logic_vector(7 downto 0);

  signal awaddr  : std_logic_vector(C_M_AXI_ADDR_WIDTH-1 downto 0);
  signal awvalid : std_logic;
  signal awlen   : std_logic_vector(7 downto 0);
  signal wdata   : std_logic_vector(C_M_AXI_DATA_WIDTH-1 downto 0);
  signal wlast   : std_logic;
  signal wvalid  : std_logic;
  signal bready  : std_logic;

  -- stats
  signal clkcnt_wait            : unsigned(31 downto 0);
  signal max_clkcnt_wait_int    : unsigned(31 downto 0);
  signal frame_tx_cnt           : unsigned(31 downto 0);
  signal frame_rx_cnt           : unsigned(31 downto 0);
  signal frame_cnt_diff_int     : unsigned(31 downto 0);
  signal max_frame_cnt_diff_int : unsigned(31 downto 0);

  -- delay_cntto is a signal for sim purposes
  signal delay_cnt   : unsigned(15 downto 0);
  signal delay_cntto : unsigned(15 downto 0);

  -- state machine
  signal state              : unsigned(3 downto 0);
  constant SRST             : unsigned(3 downto 0) := "0000";
  constant SWAIT_HDR_ADDR   : unsigned(3 downto 0) := "0001";
  constant SWAIT_HDR_DATA   : unsigned(3 downto 0) := "0010";
  constant SREAD_ADDR       : unsigned(3 downto 0) := "0100";
  constant SREAD_DATA       : unsigned(3 downto 0) := "0101";
  constant SREAD_DONE       : unsigned(3 downto 0) := "1000";
  constant SCLEAR_PAGE_ADDR : unsigned(3 downto 0) := "1001";
  constant SCLEAR_PAGE_DATA : unsigned(3 downto 0) := "1010";
  constant SCLEAR_PAGE_DONE : unsigned(3 downto 0) := "1011";
  constant SDELAY           : unsigned(3 downto 0) := "1111";

begin

  M_AXI_AWID    <= (others => '0');
  M_AXI_AWADDR  <= awaddr;
  M_AXI_AWLEN   <= awlen;
  M_AXI_AWSIZE  <= "100";               -- 16 bytes, hard coded
  M_AXI_AWBURST <= "01";
  M_AXI_AWLOCK  <= '0';
  M_AXI_AWCACHE <= "0010";              -- set for reserved, non cache
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
  M_AXI_ARID    <= (others => '0');
  M_AXI_ARADDR  <= araddr;
  M_AXI_ARLEN   <= arlen;
  M_AXI_ARSIZE  <= "100";               -- 16 byte bus width
  M_AXI_ARBURST <= "01";                -- increment
  M_AXI_ARLOCK  <= '0';
  M_AXI_ARCACHE <= "0010";
  M_AXI_ARPROT  <= "000";
  M_AXI_ARQOS   <= x"0";
  M_AXI_ARUSER  <= (others => '1');
  M_AXI_ARVALID <= arvalid;
  M_AXI_RREADY  <= rready;

  ---------------------------------
  -- extract hdr info
  ---------------------------------
  hdr_base       <= unsigned(M_AXI_RDATA(127 downto 64));
  hdr_master_cnt <= unsigned(M_AXI_RDATA(63 downto 32));
  hdr_cnt        <= unsigned(M_AXI_RDATA(31 downto 0));

  ---------------------------------
  -- ROM interface 
  ---------------------------------
  rom1 : pr_rd_rom1 port map (
    clka  => axiclk,
    ena   => '1',
    addra => std_logic_vector(rom_addr),
    douta => rom1_dout);
  rom2 : pr_rd_rom2 port map (
    clka  => axiclk,
    ena   => '1',
    addra => std_logic_vector(rom_addr),
    douta => rom2_dout);

  rom_dout <= unsigned(rom1_dout) when rom_sel = '0' else unsigned(rom2_dout);

  ---------------------------------
  -- delay count so as not to hog
  -- the bus
  ---------------------------------
  process(axiclk)
  begin
    if rising_edge(axiclk) then
      if aresetn = '0' then
        delay_cntto <= to_unsigned(10000, 16);
      end if;
    end if;
  end process;

  ---------------------------------
  -- when the read ptr is at the end of the 1MB buffer, wrap it
  ---------------------------------
  process(axiclk)
  begin
    if rising_edge (axiclk) then
      max_rd_ptr <= base_address + (1024*1024) - 4096;
      if rd_ptr = max_rd_ptr then
        rd_ptr_wrap <= '1';
      else
        rd_ptr_wrap <= '0';
      end if;
    end if;
  end process;

  ---------------------------------
  -- store rom output for when
  -- rvalid drops
  ---------------------------------
  process(axiclk)
  begin
    if rising_edge(axiclk) then
      if M_AXI_RVALID = '0' and rvalid_1 = '1' then
        rom_dout_reg <= rom_dout;
      end if;
    end if;
  end process;

  ---------------------------------
  -- state machine
  ---------------------------------
  process(axiclk)
  begin
    if rising_edge (axiclk) then
      max_clkcnt_wait <= max_clkcnt_wait_int;
      continuity_err  <= '0';
      sel_err         <= '0';
      hdr_err         <= '0';
      num_lines_err   <= '0';
      err             <= '0';
      rvalid_1        <= M_AXI_RVALID;
      if aresetn = '0' or nrst = '0' then
        state               <= SRST;
        arvalid             <= '0';
        wvalid              <= '0';
        wlast               <= '0';
        rready              <= '0';
        rd_ptr              <= base_address;
        araddr              <= std_logic_vector(base_address);
        not_ready_cnt       <= X"0000";
        bready              <= '0';
        rom_sel             <= '0';
        rom_addr            <= to_unsigned(0, 8);
        wdata               <= std_logic_vector(to_unsigned(0, 128));
        clkcnt_wait         <= to_unsigned(0, 32);
        max_clkcnt_wait_int <= to_unsigned(0, 32);
      ---------------------------------
      -- SRST       -- reset
      ---------------------------------
      elsif state = SRST then
        state   <= SWAIT_HDR_ADDR;
        arlen   <= X"00";               -- single read
        arvalid <= '1';
        rready  <= '0';
        rom_sel <= '0';
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
          state    <= SWAIT_HDR_DATA;
          arvalid  <= '0';
          rready   <= '1';
          line_cnt <= X"00";
        end if;
      ---------------------------------
      -- SWAIT_HDR_DATA
      ---------------------------------
      elsif state = SWAIT_HDR_DATA then
        state       <= SWAIT_HDR_DATA;
        clkcnt_wait <= clkcnt_wait + 1;
        if M_AXI_RVALID = '1' then
          arvalid     <= '0';
          rready      <= '0';
          delay_cnt   <= to_unsigned(0, 16);
          line_cnt    <= X"00";
          hdr_cnt_reg <= hdr_cnt;
          if hdr_base(63 downto 56) /= X"00" then
            if hdr_base = ROM1_ID then
              if rom_sel = '1' then     -- wrong ID
                sel_err <= '1';
                rom_sel <= '0';
                if hdr_cnt /= frame1_cnt then
                  continuity_err <= '1';
                end if;
              else                      -- correct ID for this rom_sel
                frame_tx_cnt <= hdr_master_cnt;
                frame_rx_cnt <= master_ctr;
                if hdr_cnt /= frame1_cnt then
                  continuity_err <= '1';
                end if;
              end if;
              state       <= SREAD_ADDR;
              clkcnt_wait <= to_unsigned(0, 32);
              if clkcnt_wait > max_clkcnt_wait_int then
                max_clkcnt_wait_int <= clkcnt_wait;
              end if;
              arvalid <= '1';
              arlen   <= X"FF";
            elsif hdr_base = ROM2_ID then
              if rom_sel = '0' then     -- wrong ID
                sel_err <= '1';
                rom_sel <= '1';
                if hdr_cnt /= frame2_cnt then
                  continuity_err <= '1';
                end if;
              else                      -- correct ID for this rom_sel
                frame_tx_cnt <= hdr_master_cnt;
                frame_rx_cnt <= master_ctr;
                if hdr_cnt /= frame2_cnt then
                  continuity_err <= '1';
                end if;
              end if;
              state       <= SREAD_ADDR;
              clkcnt_wait <= to_unsigned(0, 32);
              if clkcnt_wait > max_clkcnt_wait_int then
                max_clkcnt_wait_int <= clkcnt_wait;
              end if;
              arvalid <= '1';
              arlen   <= X"FF";
            else                        -- invalid ID
              state   <= SCLEAR_PAGE_DATA;
              awlen   <= X"00";
              awvalid <= '1';
              wvalid  <= '0';
              hdr_err <= '1';
            end if;
          else                          -- invalid or no data
            state         <= SDELAY;
            not_ready_cnt <= not_ready_cnt + 1;
          end if;
        end if;
      ---------------------------------
      -- SREAD_ADDR
      ---------------------------------
      elsif state = SREAD_ADDR then
        state <= SREAD_ADDR;
        if arvalid = '1' and M_AXI_ARREADY = '1' then
          state    <= SREAD_DATA;
          arvalid  <= '0';
          rready   <= '1';
          line_cnt <= X"00";
          rom_addr <= to_unsigned(0, 8);
        end if;
      ----------------------------------
      --SREAD_DATA        -- reading
      ----------------------------------
      elsif state = SREAD_DATA then
        state <= SREAD_DATA;
        if M_AXI_RVALID = '1' then
          line_cnt <= line_cnt + 1;
          rom_addr <= rom_addr + 1;
          if line_cnt > 0 then
            if rvalid_1 = '1' then
              if unsigned(M_AXI_RDATA) /= rom_dout then
                err <= '1';
              end if;
            else
              if unsigned(M_AXI_RDATA) /= rom_dout_reg then
                err <= '1';
              end if;
            end if;
          end if;
          if M_AXI_RLAST = '1' then
            state  <= SREAD_DONE;
            rready <= '0';
          end if;
        end if;
      ----------------------------------
      -- SREAD_DONE
      ----------------------------------
      elsif state = SREAD_DONE then
        state   <= SCLEAR_PAGE_ADDR;
        awaddr  <= std_logic_vector(base_address);
        awlen   <= X"00";
        awvalid <= '1';
        wvalid  <= '0';
        rom_sel <= not rom_sel;
        if rom_sel = '0' then
          frame1_cnt <= hdr_cnt_reg + 1;
        else
          frame2_cnt <= hdr_cnt_reg + 1;
        end if;
        if line_cnt /= X"FF" then
          num_lines_err <= '1';
        end if;
        wdata <= std_logic_vector(to_unsigned(0, 128));
      ----------------------------------
      -- SCLEAR_PAGE_ADDR
      ----------------------------------
      elsif state = SCLEAR_PAGE_ADDR then
        state <= SCLEAR_PAGE_ADDR;
        if M_AXI_AWREADY = '1' then
          awvalid <= '0';
          wvalid  <= '1';
          wlast   <= '1';
          state   <= SCLEAR_PAGE_DATA;
        end if;
      ----------------------------------
      -- SCLEAR_PAGE_DATA
      ----------------------------------
      elsif state = SCLEAR_PAGE_DATA then
        state       <= SCLEAR_PAGE_DATA;
        clkcnt_wait <= clkcnt_wait + 1;
        if M_AXI_WREADY = '1' then
          state    <= SCLEAR_PAGE_DONE;
          rom_addr <= to_unsigned(0, 8);
          wvalid   <= '0';
          wlast    <= '0';
          bready   <= '1';
          araddr   <= std_logic_vector(base_address);
        end if;
      ----------------------------------
      -- SCLEAR_PAGE_DONE
      ----------------------------------
      elsif state = SCLEAR_PAGE_DONE then
        state       <= SCLEAR_PAGE_DONE;
        clkcnt_wait <= clkcnt_wait + 1;
        if M_AXI_BVALID = '1' then
          state    <= SWAIT_HDR_ADDR;
          rom_addr <= to_unsigned(0, 8);
          wvalid   <= '0';
          wlast    <= '0';
          arlen    <= X"00";
          bready   <= '0';
          arvalid  <= '1';
          rready   <= '0';
          araddr   <= std_logic_vector(base_address);
        end if;
      ---------------------------------
      -- SDELAY
      ---------------------------------
      elsif state = SDELAY then
        state       <= SDELAY;
        delay_cnt   <= delay_cnt + 1;
        clkcnt_wait <= clkcnt_wait + 1;
        if delay_cnt = delay_cntto then
          state   <= SWAIT_HDR_ADDR;
          arvalid <= '1';
          arlen   <= X"00";
        end if;
      end if;
    end if;
  end process;

  ---------------------------------
  -- read/write errors
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

  process(axiclk)
  begin
    if rising_edge(axiclk) then
      if aresetn = '0' or nrst = '0' then
        read_resp_error <= '0';
      elsif rready = '1' and M_AXI_RVALID = '1' and M_AXI_RRESP(1) = '1' then
        read_resp_error <= '1';
      end if;
    end if;
  end process;

  ----------------------------------
  -- stats
  ----------------------------------
  process(axiclk)
  begin
    if rising_edge(axiclk) then
      frame_cnt_diff_int <= frame_rx_cnt - frame_tx_cnt;
      frame_cnt_diff     <= frame_cnt_diff_int;
    end if;
  end process;

  process(axiclk)
  begin
    if rising_edge(axiclk) then
      if nrst = '0' or stats_clr = '1' then
        max_frame_cnt_diff_int <= to_unsigned(0, 32);
      elsif frame_cnt_diff_int > max_frame_cnt_diff_int then
        max_frame_cnt_diff_int <= frame_cnt_diff_int;
      end if;
      max_frame_cnt_diff <= max_frame_cnt_diff_int;
    end if;
  end process;


  ----------------------------------
  -- mappings
  ----------------------------------
  rom_addr_out     <= rom_addr;
  rom_dout_out     <= rom_dout;
  rom_dout_reg_out <= rom_dout_reg;
  line_cnt_out     <= line_cnt;
  rom_sel_out      <= rom_sel;
  state_out        <= state;

end arch;
