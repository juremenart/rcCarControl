library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity sync_fifo is
  generic(
    DATA_WIDTH    : positive := 8;
    POINTER_WIDTH : positive := 4       -- actual size is 2**POINTER_WIDTH
    );
  port(
    TestEnablexSI         : in  std_logic;
    WrRSTxRBI             : in  std_logic;
    RdRSTxRBI             : in  std_logic;
    WrCLKxCI              : in  std_logic;
    RdCLKxCI              : in  std_logic;
    FlushxSI              : in  std_logic;
    DataxDI               : in  std_logic_vector(DATA_WIDTH-1 downto 0);
    WritexEI              : in  std_logic;
    ReadxEI               : in  std_logic;
    EmptyRdclkxSO         : out std_logic;
    EmptyWrclkxSO         : out std_logic;
    FullRdclkxSO          : out std_logic;
    FullWrclkxSO          : out std_logic;
    HalfEmptyRdclkxSO     : out std_logic;
    HalfEmptyWrclkxSO     : out std_logic;
    HalfFullRdclkxSO      : out std_logic;
    HalfFullWrclkxSO      : out std_logic;
    WordCountRdclkxSO     : out std_logic_vector(POINTER_WIDTH downto 0);
    WordCountWrclkxSO     : out std_logic_vector(POINTER_WIDTH downto 0);
    InFlushStatusRdclkxSO : out std_logic;
    InFlushStatusWrclkxSO : out std_logic;
    DataxDO               : out std_logic_vector(DATA_WIDTH-1 downto 0)
    );
end sync_fifo;

