LIBRARY IEEE;
USE IEEE.STD_LOGIC_1164.ALL;

ENTITY top_TB IS
END ENTITY;

ARCHITECTURE Behavioral OF top_TB IS
    CONSTANT clk_period : TIME := 10 ns;
    CONSTANT aer_clk_period : TIME := 0.333 ns;

    SIGNAL clk : STD_LOGIC;
    SIGNAL aer_clk : STD_LOGIC;
    SIGNAL rst : STD_LOGIC;
    SIGNAL data : STD_LOGIC_VECTOR(11 DOWNTO 0);
    SIGNAL ack : STD_LOGIC;
    SIGNAL err : STD_LOGIC;
BEGIN
    uut : ENTITY work.main
        GENERIC MAP(
            BALL_BOUNCER_CLK_DIV => 30,
            IMG_X => 3,
            IMG_Y => 3,
            PX_IDX_LEN => 4,
            BALL_INIT_X => 0,
            BALL_INIT_Y => 0,
            BALL_SIZE => 1
        )
        PORT MAP(
            clk => clk,
            aer_clk => aer_clk,
            rst => rst,
            data => data,
            ack => ack,
            err => err
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
        aer_clk <= '0';
        WAIT FOR aer_clk_period/2;
        aer_clk <= '1';
        WAIT FOR aer_clk_period/2;
    END PROCESS;

    PROCESS
    BEGIN
        ack <= '0';
        WAIT UNTIL data /= b"000000000000";
        WAIT FOR aer_clk_period * 10;
        ack <= '1';
        -- WAIT FOR aer_clk_period * 10;
        -- interestingly, a wait for before wait until will make the simulator miss the value update and stuck here 
        WAIT UNTIL data = b"000000000000";
        WAIT FOR aer_clk_period * 10;
    END PROCESS;

    PROCESS
    BEGIN
        rst <= '1';
        WAIT FOR clk_period * 2;
        rst <= '0';
        WAIT;
    END PROCESS;
END ARCHITECTURE;