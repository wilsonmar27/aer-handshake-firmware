LIBRARY IEEE;
USE IEEE.STD_LOGIC_1164.ALL;

ENTITY main IS
    GENERIC (
        BALL_BOUNCER_CLK_DIV : NATURAL := 12000000;
        IMG_X : NATURAL := 3;
        IMG_Y : NATURAL := 3;
        PX_IDX_LEN : NATURAL := 4;
        BALL_INIT_X : NATURAL := 0;
        BALL_INIT_Y : NATURAL := 0;
        BALL_SIZE : NATURAL := 1
    );
    PORT (
        clk : IN STD_LOGIC;
        aer_clk : IN STD_LOGIC;
        rst : IN STD_LOGIC;

        data : OUT STD_LOGIC_VECTOR(11 DOWNTO 0);
        ack : IN STD_LOGIC;

        err : OUT STD_LOGIC
    );
END ENTITY;

ARCHITECTURE arch OF main IS
    COMPONENT fifo_words
        PORT (
            rst : IN STD_LOGIC;
            wr_clk : IN STD_LOGIC;
            rd_clk : IN STD_LOGIC;
            din : IN STD_LOGIC_VECTOR(5 DOWNTO 0);
            wr_en : IN STD_LOGIC;
            rd_en : IN STD_LOGIC;
            dout : OUT STD_LOGIC_VECTOR(5 DOWNTO 0);
            full : OUT STD_LOGIC;
            empty : OUT STD_LOGIC;
            wr_rst_busy : OUT STD_LOGIC;
            rd_rst_busy : OUT STD_LOGIC
        );
    END COMPONENT;

    SIGNAL fifo_rst : STD_LOGIC;
    SIGNAL main_rst : STD_LOGIC;
    SIGNAL rst_cnt : NATURAL RANGE 0 TO 35 := 0;

    SIGNAL pixels : STD_LOGIC_VECTOR(IMG_X * IMG_Y - 1 DOWNTO 0);

    SIGNAL ball_bouncer_clk_en : STD_LOGIC;

    SIGNAL event_gen_trg : STD_LOGIC;
    SIGNAL event_gen_px_idx : STD_LOGIC_VECTOR(PX_IDX_LEN - 1 DOWNTO 0);
    SIGNAL event_gen_event : STD_LOGIC_VECTOR(0 DOWNTO 0);
    SIGNAL event_gen_row_active : STD_LOGIC;
    SIGNAL event_gen_row_rdy : STD_LOGIC;
    SIGNAL event_gen_rdy : STD_LOGIC;

    SIGNAL event_encoder_valid : STD_LOGIC;

    SIGNAL fifo_full : STD_LOGIC;
    SIGNAL fifo_wen : STD_LOGIC;
    SIGNAL fifo_din : STD_LOGIC_VECTOR(5 DOWNTO 0);

    SIGNAL fifo_empty : STD_LOGIC;
    SIGNAL fifo_ren : STD_LOGIC;
    SIGNAL fifo_dout : STD_LOGIC_VECTOR(5 DOWNTO 0);

    SIGNAL clk_div_cnt : NATURAL RANGE 0 TO BALL_BOUNCER_CLK_DIV - 1 := 0;
    TYPE s_type IS (FIFO_RESET, FIFO_NO_ACCESS, EVENTS, WAIT_FOR_EVENTS, WAIT_FOR_FRAME, FRAME, ERROR_STATE);
    SIGNAL s : s_type := FIFO_RESET;
    SIGNAL s_next : s_type;
