LIBRARY IEEE;
USE IEEE.STD_LOGIC_1164.ALL;
USE IEEE.NUMERIC_STD.ALL;

ENTITY event_gen IS
    GENERIC (
        NUM_PIXELS : NATURAL;
        PX_IDX_LEN : NATURAL;
        ROW_LEN : NATURAL
    );
    PORT (
        clk : IN STD_LOGIC;
        rst : IN STD_LOGIC;

        pixels : IN STD_LOGIC_VECTOR(NUM_PIXELS - 1 DOWNTO 0);
        -- on trg, start to scan and generate events and update, pixel by pixel
        trg : IN STD_LOGIC;
        px_idx : OUT STD_LOGIC_VECTOR(PX_IDX_LEN - 1 DOWNTO 0);
        -- 1-bit, 1 for "changed", extend in the future
        event : OUT STD_LOGIC_VECTOR(0 DOWNTO 0);
        -- before every row, pause and drive this with whether any pixel in this row has events for 1 clock cycle
        row_active : OUT STD_LOGIC;
        -- for every ROW_LEN pixels, pause and drive this high for 1 clock cycle
        row_rdy : OUT STD_LOGIC;
        rdy : OUT STD_LOGIC
    );
END ENTITY;

ARCHITECTURE arch OF event_gen IS
    SIGNAL pixels_old : STD_LOGIC_VECTOR(NUM_PIXELS - 1 DOWNTO 0) := (OTHERS => '0');
    SIGNAL px_idx_reg : NATURAL RANGE 0 TO NUM_PIXELS - 1;
    SIGNAL px_idx_next : NATURAL RANGE 0 TO NUM_PIXELS - 1;

    TYPE s_type IS (IDLE, ROW_BEGIN, ROW_SCAN, ROW_SKIP, ROW_END);
    SIGNAL s : s_type := IDLE;
    SIGNAL s_next : s_type;
BEGIN
    -- overcome VHDL limitation of OUTs cannot be on the right
    px_idx <= STD_LOGIC_VECTOR(TO_UNSIGNED(px_idx_reg, PX_IDX_LEN));

    -- combinatorial
    PROCESS (s, pixels, pixels_old, trg, px_idx_reg) BEGIN
        px_idx_next <= 0;
        event <= b"0";
        row_active <= '0';
        row_rdy <= '0';
        rdy <= '0';
        s_next <= s;

        CASE s IS
            WHEN IDLE =>
                IF trg = '1' THEN
                    s_next <= ROW_BEGIN;
                END IF;
            WHEN ROW_BEGIN =>
                IF unsigned(
                    pixels_old(px_idx_reg + ROW_LEN - 1 DOWNTO px_idx_reg)
                    XOR
                    pixels(px_idx_reg + ROW_LEN - 1 DOWNTO px_idx_reg)
                    ) /= 0 THEN
                    -- if any pixel in this row has changed
                    row_active <= '1';
                    px_idx_next <= px_idx_reg;
                    s_next <= ROW_SCAN;
                ELSE
                    -- if not, skip this row, also skip signaling row ready
                    px_idx_next <= px_idx_reg + ROW_LEN - 1;
                    s_next <= ROW_SKIP;
                END IF;
            WHEN ROW_SCAN =>
                -- compare old and current pixels to create events
                event(0) <= pixels_old(px_idx_reg) XOR pixels(px_idx_reg);
                px_idx_next <= px_idx_reg + 1;
                IF (px_idx_reg + 1) MOD ROW_LEN = 0 THEN
                    px_idx_next <= px_idx_reg;
                    s_next <= ROW_END;
                END IF;
            WHEN ROW_SKIP | ROW_END =>
                IF s = ROW_END THEN
                    -- if row is not skipped
                    row_rdy <= '1';
                END IF;
                IF px_idx_reg = NUM_PIXELS - 1 THEN
                    -- if end of last row
                    px_idx_next <= 0;
                    rdy <= '1';
                    s_next <= IDLE;
                ELSE
                    px_idx_next <= px_idx_reg + 1;
                    s_next <= ROW_BEGIN;
                END IF;
        END CASE;
    END PROCESS;

    -- sequential
    PROCESS (clk, rst)
    BEGIN
        IF rst = '1' THEN
            px_idx_reg <= 0;
            pixels_old <= (OTHERS => '0');
            s <= IDLE;
        ELSIF rising_edge(clk) THEN
            -- update pixel that has been compared last cycle
            IF s = ROW_SCAN THEN
                pixels_old(px_idx_reg) <= pixels(px_idx_reg);
            END IF;
            px_idx_reg <= px_idx_next;
            s <= s_next;
        END IF;
    END PROCESS;
END ARCHITECTURE;