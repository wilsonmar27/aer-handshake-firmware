LIBRARY IEEE;
USE IEEE.STD_LOGIC_1164.ALL;

ENTITY top_cmod_s7_25 IS
    PORT (
        sys_clk : IN STD_LOGIC;
        btn0 : IN STD_LOGIC;
        led0_r : OUT STD_LOGIC;

        data : OUT STD_LOGIC_VECTOR(11 DOWNTO 0);
        ack : IN STD_LOGIC
    );
END ENTITY;

ARCHITECTURE arch OF top_cmod_s7_25 IS
    COMPONENT cmt
        PORT (
            main_clk : OUT STD_LOGIC;
            aer_clk : OUT STD_LOGIC;
            sys_clk : IN STD_LOGIC
        );
    END COMPONENT;
    SIGNAL main_clk : STD_LOGIC;
    SIGNAL aer_clk : STD_LOGIC;

    ATTRIBUTE ASYNC_REG : STRING;
    SIGNAL rst_sync_reg : STD_LOGIC_VECTOR(2 DOWNTO 0) := (OTHERS => '1');
    SIGNAL ack_sync_reg : STD_LOGIC_VECTOR(2 DOWNTO 0) := (OTHERS => '0');
    ATTRIBUTE ASYNC_REG OF rst_sync_reg : SIGNAL IS "TRUE";
    ATTRIBUTE ASYNC_REG OF ack_sync_reg : SIGNAL IS "TRUE";
BEGIN
    cmt_inst : cmt
    PORT MAP(
        main_clk => main_clk,
        aer_clk => aer_clk,
        sys_clk => sys_clk
    );

    PROCESS (main_clk, btn0)
    BEGIN
        IF btn0 = '1' THEN
            rst_sync_reg <= (OTHERS => '1');
        ELSIF rising_edge(main_clk) THEN
            rst_sync_reg <= rst_sync_reg(1 DOWNTO 0) & '0';
        END IF;
    END PROCESS;

    PROCESS (aer_clk)
    BEGIN
        IF rising_edge(aer_clk) THEN
            ack_sync_reg <= ack_sync_reg(1 DOWNTO 0) & ack;
        END IF;
    END PROCESS;

    main : ENTITY work.main
        GENERIC MAP(
            BALL_BOUNCER_CLK_DIV => 12000000,
            IMG_X => 32,
            IMG_Y => 32,
            PX_IDX_LEN => 10,
            BALL_INIT_X => 10,
            BALL_INIT_Y => 0,
            BALL_SIZE => 3
        )
        PORT MAP(
            clk => main_clk,
            aer_clk => aer_clk,
            rst => rst_sync_reg(2),
            data => data,
            ack => ack_sync_reg(2),
            err => led0_r
        );
END ARCHITECTURE;