BEGIN
    ball_bouncer : ENTITY work.ball_bouncer
        GENERIC MAP(
            ARENA_X => IMG_X,
            ARENA_Y => IMG_Y,
            BALL_INIT_X => BALL_INIT_X,
            BALL_INIT_Y => BALL_INIT_Y,
            BALL_SIZE => BALL_SIZE
        )
        PORT MAP(
            clk => clk,
            clk_en => ball_bouncer_clk_en,
            rst => main_rst,
            arena => pixels
        );
    event_gen : ENTITY work.event_gen
        GENERIC MAP(
            NUM_PIXELS => IMG_X * IMG_Y,
            PX_IDX_LEN => PX_IDX_LEN,
            ROW_LEN => IMG_X
        )
        PORT MAP(
            clk => clk,
            rst => main_rst,
            pixels => pixels,
            trg => event_gen_trg,
            px_idx => event_gen_px_idx,
            event => event_gen_event,
            row_active => event_gen_row_active,
            row_rdy => event_gen_row_rdy,
            rdy => event_gen_rdy
        );
    event_encoder : ENTITY work.event_encoder
        GENERIC MAP(
            PX_IDX_LEN => PX_IDX_LEN,
            ROW_LEN => IMG_X
        )
        PORT MAP(
            row_active => event_gen_row_active,
            row_rdy => event_gen_row_rdy,
            event => event_gen_event,
            px_idx => event_gen_px_idx,
            valid => event_encoder_valid,
            word => fifo_din
        );
    fifo : fifo_words
    PORT MAP(
        rst => fifo_rst,
        wr_clk => clk,
        rd_clk => aer_clk,
        din => fifo_din,
        wr_en => fifo_wen,
        rd_en => fifo_ren,
        dout => fifo_dout,
        full => fifo_full,
        empty => fifo_empty,
        wr_rst_busy => OPEN,
        rd_rst_busy => OPEN
    );
    aer_consumer : ENTITY work.aer_consumer
        PORT MAP(
            clk => aer_clk,
            rst => main_rst,
            no_events => fifo_empty,
            ren => fifo_ren,
            event => fifo_dout,
            ack => ack,
            data => data
        );

    PROCESS (s, rst_cnt, clk_div_cnt, event_encoder_valid, fifo_full, event_gen_rdy)
    BEGIN
        fifo_rst <= '0';
        main_rst <= '0';

        ball_bouncer_clk_en <= '0';
        event_gen_trg <= '0';
        fifo_wen <= '0';
        s_next <= s;
        err <= '0';

        CASE s IS
            WHEN FIFO_RESET =>
                -- hold fifo reset for 5 cycles
                fifo_rst <= '1';
                main_rst <= '1';
                IF rst_cnt = 5 THEN
                    s_next <= FIFO_NO_ACCESS;
                END IF;
            WHEN FIFO_NO_ACCESS =>
                -- and then, hold the rest of the system in reset for 30 cycles
                main_rst <= '1';
                IF rst_cnt = 35 THEN
                    s_next <= EVENTS;
                END IF;
            WHEN EVENTS =>
                -- trigger event generator
                event_gen_trg <= '1';
                s_next <= WAIT_FOR_EVENTS;
            WHEN WAIT_FOR_EVENTS =>
                -- event generator loading new frame and scanning for diff
                IF event_encoder_valid = '1' THEN
                    -- there is an event
                    IF fifo_full = '1' THEN
                        -- fifo full, stay in error state
                        s_next <= ERROR_STATE;
                    ELSE
                        -- push one event to fifo
                        fifo_wen <= '1';
                    END IF;
                END IF;
                IF event_gen_rdy = '1' THEN
                    s_next <= WAIT_FOR_FRAME;
                END IF;
            WHEN WAIT_FOR_FRAME =>
                -- wait for counter to expire...
                IF clk_div_cnt = BALL_BOUNCER_CLK_DIV - 1 THEN
                    s_next <= FRAME;
                END IF;
            WHEN FRAME =>
                -- ...and then generate a new frame
                ball_bouncer_clk_en <= '1';
                s_next <= EVENTS;
            WHEN ERROR_STATE =>
                err <= '1';
        END CASE;
    END PROCESS;

    PROCESS (clk, rst)
    BEGIN
        IF rst = '1' THEN
            s <= FIFO_RESET;
            rst_cnt <= 0;
            clk_div_cnt <= 0;
        ELSIF rising_edge(clk) THEN
            IF s /= FIFO_RESET AND s /= FIFO_NO_ACCESS THEN
                -- always running counter for generating new frames at a steady rate
                IF clk_div_cnt = BALL_BOUNCER_CLK_DIV - 1 THEN
                    clk_div_cnt <= 0;
                ELSE
                    clk_div_cnt <= clk_div_cnt + 1;
                END IF;
            ELSE
                IF rst_cnt < 35 THEN
                    rst_cnt <= rst_cnt + 1;
                END IF;
            END IF;

            s <= s_next;
        END IF;
    END PROCESS;
END ARCHITECTURE;