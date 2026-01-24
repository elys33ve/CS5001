library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity hwaccel_axim_out is
  generic (
    -- Thread ID Width
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
    delay_cntto          : in unsigned(15 downto 0);

    -- interface from acceleration core
    din      : in  unsigned(127 downto 0);
    wr       : in  std_logic;
    wr_ready : out std_logic;

    -- test and verification
    state_out : out unsigned(3 downto 0);
    dcount_out : out unsigned(10 downto 0);
    
    -- AW
    M_AXI_AWID    : out std_logic_vector(C_M_AXI_ID_WIDTH-1 downto 0);
    M_AXI_AWADDR  : out std_logic_vector(C_M_AXI_ADDR_WIDTH-1 downto 0);  -- len = burst length - 1
    M_AXI_AWLEN   : out std_logic_vector(7 downto 0);  -- size = log2(burst size in bytes)
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
end hwaccel_axim_out;

architecture arch of hwaccel_axim_out is

  component axim_out_buffer is
    port (
      clk : in std_logic;
      rst : in std_logic;

      wr_din : in unsigned(127 downto 0);
      wr_en  : in std_logic;

      rd_dout  : out unsigned(127 downto 0);
      rd_en    : in  std_logic;
      wr_ready : out std_logic;

      fifo_full      : out std_logic;
      fifo_ready_out : out std_logic;

      dcount : out unsigned(10 downto 0));
  end component axim_out_buffer;

  -- accounts for fifo output latency (2 cycles)
  -- dcount can be updated before the output is correct
  signal fifo_latency_check     : std_logic;
  signal fifo_latency_check_cnt : unsigned(1 downto 0);

  -- axim signals
  signal awaddr    : std_logic_vector(C_M_AXI_ADDR_WIDTH-1 downto 0);
  signal awvalid   : std_logic;
  signal wdata_tmp : std_logic_vector(C_M_AXI_DATA_WIDTH-1 downto 0);
  signal wdata     : std_logic_vector(C_M_AXI_DATA_WIDTH-1 downto 0);
  signal rdata     : std_logic_vector(C_M_AXI_DATA_WIDTH-1 downto 0);
  signal wlast     : std_logic;
  signal wvalid    : std_logic;
  signal bready    : std_logic;
  signal araddr    : std_logic_vector(C_M_AXI_ADDR_WIDTH-1 downto 0);
  signal arvalid   : std_logic;
  signal rready    : std_logic;
  signal arlen     : std_logic_vector(7 downto 0);
  signal awlen     : std_logic_vector(7 downto 0);
  signal bresp     : std_logic_vector(1 downto 0);

  -- ddr contains 256, 4kbyte pages
  signal page_num : unsigned(7 downto 0);
  signal wready_1 : std_logic;

  -- memory interface
  signal wr_ptr    : unsigned(31 downto 0);
  signal hdr       : unsigned(127 downto 0);
  signal hdr_base  : unsigned(7 downto 0);
  signal line_cnt  : unsigned(7 downto 0);
  signal write_hdr : unsigned(127 downto 0);

  -- state machine
  signal state                 : unsigned(3 downto 0);
  -- reset
  constant SRST                : unsigned(3 downto 0) := "0000";
  -- poll for available data
  constant SWAIT_FOR_DATA      : unsigned(3 downto 0) := "0001";
  constant SWAIT_HDR_ADDR      : unsigned(3 downto 0) := "0010";
  constant SWAIT_HDR_DATA      : unsigned(3 downto 0) := "0011";
  constant SWRITE_ADDR         : unsigned(3 downto 0) := "0100";
  constant SWRITE_DATA         : unsigned(3 downto 0) := "0101";
  constant SBREADY             : unsigned(3 downto 0) := "0110";
  constant SCLEAR_LINE         : unsigned(3 downto 0) := "0111";
  constant SDONE_ADDR          : unsigned(3 downto 0) := "1000";
  constant SDONE_DATA          : unsigned(3 downto 0) := "1001";
  constant SDONE_BREADY        : unsigned(3 downto 0) := "1010";
  constant SFIFO_LATENCY_CHECK : unsigned(3 downto 0) := "1110";
  constant SDELAY              : unsigned(3 downto 0) := "1111";

  signal delay_cnt       : unsigned(15 downto 0);
  signal delay_cntto_reg : unsigned(15 downto 0);

  -- write the INTERNAL SYNC last
  constant INTERNAL_SYNC : unsigned(7 downto 0) := X"EB";
  signal first_line      : unsigned(127 downto 0);
  signal sdone_wlast     : std_logic;

  signal invalid_hdr : std_logic;

  -- buffer interface
  signal rd_dout : unsigned(127 downto 0);
  signal rd_en   : std_logic;
  signal full    : std_logic;
  signal ready   : std_logic;
  signal dcount  : unsigned(10 downto 0);

  -- testing
  signal wlast_to_wready_cnt        : unsigned(7 downto 0);
  signal wlast_to_wready_trigger    : std_logic;
  signal wvalid_to_wready_cnt       : unsigned(7 downto 0);
  signal wvalid_to_wready_trigger   : std_logic;
  signal awvalid_to_awready_cnt     : unsigned(7 downto 0);
  signal awvalid_to_awready_trigger : std_logic;
  signal seq                        : unsigned(47 downto 0);
  signal seq_1                      : unsigned(47 downto 0);
  signal seq_check_en               : std_logic;
  signal seq_mismatch               : std_logic;

begin

  M_AXI_AWID    <= (others => '0');
  M_AXI_AWADDR  <= awaddr;
  M_AXI_AWLEN   <= awlen;
  M_AXI_AWSIZE  <= "100";               -- 16 bytes, hard coded
  M_AXI_AWBURST <= "01";
  M_AXI_AWLOCK  <= '0';
  M_AXI_AWCACHE <= "0010";              -- set for reserved mem, no cache
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

  M_AXI_ARADDR  <= araddr;
  M_AXI_ARID    <= (others => '0');
  M_AXI_ARVALID <= arvalid;
  M_AXI_RREADY  <= rready;
  M_AXI_ARUSER  <= (others => '1');
  M_AXI_ARQOS   <= x"0";
  M_AXI_ARCACHE <= "0010";
  M_AXI_ARPROT  <= "000";
  M_AXI_ARSIZE  <= "100";               -- 16 byte bus width
  M_AXI_ARBURST <= "01";                -- increment
  M_AXI_ARLOCK  <= '0';
  M_AXI_ARLEN   <= arlen;

  arlen <= X"00";  -- channel_in only reads one word to see if the page is available

  ---------------------------------
  -- for SDELAY, when no data is
  -- available to send
  ---------------------------------
  process(axiclk)
  begin
    if rising_edge(axiclk) then
      delay_cntto_reg <= delay_cntto;
    end if;
  end process;

  ---------------------------------
  -- reverse RDATA from userspace
  ---------------------------------
  rdata <= M_AXI_RDATA(7 downto 0) &
           M_AXI_RDATA(15 downto 8) &
           M_AXI_RDATA(23 downto 16) &
           M_AXI_RDATA(31 downto 24) &
           M_AXI_RDATA(39 downto 32) &
           M_AXI_RDATA(47 downto 40) &
           M_AXI_RDATA(55 downto 48) &
           M_AXI_RDATA(63 downto 56) &
           M_AXI_RDATA(71 downto 64) &
           M_AXI_RDATA(79 downto 72) &
           M_AXI_RDATA(87 downto 80) &
           M_AXI_RDATA(95 downto 88) &
           M_AXI_RDATA(103 downto 96) &
           M_AXI_RDATA(111 downto 104) &
           M_AXI_RDATA(119 downto 112) &
           M_AXI_RDATA(127 downto 120);
  hdr      <= unsigned(rdata);
  hdr_base <= hdr(127 downto 120);

  ---------------------------------
  -- state machine
  ---------------------------------
  process(axiclk)
  begin
    if rising_edge (axiclk) then
      wready_1    <= M_AXI_WREADY;
      sdone_wlast <= '0';
      invalid_hdr <= '0';
      ---------------------------------
      -- SRST       -- reset
      ---------------------------------
      if aresetn = '0' or nrst = '0' or state = SRST then
        state              <= SWAIT_FOR_DATA;
        arvalid            <= '0';
        awvalid            <= '0';
        wvalid             <= '0';
        rready             <= '0';
        wr_ptr             <= base_address;
        bready             <= '0';
        bresp              <= "00";
        fifo_latency_check <= '1';
        page_num           <= to_unsigned(0, 8);
      ---------------------------------
      -- SWAIT_FOR_DATA
      ---------------------------------
      elsif state = SWAIT_FOR_DATA then
        state    <= SWAIT_FOR_DATA;
        line_cnt <= X"00";
        if shutdown = '1' then
          state <= SRST;
        elsif dcount >= frame_size_lines then
          -- always make sure the fifo_dout is valid -- that
          -- the latency clocks have passed. this would
          -- only matter for small frame sizes (probably <= 48 bytes)
          if fifo_latency_check = '1' then
            state                  <= SFIFO_LATENCY_CHECK;
            fifo_latency_check_cnt <= "00";
          elsif rd_dout(127 downto 120) = X"EB" then
            state   <= SWAIT_HDR_ADDR;
            arvalid <= '1';
            araddr  <= std_logic_vector(wr_ptr);
            awaddr  <= std_logic_vector(wr_ptr);
          else
            state <= SCLEAR_LINE;
          end if;
        end if;
      ---------------------------------
      -- SWAIT_HDR_ADDR
      -- check that the page is clear
      ---------------------------------
      elsif state = SWAIT_HDR_ADDR then
        state     <= SWAIT_HDR_ADDR;
        delay_cnt <= to_unsigned(0, 16);
        if M_AXI_ARREADY = '1' then
          arvalid <= '0';
          rready  <= '1';
          state   <= SWAIT_HDR_DATA;
        end if;
      ---------------------------------
      -- SWAIT_HDR_DATA
      ---------------------------------
      elsif state = SWAIT_HDR_DATA then
        state <= SWAIT_HDR_DATA;
        if M_AXI_RVALID = '1' and M_AXI_RLAST = '1' then
          rready <= '0';
          if rdata(127 downto 120) = X"00" then     -- if not 00 or AA consider
                                                    -- clearing the page
            awlen      <= std_logic_vector(frame_size_lines(7 downto 0)-1);
            awvalid    <= '1';
            wvalid     <= '1';
            first_line <= rd_dout;
            state      <= SWRITE_ADDR;
          elsif rdata(127 downto 120) = X"AA" then  -- not available
            state <= SDELAY;
          else
            state       <= SDELAY;
            invalid_hdr <= '1';
          end if;
        end if;
      ---------------------------------
      -- SWRITE_ADDR
      ---------------------------------
      elsif state = SWRITE_ADDR then
        state <= SWRITE_ADDR;
        -- we know awvalid is 1 going in to this state
        if M_AXI_AWREADY = '1' then
          state   <= SWRITE_DATA;
          awvalid <= '0';
          if M_AXI_WREADY = '1' then
            line_cnt <= line_cnt + 1;
          end if;
        end if;
      ----------------------------------
      --SWRITE_DATA        -- writing
      ----------------------------------
      elsif state = SWRITE_DATA then
        state <= SWRITE_DATA;
        if M_AXI_WREADY = '1' then
          line_cnt <= line_cnt + 1;
          if wlast = '1' then
            wvalid <= '0';
            bready <= '1';
            state  <= SBREADY;
          end if;
        end if;
      ---------------------------------
      -- SBREADY
      ---------------------------------
      elsif state = SBREADY then
        state    <= SBREADY;
        line_cnt <= X"00";
        if M_AXI_BVALID = '1' then
          state   <= SDONE_ADDR;
          awvalid <= '1';
          awlen   <= X"00";
          bresp   <= M_AXI_BRESP;
          bready  <= '0';
        end if;
      ---------------------------------
      -- SDONE_ADDR
      ---------------------------------
      elsif state = SDONE_ADDR then
        state <= SDONE_ADDR;
        if M_AXI_AWREADY = '1' then
          state       <= SDONE_DATA;
          awvalid     <= '0';
          wvalid      <= '1';
          sdone_wlast <= '1';
        end if;
      ---------------------------------
      -- SDONE_DATA
      ---------------------------------
      elsif state = SDONE_DATA then
        state       <= SDONE_DATA;
        sdone_wlast <= '1';
        if M_AXI_WREADY = '1' then
          state  <= SDONE_BREADY;
          wvalid <= '0';
          bready <= '1';
        end if;
      ---------------------------------
      -- SDONE_BREADY
      ---------------------------------
      elsif state = SDONE_BREADY then
        state <= SDONE_BREADY;
        if M_AXI_BVALID = '1' then
          state              <= SWAIT_FOR_DATA;
          bresp              <= M_AXI_BRESP;
          bready             <= '0';
          wr_ptr             <= base_address + ((page_num+1) & X"000");
          page_num           <= page_num + 1;
          fifo_latency_check <= '1';
        end if;
      ---------------------------------
      -- SCLEAR_LINE
      ---------------------------------
      elsif state = SCLEAR_LINE then
        state              <= SWAIT_FOR_DATA;
        fifo_latency_check <= '1';
      -----------------------------------
      -- SFIFO_LATENCY_CHECK
      -- makes sure fifo_dout is valid
      -- (accounts for wr-to-rd latency)
      -----------------------------------
      elsif state = SFIFO_LATENCY_CHECK then
        state              <= SFIFO_LATENCY_CHECK;
        fifo_latency_check <= '0';
        if fifo_latency_check_cnt = "10" then
          state <= SWAIT_FOR_DATA;
        else
          fifo_latency_check_cnt <= fifo_latency_check_cnt + 1;
        end if;
      ---------------------------------
      -- SDELAY
      ---------------------------------
      elsif state = SDELAY then
        state     <= SDELAY;
        delay_cnt <= delay_cnt + 1;
        if delay_cnt = delay_cntto_reg then
          state   <= SWAIT_HDR_ADDR;
          arvalid <= '1';
        end if;
      ----------------------------------
      -- invalid
      ----------------------------------
      else
        state <= SRST;
      end if;
    end if;
  end process;

  ---------------------------------
  -- generate rd_en and wlast
  ---------------------------------
  process(M_AXI_WREADY, wvalid, state)
  begin
    if M_AXI_WREADY = '1' and wvalid = '1' then
      rd_en <= '1';
    elsif state = SCLEAR_LINE then
      rd_en <= '1';
    else
      rd_en <= '0';
    end if;
  end process;

  wlast <= '1' when (wvalid = '1' and line_cnt = frame_size_lines-1) or sdone_wlast = '1' else '0';

  ---------------------------------
  -- read/write errors
  ---------------------------------
  process(axiclk)
  begin
    if rising_edge(axiclk) then
      if aresetn = '0' or nrst = '0' then
        write_resp_error <= "00";
      elsif bready = '1' and M_AXI_BVALID = '1' and M_AXI_BRESP /= "00" then
        write_resp_error <= unsigned(M_AXI_BRESP);
      end if;
    end if;
  end process;

  process(axiclk)
  begin
    if rising_edge(axiclk) then
      if aresetn = '0' or nrst = '0' then
        read_resp_error <= "00";
      elsif rready = '1' and M_AXI_RVALID = '1' and M_AXI_RRESP /= "00" then
        read_resp_error <= unsigned(M_AXI_RRESP);
      end if;
    end if;
  end process;

  ---------------------------------
  -- reverse byte order for
  -- userspace
  ---------------------------------
  wdata_tmp <= std_logic_vector(rd_dout) when state /= SDONE_ADDR and state /= SDONE_DATA else
               std_logic_vector((INTERNAL_SYNC & first_line(119 downto 0)));
  wdata <= wdata_tmp(7 downto 0) &
           wdata_tmp(15 downto 8) &
           wdata_tmp(23 downto 16) &
           wdata_tmp(31 downto 24) &
           wdata_tmp(39 downto 32) &
           wdata_tmp(47 downto 40) &
           wdata_tmp(55 downto 48) &
           wdata_tmp(63 downto 56) &
           wdata_tmp(71 downto 64) &
           wdata_tmp(79 downto 72) &
           wdata_tmp(87 downto 80) &
           wdata_tmp(95 downto 88) &
           wdata_tmp(103 downto 96) &
           wdata_tmp(111 downto 104) &
           wdata_tmp(119 downto 112) &
           wdata_tmp(127 downto 120);

----------------------------------
-- verification
----------------------------------
  process(axiclk)
  begin
    if rising_edge(axiclk) then
      if state = SWRITE_ADDR then
        wlast_to_wready_cnt <= X"00";
      elsif state = SWRITE_DATA then
        if M_AXI_WREADY = '0' then
          if wlast = '0' then
            wlast_to_wready_cnt <= X"00";
          else
            if wlast_to_wready_cnt /= X"FF" then
              wlast_to_wready_cnt <= wlast_to_wready_cnt+1;
            end if;
          end if;
        end if;
      end if;
      if state = SWRITE_ADDR or state = SRST then
        wlast_to_wready_trigger <= '0';
      elsif state = SWRITE_DATA and wlast_to_wready_cnt > to_unsigned(100, 8) then
        wlast_to_wready_trigger <= '1';
      end if;
    end if;
  end process;

  process(axiclk)
  begin
    if rising_edge(axiclk) then
      if state = SWRITE_ADDR then
        wvalid_to_wready_cnt <= X"00";
      elsif state = SWRITE_DATA then
        if M_AXI_WREADY = '0' then
          if wvalid = '0' then
            wvalid_to_wready_cnt <= X"00";
          else
            if wvalid_to_wready_cnt /= X"FF" then
              wvalid_to_wready_cnt <= wvalid_to_wready_cnt+1;
            end if;
          end if;
        end if;
      end if;
      if state = SWRITE_ADDR or state = SRST then
        wvalid_to_wready_trigger <= '0';
      elsif state = SWRITE_DATA and wvalid_to_wready_cnt > to_unsigned(100, 8) then
        wvalid_to_wready_trigger <= '1';
      end if;
    end if;
  end process;

  process(axiclk)
  begin
    if rising_edge(axiclk) then
      if state = SWAIT_FOR_DATA then
        awvalid_to_awready_cnt <= X"00";
      elsif state = SWRITE_ADDR then
        if M_AXI_AWREADY = '0' then
          if awvalid = '0' then
            awvalid_to_awready_cnt <= X"00";
          else
            if awvalid_to_awready_cnt /= X"FF" then
              awvalid_to_awready_cnt <= awvalid_to_awready_cnt+1;
            end if;
          end if;
        end if;
      end if;
      if state = SWAIT_FOR_DATA or state = SRST then
        awvalid_to_awready_trigger <= '0';
      elsif state = SWRITE_ADDR and awvalid_to_awready_cnt > to_unsigned(100, 8) then
        awvalid_to_awready_trigger <= '1';
      end if;
    end if;
  end process;

  process(axiclk)
  begin
    if rising_edge(axiclk) then
      seq_check_en <= '0';
      seq_mismatch <= '0';
      if wvalid = '1' and line_cnt = to_unsigned(0, 8) and M_AXI_WREADY = '1' then
        if state = SWRITE_ADDR or state = SWRITE_DATA then
          seq          <= unsigned(wdata_tmp(119 downto 72));
          seq_1        <= seq;
          seq_check_en <= '1';
        end if;
      end if;
      if seq_check_en = '1' then
        if seq_1 + 1 /= seq then
          seq_mismatch <= '1';
        end if;
      end if;
    end if;
  end process;


  --------------------------------------------------------
  -- buffer
  --------------------------------------------------------
  axim_out_buffer0 : axim_out_buffer port map (
    clk => axiclk,
    rst => rst,

    wr_din => din,
    wr_en  => wr,

    rd_dout => rd_dout,
    rd_en   => rd_en,

    wr_ready => wr_ready,

    fifo_full      => full,
    fifo_ready_out => ready,

    dcount => dcount);

  ----------------------------------
  -- verification
  ----------------------------------
  state_out <= state;
  dcount_out <= dcount;
end arch;
