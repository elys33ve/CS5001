-- hdr
-- 127..120 : 0 or 0xEB (EB indicates available data)
-- 119..104 : frame_size_bytes
-- 103..64 : sequence number
-- 63..0 : 

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity hwaccel_axim_in is
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
    aresetn          : in  std_logic;
    nrst             : in  std_logic;
    base_address     : in  unsigned(31 downto 0);
    write_resp_error : out unsigned(1 downto 0);
    read_resp_error  : out unsigned(1 downto 0);
    shutdown : in std_logic;

    frame_size_lines : in unsigned(7 downto 0);
    delay_cntto          : in unsigned(15 downto 0);

    frame_read_done          : out std_logic;
    frame_read_start         : out std_logic;

    -- interface to acceleration core
    fifo_empty : out std_logic;
    fifo_dout  : out unsigned(127 downto 0);
    fifo_rd    : in  std_logic;
    
    -- test and verification
    state_out : out unsigned(3 downto 0);
    
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
end hwaccel_axim_in;

architecture arch of hwaccel_axim_in is

  -- 4kbytes
  component axim_in_fifo is
    port (
      clk         : in  std_logic;
      srst        : in  std_logic;
      din         : in  std_logic_vector(127 downto 0);
      wr_en       : in  std_logic;
      rd_en       : in  std_logic;
      dout        : out std_logic_vector(127 downto 0);
      full        : out std_logic;
      empty       : out std_logic;
      wr_rst_busy : out std_logic;
      rd_rst_busy : out std_logic);
  end component axim_in_fifo;

  signal awaddr  : std_logic_vector(C_M_AXI_ADDR_WIDTH-1 downto 0);
  signal awvalid : std_logic;
  signal wdata   : std_logic_vector(C_M_AXI_DATA_WIDTH-1 downto 0);
  signal rdata   : std_logic_vector(C_M_AXI_DATA_WIDTH-1 downto 0);
  signal wlast   : std_logic;
  signal wvalid  : std_logic;
  signal bready  : std_logic;
  signal araddr  : std_logic_vector(C_M_AXI_ADDR_WIDTH-1 downto 0);
  signal arvalid : std_logic;
  signal rready  : std_logic;
  signal arlen   : std_logic_vector(7 downto 0);
  signal awlen   : std_logic_vector(7 downto 0);

  -- memory interface
  signal rd_ptr      : unsigned(31 downto 0);
  signal hdr         : unsigned(127 downto 0);
  signal hdr_base    : unsigned(7 downto 0);
  signal checksum    : unsigned(111 downto 0);
  signal hdr_err_cnt : unsigned(15 downto 0);

  -- fifo interface
  signal line_cnt : unsigned(7 downto 0);

  signal fifo_rst      : std_logic;
  signal fifo_din      : std_logic_vector(127 downto 0);
  signal fifo_dout_int : std_logic_vector(127 downto 0);

  signal fifo_wr          : std_logic;
  signal fifo_full        : std_logic;
  signal fifo_empty_int   : std_logic;
  signal fifo_rd_rst_busy : std_logic;
  signal fifo_wr_rst_busy : std_logic;

  -- state machine
  signal state              : unsigned(3 downto 0);
  -- reset
  constant SRST             : unsigned(3 downto 0) := "0000";
  -- poll for available data
  constant SWAIT_HDR_ADDR   : unsigned(3 downto 0) := "0001";
  constant SWAIT_HDR_DATA   : unsigned(3 downto 0) := "0010";
  constant SWAIT_FOR_FIFO   : unsigned(3 downto 0) := "0011";
  constant SREAD_ADDR       : unsigned(3 downto 0) := "0100";
  constant SREAD_DATA       : unsigned(3 downto 0) := "0101";
  constant SREAD_DONE       : unsigned(3 downto 0) := "1000";
  constant SCLEAR_PAGE_ADDR : unsigned(3 downto 0) := "1001";
  constant SCLEAR_PAGE_DATA : unsigned(3 downto 0) := "1010";
  constant SCLEAR_PAGE_DONE : unsigned(3 downto 0) := "1011";
  constant SDELAY           : unsigned(3 downto 0) := "1111";

  signal delay_cnt       : unsigned(15 downto 0);
  signal delay_cntto_reg : unsigned(15 downto 0);

  signal frame_read_done_pending : std_logic;
  signal frame_read_done_int     : std_logic;

  -- stats
  signal frame_num : unsigned(31 downto 0);
  signal page_num  : unsigned(7 downto 0);

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

  process(axiclk)
  begin
    if rising_edge(axiclk) then
      delay_cntto_reg <= delay_cntto;
    end if;
  end process;

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
  fifo_din <= rdata;
  hdr      <= unsigned(rdata);
  hdr_base <= hdr(127 downto 120);

  --------------------------------------------------------------------
  -- state machine
  -- adding logic to delay frame read done until we know there there
  -- isn't another pending data frame after the first
  --------------------------------------------------------------------
  process(axiclk)
  begin
    if rising_edge (axiclk) then
      frame_read_done_int      <= '0';
      frame_read_start         <= '0';
      if aresetn = '0' or nrst = '0' then
        state                   <= SRST;
        arvalid                 <= '0';
        wvalid                  <= '0';
        awvalid                 <= '0';
        wlast                   <= '0';
        rready                  <= '0';
        rd_ptr                  <= base_address;
        hdr_err_cnt             <= X"0000";
        frame_num               <= (others => '0');
        fifo_rst                <= '1';
        bready                  <= '0';
        frame_read_done_pending <= '0';
        wdata                   <= std_logic_vector(to_unsigned(0, 128));
      ---------------------------------
      -- SRST       -- reset
      ---------------------------------
      elsif state = SRST then
        frame_read_done_pending <= '0';
        if shutdown = '1' then
          state <= SRST;
        else
          state     <= SWAIT_HDR_ADDR;
          rready    <= '0';
          fifo_rst  <= '0';
          rd_ptr    <= base_address;
          arvalid   <= '1';
          arlen     <= X"00";
          frame_num <= (others => '0');
          araddr    <= std_logic_vector(rd_ptr);
        end if;
      ---------------------------------
      -- SWAIT_HDR_ADDR
      ---------------------------------
      elsif state = SWAIT_HDR_ADDR then
        state <= SWAIT_HDR_ADDR;
        if M_AXI_ARREADY = '1' then
          arvalid  <= '0';
          rready   <= '1';
          line_cnt <= X"00";
          state    <= SWAIT_HDR_DATA;
        end if;
      ---------------------------------
      -- SWAIT_HDR_DATA
      ---------------------------------
      elsif state = SWAIT_HDR_DATA then
        state <= SWAIT_HDR_DATA;
        if M_AXI_RVALID = '1' and M_AXI_RLAST = '1' then
          arvalid   <= '0';
          rready    <= '0';
          delay_cnt <= to_unsigned(0, 16);
          line_cnt  <= X"00";
          if hdr_base = X"EB" then
            state            <= SWAIT_FOR_FIFO;
            araddr           <= std_logic_vector(rd_ptr);
            awaddr           <= std_logic_vector(rd_ptr);
            frame_read_start <= '1';
          else                          -- invalid or no data
            state <= SDELAY;
            if frame_read_done_pending = '1' then
              frame_read_done_pending  <= '0';
            end if;
            hdr_err_cnt <= hdr_err_cnt + 1;
          end if;
        end if;
      ---------------------------------
      -- SWAIT_FOR_FIFO
      ---------------------------------
      elsif state = SWAIT_FOR_FIFO then
        state <= SWAIT_FOR_FIFO;
        if fifo_empty_int = '1' then
          state   <= SREAD_ADDR;
          arvalid <= '1';
          arlen   <= std_logic_vector(frame_size_lines-1);
        end if;
      ---------------------------------
      -- SREAD_ADDR
      ---------------------------------
      elsif state = SREAD_ADDR then
        state <= SREAD_ADDR;
        if arvalid = '1' and M_AXI_ARREADY = '1' then
          arvalid  <= '0';
          rready   <= '1';
          line_cnt <= X"00";
          state    <= SREAD_DATA;
        end if;
      ----------------------------------
      --SREAD_DATA        -- reading
      ---------------------------------- 
      elsif state = SREAD_DATA then
        state <= SREAD_DATA;
        if M_AXI_RVALID = '1' then
          line_cnt <= line_cnt + 1;
          if M_AXI_RLAST = '1' then
            state                   <= SREAD_DONE;
            rready                  <= '0';
            frame_read_done_pending <= '1';
            frame_read_done_int     <= '1';
          end if;
        end if;
      ----------------------------------
      -- SREAD_DONE
      ----------------------------------
      elsif state = SREAD_DONE then
        state   <= SCLEAR_PAGE_ADDR;
        awlen   <= X"00";
        awvalid <= '1';
        wvalid  <= '1';
        wlast   <= '1';
        wdata   <= std_logic_vector(to_unsigned(0, 128));
      ----------------------------------
      -- SCLEAR_PAGE_ADDR
      ----------------------------------
      elsif state = SCLEAR_PAGE_ADDR then
        state <= SCLEAR_PAGE_ADDR;
        if M_AXI_AWREADY = '1' then
          if M_AXI_WREADY = '1' then
            state   <= SCLEAR_PAGE_DONE;
            bready  <= '1';
            awvalid <= '0';
            wvalid  <= '0';
            wlast   <= '0';
          else
            state   <= SCLEAR_PAGE_DATA;
            awvalid <= '0';
          end if;
        end if;
      ----------------------------------
      -- SCLEAR_PAGE_DATA
      ----------------------------------
      elsif state = SCLEAR_PAGE_DATA then
        state <= SCLEAR_PAGE_DATA;
        if M_AXI_WREADY = '1' then
          state  <= SCLEAR_PAGE_DONE;
          wvalid <= '0';
          wlast  <= '0';
          bready <= '1';
        end if;
      ----------------------------------
      -- SCLEAR_PAGE_DONE
      ----------------------------------
      elsif state = SCLEAR_PAGE_DONE then
        state <= SCLEAR_PAGE_DONE;
        if M_AXI_BVALID = '1' then
          bready <= '0';
          if shutdown = '1' then
            state <= SRST;
          else
            state     <= SWAIT_HDR_ADDR;
            arvalid   <= '1';
            arlen     <= X"00";
            frame_num <= frame_num + 1;
            rd_ptr    <= base_address + ((page_num+1) & X"000");
            araddr    <= std_logic_vector(base_address + ((page_num+1) & X"000"));
            arlen     <= X"00";
            arvalid   <= '1';
          end if;
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
      end if;
    end if;
  end process;

  page_num <= frame_num(7 downto 0);

  ----------------------------------
  -- axim fifo interface
  ----------------------------------
  axim_fifo0 : axim_in_fifo port map (
    clk         => axiclk,
    srst        => fifo_rst,
    din         => fifo_din,
    wr_en       => fifo_wr,
    rd_en       => fifo_rd,
    dout        => fifo_dout_int,
    full        => fifo_full,
    empty       => fifo_empty_int,
    wr_rst_busy => fifo_wr_rst_busy,
    rd_rst_busy => fifo_rd_rst_busy);

  fifo_wr <= '1' when fifo_full = '0' and rready = '1' and
             M_AXI_RVALID = '1' and state /= SWAIT_HDR_DATA else '0';
  fifo_empty <= '1' when fifo_empty_int = '1' or fifo_rd_rst_busy = '1' else '0';
  fifo_dout  <= unsigned(fifo_dout_int);

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

  ----------------------------------
  -- verification
  ----------------------------------
  state_out <= state;
  
end arch;