architecture rtl of sync_fifo is

  component cnt_nbit_gray
    generic (
      UPDOWN : integer;
      WIDTH  : integer);
    port (
      clk      : in  std_logic;
      nrst     : in  std_logic;
      reset    : in  std_logic;
      enable   : in  std_logic;
      preload  : in  std_logic;
      testmode : in  std_logic;
      outben   : in  std_logic;
      input_b  : in  std_logic_vector(WIDTH-1 downto 0);
      output_b : out std_logic_vector(WIDTH-1 downto 0);
      output_g : out std_logic_vector(WIDTH-1 downto 0));
  end component;

  function gray2bin(qgray : std_logic_vector) return std_logic_vector is

    variable qbin : std_logic_vector(qgray'range);
  begin
    qbin(qgray'length-1) := qgray(qgray'length-1);
    for i in qgray'length-1 downto qgray'length*3/4+1 loop
      qbin(i-1) := qgray(i-1) xor qbin(i);
    end loop;
    qbin(qgray'length*3/4-1) := qgray(qgray'length*3/4-1) xor qbin(qgray'length*3/4);
    for i in qgray'length*3/4-1 downto qgray'length*2/4+1 loop
      qbin(i-1) := qgray(i-1) xor qbin(i);
    end loop;
    if qgray'length >= 2 then
      qbin(qgray'length*2/4-1) := qgray(qgray'length*2/4-1) xor qbin(qgray'length*2/4);
      for i in qgray'length*2/4-1 downto qgray'length*1/4+1 loop
        qbin(i-1) := qgray(i-1) xor qbin(i);
      end loop;
    end if;  -- length >= 2
    if qgray'length >= 4 then
      qbin(qgray'length*1/4-1) := qgray(qgray'length*1/4-1) xor qbin(qgray'length*1/4);
      for i in qgray'length*1/4-1 downto 1 loop
        qbin(i-1) := qgray(i-1) xor qbin(i);
      end loop;
    end if;  -- length >= 4

    return qbin;
  end;

  type fifoType is array (natural range 0 to 2**(POINTER_WIDTH)-1) of std_logic_vector(DATA_WIDTH-1 downto 0);

  signal WritePtrxS      : std_logic_vector(POINTER_WIDTH downto 0);
  signal WritePtrBinxS   : std_logic_vector(POINTER_WIDTH downto 0);
  signal WritePtrRdclkxS : std_logic_vector(POINTER_WIDTH downto 0);  -- this is a register. TODO: change name (...xSP)
  signal ReadPtrxS       : std_logic_vector(POINTER_WIDTH downto 0);
  signal ReadPtrBinxS    : std_logic_vector(POINTER_WIDTH downto 0);
  signal ReadPtrWrclkxS  : std_logic_vector(POINTER_WIDTH downto 0);  -- this is a register. TODO: change name (...xSP)
  signal FifoxD          : fifoType;

  signal WordCountRdclkxS : std_logic_vector(POINTER_WIDTH downto 0) := (others => '0');
  signal WordCountWrclkxS : std_logic_vector(POINTER_WIDTH downto 0) := (others => '0');

  signal FlushRdclkxRB_i, FlushWrclkxRB_i   : std_logic;
  signal WriteCntEnxS, ReadCntEnxS          : std_logic;
  signal FullWrclkxS, EmptyRdclkxS          : std_logic;

  signal FlushWrclkRelxRB                   : std_logic;
  signal FlushWrclkRelxRB_s1                : std_logic;
  signal FlushWrclkRelxRB_s2                : std_logic;
  signal FlushRdclkRelxRB                   : std_logic;
  signal FlushRdclkRelxRB_s1                : std_logic;
  signal FlushRdclkRelxRB_s2                : std_logic;

  signal FlushWrClkxS, FlushWrClkxSB : std_logic;
  signal FlushRdClkxS, FlushRdClkxSB : std_logic;
  signal FlushxSB                    : std_logic;
  signal CombFlush_Rd_xSB            : std_logic;
  signal CombFlush_Wr_xSB            : std_logic;

  signal FlushRdclkxRB                      : std_logic;
  signal FlushRdclkWrClkxRB_s1              : std_logic;
  signal FlushRdclkWrClkxRB_s2              : std_logic;

  signal FlushWrclkxRB                      : std_logic;
  signal FlushWrclkRdClkxRB_s1              : std_logic;
  signal FlushWrclkRdClkxRB_s2              : std_logic;

  signal zero_1, one_1 : std_logic;
  signal zeros         : std_logic_vector(POINTER_WIDTH downto 0);

begin

  zero_1 <= '0';
  one_1  <= '1';
  zeros  <= (others => '0');

------------------------------------------------------------------------------------------
-- Flush Synchronisation to both the Read and Write domain. The flush is applied 
-- asynchronously in both domains and release synchronously to the respective
-- domain. We will use one of the reset synchronisers to achieve this.
------------------------------------------------------------------------------------------

  --FlushxSB <= not FlushxSI when TestEnablexSI = '0' else WrRSTxRBI;  -- WrRSTxRBI=test_reset in test_mode;
  CombFlush_Rd_xSB <= (not FlushxSI) and RdRSTxRBI when TestEnablexSI = '0' else RdRSTxRBI;
  CombFlush_Wr_xSB <= (not FlushxSI) and WrRSTxRBI when TestEnablexSI = '0' else WrRSTxRBI;
  
  i_sync_rd_flush : reset_sync
    port map(
      rst_an_i        => CombFlush_Rd_xSB,   
      clk             => RdCLKxCI,
      rst_sync_bypass => TestEnablexSI,
      rst_n_o         => FlushRdClkxSB
      );

  FlushRdClkxS <= not FlushRdClkxSB;

  i_sync_wr_flush : reset_sync
    port map(
      rst_an_i        => CombFlush_Wr_xSB, 
      clk             => WrCLKxCI,
      rst_sync_bypass => TestEnablexSI,
      rst_n_o         => FlushWrClkxSB
      );                                         

  FlushWrClkxS <= not FlushWrClkxSB;

------------------------------------------------------------------------------------------
-- WrCLK WordCount/Flags/WrPointer    
--
-- The InFlushStatusWrClkxS indicates when both the read and write sides have finished
-- ( synced to the write side ).
--
------------------------------------------------------------------------------------------

  WriteCntEnxS <= WritexEI and not FullWrclkxS;

  WrPointerGrey : cnt_nbit_gray
    generic map (
      UPDOWN => 0,                      -- 0=UP ; 1=DOWN
      WIDTH  => POINTER_WIDTH+1)
    port map(
      clk      => WrCLKxCI,             -- in  std_logic; 
      nrst     => FlushWrclkxRB_i,      -- in  std_logic; 
      reset    => zero_1,               -- in  std_logic;
      enable   => WriteCntEnxS,         -- in  std_logic;
      preload  => zero_1,               -- in  std_logic;
      testmode => zero_1,               -- in  std_logic;
      outben   => one_1,                -- in  std_logic;
      input_b  => zeros,                -- in  std_logic_vector(WIDTH-1 downto 0);
      output_b => WritePtrBinxS,        -- out std_logic_vector(WIDTH-1 downto 0);
      output_g => WritePtrxS            -- out std_logic_vector(WIDTH-1 downto 0)
      );

  --hclkSync : process(FlushWrClkxS, TestEnablexSI, WrCLKxCI, WrRSTxRBI)
  hclkSync : process(FlushWrClkxS, WrCLKxCI)
  begin
    --if WrRSTxRBI = '0' or (FlushWrClkxS = '1' and TestEnablexSI = '0') then
    if FlushWrClkxS = '1' then
      InFlushStatusWrClkxSO <= '1';
      FlushWrclkxRB         <= '0';
      FlushWrclkRelxRB      <= '0';
      FlushRdclkRelxRB_s1   <= '0';
      FlushRdclkRelxRB_s2   <= '0';
      ReadPtrWrclkxS        <= (others => '0');
      FlushRdclkWrClkxRB_s1 <= '0';
      FlushRdclkWrClkxRB_s2 <= '0';
    elsif (WrCLKxCI'event and WrCLKxCI = '1') then
      if( FlushRdclkWrClkxRB_s2  = '1' ) then
        InFlushStatusWrClkxSO <= '0';
      end if;
      FlushWrclkRelxRB      <= '1';
      FlushRdclkRelxRB_s1   <= FlushRdclkRelxRB;
      FlushRdclkRelxRB_s2   <= FlushRdclkRelxRB_s1;
      FlushRdclkWrClkxRB_s1 <= FlushRdClkxRB;
      FlushRdclkWrClkxRB_s2 <= FlushRdclkWrClkxRB_s1;
      if FlushRdclkRelxRB_s2 = '1' then
        FlushWrclkxRB <= '1';
      end if;
      ReadPtrWrclkxS <= ReadPtrxS;
    end if;
  end process;

  FlushWrclkxRB_i <= FlushWrclkxRB when (TestEnablexSI = '0') else WrRSTxRBI;  -- WrRSTxRBI=test_reset in test_mode

  WordCountWrclkxS <= std_logic_vector(unsigned(WritePtrBinxS) - unsigned(gray2bin(ReadPtrWrclkxS))) when (FlushWrclkxRB = '1' and FlushRdclkWrClkxRB_s2 = '1') else
                      WritePtrBinxS;

  WordCountWrclkxSO <= std_logic_vector((2**POINTER_WIDTH) - unsigned(WordCountWrclkxS));
  EmptyWrclkxSO     <= '1' when (unsigned(WordCountWrclkxS) = 0)                     else '0';
  FullWrclkxS       <= '1' when (unsigned(WordCountWrclkxS) = 2**POINTER_WIDTH)      else '0';
  FullWrclkxSO      <= FullWrclkxS;
  HalfEmptyWrclkxSO <= '1' when (unsigned(WordCountWrclkxS) <= 2**(POINTER_WIDTH-1)) else '0';
  HalfFullWrclkxSO  <= '1' when (unsigned(WordCountWrclkxS) >= 2**(POINTER_WIDTH-1)) else '0';

------------------------------------------------------------------------------------------
-- RdCLK WordCount/Flags/RdPointer
--
-- The InFlushStatusRdClkxS indicates when both the read and write sides have finished
-- ( synced to the read side ).
--
------------------------------------------------------------------------------------------

  ReadCntEnxS <= ReadxEI and not EmptyRdclkxS;

  RdPointerGrey : cnt_nbit_gray
    generic map (
      UPDOWN => 0,                      -- 0=UP ; 1=DOWN
      WIDTH  => POINTER_WIDTH+1)
    port map(
      clk      => RdCLKxCI,             -- in  std_logic; 
      nrst     => FlushRdclkxRB_i,      -- in  std_logic; 
      reset    => zero_1,               -- in  std_logic;
      enable   => ReadCntEnxS,          -- in  std_logic;
      preload  => zero_1,               -- in  std_logic;
      testmode => zero_1,               -- in  std_logic;
      outben   => one_1,                -- in  std_logic;
      input_b  => zeros,                -- in  std_logic_vector(WIDTH-1 downto 0);
      output_b => ReadPtrBinxS,         -- out std_logic_vector(WIDTH-1 downto 0);
      output_g => ReadPtrxS             -- out std_logic_vector(WIDTH-1 downto 0)
      );

  --pclkSync : process(FlushRdClkxS, RdCLKxCI, RdRSTxRBI, TestEnablexSI)
  pclkSync : process(FlushRdClkxS, RdCLKxCI)
  begin
    --if RdRSTxRBI = '0' or (FlushRdClkxS = '1' and TestEnablexSI = '0') then
    if FlushRdClkxS = '1' then
      InFlushStatusRdclkxSO <= '1';
      FlushRdclkxRB         <= '0';
      FlushRdclkRelxRB      <= '0';
      FlushWrclkRelxRB_s1   <= '0';
      FlushWrclkRelxRB_s2   <= '0';
      WritePtrRdclkxS       <= (others => '0');
      FlushWrclkRdClkxRB_s1 <= '0';
      FlushWrclkRdClkxRB_s2 <= '0';
    elsif (RdCLKxCI'event and RdCLKxCI = '1') then
      if( FlushWrclkRdClkxRB_s2 = '1' ) then
        InFlushStatusRdclkxSO <= '0';
      end if;
      FlushRdclkRelxRB    <= '1';
      FlushWrclkRelxRB_s1 <= FlushWrclkRelxRB;
      FlushWrclkRelxRB_s2 <= FlushWrclkRelxRB_s1;
      FlushWrclkRdClkxRB_s1 <= FlushWrclkxRB;
      FlushWrclkRdClkxRB_s2 <= FlushWrclkRdClkxRB_s1;
      if FlushWrclkRelxRB_s2 = '1' then
        FlushRdclkxRB <= '1';
      end if;
      WritePtrRdclkxS <= WritePtrxS;
    end if;
  end process;

  FlushRdclkxRB_i <= FlushRdclkxRB when (TestEnablexSI = '0') else RdRSTxRBI;  -- RdRSTxRBI=test_reset in test_mode

  WordCountRdclkxS <= std_logic_vector(unsigned(gray2bin(WritePtrRdclkxS)) - unsigned(ReadPtrBinxS)) when (FlushWrclkRdClkxRB_s2 = '1' and FlushRdclkxRB = '1') else
                      (others => '0');

  WordCountRdclkxSO <= WordCountRdclkxS;
  EmptyRdclkxS      <= '1' when (unsigned(WordCountRdclkxS) = 0)                     else '0';
  EmptyRdclkxSO     <= EmptyRdclkxS;
  FullRdclkxSO      <= '1' when (unsigned(WordCountRdclkxS) = 2**POINTER_WIDTH)      else '0';
  HalfEmptyRdclkxSO <= '1' when (unsigned(WordCountRdclkxS) <= 2**(POINTER_WIDTH-1)) else '0';
  HalfFullRdclkxSO  <= '1' when (unsigned(WordCountRdclkxS) >= 2**(POINTER_WIDTH-1)) else '0';

------------------------------------------------------------------------------------------
-- Fifo write / read
------------------------------------------------------------------------------------------

  writeReg : process(WrCLKxCI, WrRSTxRBI)
  begin
    if WrRSTxRBI = '0' then
      for i in FifoxD'range loop
        FifoxD(i) <= (others => '0');
      end loop;
    elsif (WrCLKxCI'event and WrCLKxCI = '1') then
      if WriteCntEnxS = '1' then
        FifoxD(to_integer(unsigned(WritePtrBinxS(POINTER_WIDTH-1 downto 0)))) <= DataxDI;
      end if;
    end if;
  end process;

  DataxDO <= FifoxD(to_integer(unsigned(ReadPtrBinxS(POINTER_WIDTH-1 downto 0))));

end rtl;
