LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.numeric_std.ALL;

ENTITY aer_consumer IS
    PORT (
        clk : IN STD_LOGIC;
        rst : IN STD_LOGIC;

        no_events : IN STD_LOGIC;
        ren : OUT STD_LOGIC;
        event : IN STD_LOGIC_VECTOR(5 DOWNTO 0);

        ack : IN STD_LOGIC;
        data : OUT STD_LOGIC_VECTOR(11 DOWNTO 0)
    );
END ENTITY;

ARCHITECTURE arch OF aer_consumer IS
    SIGNAL event_encoding : STD_LOGIC_VECTOR(11 DOWNTO 0);

    TYPE s_type IS (IDLE, WAIT_FOR_FIFO_READ, WAIT_FOR_FIFO_OUTPUT, WAIT_FOR_ACK, WAIT_FOR_NACK);
    SIGNAL s : s_type := IDLE;
    SIGNAL s_next : s_type;
BEGIN
    PROCESS (event)
        VARIABLE tmp : STD_LOGIC_VECTOR(11 DOWNTO 0);
        VARIABLE sym_0 : NATURAL RANGE 0 TO 3;
        VARIABLE sym_1 : NATURAL RANGE 0 TO 3;
        VARIABLE sym_2 : NATURAL RANGE 0 TO 3;
    BEGIN
        tmp := (OTHERS => '0');
        sym_0 := to_integer(unsigned(event(1 DOWNTO 0)));
        sym_1 := to_integer(unsigned(event(3 DOWNTO 2)));
        sym_2 := to_integer(unsigned(event(5 DOWNTO 4)));

        tmp(sym_0) := '1';
        tmp(sym_1 + 4) := '1';
        tmp(sym_2 + 8) := '1';

        event_encoding <= tmp;
    END PROCESS;

    PROCESS (s, no_events, ack, event_encoding)
    BEGIN
        ren <= '0';
        data <= (OTHERS => '0');
        s_next <= s;

        CASE s IS
            WHEN IDLE =>
                IF no_events = '0' THEN
                    ren <= '1';
                    s_next <= WAIT_FOR_FIFO_READ;
                END IF;
            WHEN WAIT_FOR_FIFO_READ =>
                -- FIFO read data is ready later in the next cycle w.r.t. ren, thus wait for an extra cycle
                s_next <= WAIT_FOR_FIFO_OUTPUT;
            WHEN WAIT_FOR_FIFO_OUTPUT =>
                -- FIFO embedded output register adds another cycle of delay
                s_next <= WAIT_FOR_ACK;
            WHEN WAIT_FOR_ACK =>
                data <= event_encoding;
                IF ack = '1' THEN
                    s_next <= WAIT_FOR_NACK;
                END IF;
            WHEN WAIT_FOR_NACK =>
                IF ack = '0' THEN
                    s_next <= IDLE;
                END IF;
        END CASE;
    END PROCESS;

    PROCESS (clk, rst)
    BEGIN
        IF rst = '1' THEN
        ELSIF rising_edge(clk) THEN
            s <= s_next;
        END IF;
    END PROCESS;
END ARCHITECTURE;