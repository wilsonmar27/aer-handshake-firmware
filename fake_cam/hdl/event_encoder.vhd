LIBRARY IEEE;
USE IEEE.STD_LOGIC_1164.ALL;
USE IEEE.NUMERIC_STD.ALL;

ENTITY event_encoder IS
    GENERIC (
        PX_IDX_LEN : NATURAL;
        ROW_LEN : NATURAL
    );
    PORT (
        row_active : IN STD_LOGIC;
        row_rdy : IN STD_LOGIC;
        event : IN STD_LOGIC_VECTOR (0 DOWNTO 0);
        px_idx : IN STD_LOGIC_VECTOR(PX_IDX_LEN - 1 DOWNTO 0);

        valid : OUT STD_LOGIC;
        word : OUT STD_LOGIC_VECTOR(5 DOWNTO 0)
    );
END ENTITY;

ARCHITECTURE arch OF event_encoder IS
    SIGNAL px_idx_nat : NATURAL RANGE 0 TO 62 * 62 - 1;
    SIGNAL row : NATURAL RANGE 0 TO 62;
    SIGNAL col : NATURAL RANGE 0 TO 62;
BEGIN
    px_idx_nat <= to_integer(unsigned(px_idx));
    row <= px_idx_nat / ROW_LEN;
    col <= px_idx_nat MOD ROW_LEN;

    valid <= row_rdy OR row_active OR event(0);
    word <= b"111111" WHEN row_rdy = '1' ELSE
        STD_LOGIC_VECTOR(to_unsigned(row, 6)) WHEN row_active = '1' ELSE
        STD_LOGIC_VECTOR(to_unsigned(col, 6)) WHEN event(0) = '1' ELSE
        b"000000";
END ARCHITECTURE;