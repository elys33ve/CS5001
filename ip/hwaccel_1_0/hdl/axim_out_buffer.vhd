library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity axim_out_buffer is
  port (
    clk : in std_logic;
    rst : in std_logic;

    wr_din : in unsigned(127 downto 0);
    wr_en  : in std_logic;

    rd_dout  : out unsigned(127 downto 0);
    rd_en    : in  std_logic;
    wr_ready : out std_logic;

    fifo_full : out std_logic;
    fifo_ready_out : out std_logic;

    dcount : out unsigned(10 downto 0));
end axim_out_buffer;

architecture arch of axim_out_buffer is

  component axim_out_fifo is
    port (
      clk         : in  std_logic;
      srst        : in  std_logic;
      din         : in  std_logic_vector(127 downto 0);
      wr_en       : in  std_logic;
      rd_en       : in  std_logic;
      dout        : out std_logic_vector(127 downto 0);
      full        : out std_logic;
      empty       : out std_logic;
      data_count  : out std_logic_vector(10 downto 0);
      wr_rst_busy : out std_logic;
      rd_rst_busy : out std_logic);
  end component axim_out_fifo;

  signal fifo_rst    : std_logic;
  signal fifo_wren   : std_logic;
  signal fifo_rden   : std_logic;
  signal din         : std_logic_vector(127 downto 0);
  signal empty, full : std_logic;
  signal wr_rst_busy : std_logic;
  signal rd_rst_busy : std_logic;
  signal fifo_ready  : std_logic;

  signal data_count : std_logic_vector(10 downto 0);
  signal dout_tmp   : std_logic_vector(127 downto 0);

begin

  fifo_rst <= rst;

  process(clk)
  begin
    if rising_edge(clk) then
      if rst = '1' then
        fifo_ready <= '0';
      elsif fifo_rst = '0' and wr_rst_busy = '0' and rd_rst_busy = '0' then
        fifo_ready <= '1';
      end if;
    end if;
  end process;

  fifo_u1 : axim_out_fifo port map (
    clk         => clk,
    srst        => fifo_rst,
    din         => din,
    wr_en       => fifo_wren,
    rd_en       => fifo_rden,
    dout        => dout_tmp,
    full        => full,
    empty       => empty,
    data_count  => data_count,
    wr_rst_busy => wr_rst_busy,
    rd_rst_busy => rd_rst_busy);

  dcount  <= unsigned(data_count);
  rd_dout <= unsigned(dout_tmp);
  din     <= std_logic_vector(wr_din);

  fifo_wren <= '1' when full = '0' and wr_en = '1' and fifo_ready = '1'  else '0';
  fifo_rden <= '1' when empty = '0' and rd_en = '1' and fifo_ready = '1' else '0';
  wr_ready  <= '1' when fifo_ready = '1' and full = '0'                  else '0';

  fifo_full <= full;
  fifo_ready_out <= fifo_ready;
  
end arch;
