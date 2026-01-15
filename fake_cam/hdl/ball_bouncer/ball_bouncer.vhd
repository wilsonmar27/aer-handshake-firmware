LIBRARY IEEE;
USE IEEE.STD_LOGIC_1164.ALL;
USE IEEE.NUMERIC_STD.ALL;

ENTITY ball_bouncer IS
    GENERIC (
        ARENA_X : NATURAL;
        ARENA_Y : NATURAL;
        BALL_INIT_X : NATURAL;
        BALL_INIT_Y : NATURAL;
        BALL_SIZE : NATURAL
    );
    PORT (
        clk : IN STD_LOGIC;
        clk_en : IN STD_LOGIC;
        rst : IN STD_LOGIC;
        -- on every rising clock edge, move ball and output image
        arena : OUT STD_LOGIC_VECTOR(ARENA_X * ARENA_Y - 1 DOWNTO 0)
    );
END ball_bouncer;

ARCHITECTURE arch OF ball_bouncer IS
    -- ball top-left corner
    SIGNAL ball_x : INTEGER RANGE 0 TO ARENA_X - BALL_SIZE := BALL_INIT_X;
    SIGNAL ball_y : INTEGER RANGE 0 TO ARENA_Y - BALL_SIZE := BALL_INIT_Y;

    SIGNAL vel_x : INTEGER RANGE -1 TO 1 := 1;
    SIGNAL vel_y : INTEGER RANGE -1 TO 1 := 1;

    SIGNAL ball_x_next : INTEGER RANGE 0 TO ARENA_X - BALL_SIZE;
    SIGNAL ball_y_next : INTEGER RANGE 0 TO ARENA_Y - BALL_SIZE;
    SIGNAL vel_x_next : INTEGER RANGE -1 TO 1;
    SIGNAL vel_y_next : INTEGER RANGE -1 TO 1;
BEGIN
    -- combinatorial: bounce
    PROCESS (ball_x, ball_y, vel_x, vel_y)
        VARIABLE temp_x : INTEGER RANGE -1 TO 30;
        VARIABLE temp_y : INTEGER RANGE -1 TO 30;
        VARIABLE new_vel_x : INTEGER RANGE -1 TO 1;
        VARIABLE new_vel_y : INTEGER RANGE -1 TO 1;
    BEGIN
        -- if no bouncing
        temp_x := ball_x + vel_x;
        temp_y := ball_y + vel_y;
        new_vel_x := vel_x;
        new_vel_y := vel_y;

        -- if bumped into left or right
        IF temp_x < 0 THEN
            temp_x := 0;
            new_vel_x := - new_vel_x;
        ELSIF temp_x > ARENA_X - BALL_SIZE THEN
            temp_x := ARENA_X - BALL_SIZE;
            new_vel_x := - new_vel_x;
        END IF;

        -- if bumped into up or down
        IF temp_y < 0 THEN
            temp_y := 0;
            new_vel_y := - new_vel_y;
        ELSIF temp_y > ARENA_Y - BALL_SIZE THEN
            temp_y := ARENA_Y - BALL_SIZE;
            new_vel_y := - new_vel_y;
        END IF;

        ball_x_next <= temp_x;
        ball_y_next <= temp_y;
        vel_x_next <= new_vel_x;
        vel_y_next <= new_vel_y;
    END PROCESS;

    -- combinatorial: check if each pixel is part of the ball
    gen_y : FOR y IN 0 TO ARENA_Y - 1 GENERATE
        gen_x : FOR x IN 0 TO ARENA_X - 1 GENERATE
            PROCESS (ball_x, ball_y)
            BEGIN
                IF (x >= ball_x AND x < ball_x + BALL_SIZE AND
                    y >= ball_y AND y < ball_y + BALL_SIZE) THEN
                    arena(y * ARENA_X + x) <= '1';
                ELSE
                    arena(y * ARENA_X + x) <= '0';
                END IF;
            END PROCESS;
        END GENERATE gen_x;
    END GENERATE gen_y;

    -- sequential
    PROCESS (clk, clk_en, rst)
    BEGIN
        IF rst = '1' THEN
            ball_x <= BALL_INIT_X;
            ball_y <= BALL_INIT_Y;
            vel_x <= 1;
            vel_y <= 1;
        ELSIF rising_edge(clk) AND clk_en = '1' THEN
            ball_x <= ball_x_next;
            ball_y <= ball_y_next;
            vel_x <= vel_x_next;
            vel_y <= vel_y_next;
        END IF;
    END PROCESS;
END arch;