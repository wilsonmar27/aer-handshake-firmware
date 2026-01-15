LIBRARY IEEE;
USE IEEE.STD_LOGIC_1164.ALL;
LIBRARY std;
USE std.env.ALL;

ENTITY event_gen_TB IS
END ENTITY;

ARCHITECTURE Behavioral OF event_gen_TB IS
    CONSTANT NUM_PIXELS : NATURAL := 9;
    CONSTANT PX_IDX_LEN : NATURAL := 4;
    CONSTANT clk_period : TIME := 1 ns;

    SIGNAL clk : STD_LOGIC := '0';
    SIGNAL rst : STD_LOGIC := '1';
    SIGNAL trg : STD_LOGIC := '0';
    SIGNAL pixels : STD_LOGIC_VECTOR(NUM_PIXELS - 1 DOWNTO 0) := (OTHERS => '0');
    SIGNAL px_idx : STD_LOGIC_VECTOR(PX_IDX_LEN - 1 DOWNTO 0);
    SIGNAL event : STD_LOGIC_VECTOR(0 DOWNTO 0);
    SIGNAL row_active : STD_LOGIC;
    SIGNAL row_rdy : STD_LOGIC;
    SIGNAL rdy : STD_LOGIC;
BEGIN
    uut : ENTITY work.event_gen
        GENERIC MAP(
            NUM_PIXELS => NUM_PIXELS,
            PX_IDX_LEN => PX_IDX_LEN,
            ROW_LEN => 3
        )
        PORT MAP(
            clk => clk,
            rst => rst,
            pixels => pixels,
            trg => trg,
            px_idx => px_idx,
            event => event,
            row_active => row_active,
            row_rdy => row_rdy,
            rdy => rdy
        );

    PROCESS
    BEGIN
        clk <= '0';
        WAIT FOR clk_period/2;
        clk <= '1';
        WAIT FOR clk_period/2;
    END PROCESS;

    PROCESS
    BEGIN
        rst <= '1';
        WAIT FOR clk_period * 2;
        rst <= '0';

        trg <= '1';
        pixels <= b"111000000";
        WAIT FOR clk_period;
        trg <= '0';
        WAIT UNTIL rdy = '1';
        WAIT FOR clk_period;

        trg <= '1';
        pixels <= b"001110000";
        WAIT FOR clk_period;
        trg <= '0';
        WAIT UNTIL rdy = '1';
        WAIT FOR clk_period;

        FINISH;
    END PROCESS;
END ARCHITECTURE;