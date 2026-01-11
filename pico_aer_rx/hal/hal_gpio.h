// pico/hal/hal_gpio.h
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Minimal GPIO HAL for DI-AER-style handshakes.
 *
 * This module is deliberately "dumb":
 *  - it knows pin numbers and electrical setup
 *  - it can read the DATA bus quickly as a packed word
 *  - it can drive ACK (when the CPU owns ACK; in PIO mode you may not use ack_write())
 *
 * It does NOT implement the handshake state machine (that lives in aer_rx_poll / PIO program).
 */
typedef struct hal_gpio_cfg_s {
    /** DATA bus pins are assumed contiguous: GPIO[data_base + i] for i in [0..data_width-1]. */
    uint8_t data_base;
    /** Number of DATA pins (e.g. 24 for 6 groups of 1-of-4). */
    uint8_t data_width;

    /** ACK pin GPIO number. */
    uint8_t ack_pin;

    /** If true, "ACK asserted" means drive pin high; else asserted means drive low. */
    bool    ack_active_high;

    /** Apply pull-downs to DATA pins (often useful so neutral reads as 0). */
    bool    data_pull_down;
    /** Apply pull-ups to DATA pins (usually false for this bus). */
    bool    data_pull_up;

    /** Initial "ACK deasserted" level at init time (usually false). */
    bool    ack_deasserted_level;
} hal_gpio_cfg_t;

/** Initialize DATA pins as inputs and ACK as output; sets ACK to cfg->ack_deasserted_level. */
void hal_gpio_init(const hal_gpio_cfg_t *cfg);

/** Put pins into a safe idle (DATA inputs, ACK deasserted). */
void hal_gpio_idle(void);

/**
 * Read all GPIO inputs as a single snapshot.
 * On RP2350 this includes GPIO 0-63 (lo+hi SIO input regs).
 */
uint64_t hal_gpio_read_all(void);

/** Read DATA bus as packed bits in LSBs: bit0 corresponds to GPIO cfg->data_base. */
uint32_t hal_gpio_read_data_raw(void);

/**
 * Drive ACK to the asserted/deasserted state.
 * NOTE: Use this only when CPU owns ACK (polling mode). In PIO mode, PIO drives ACK.
 */
void hal_gpio_ack_write(bool asserted);

/** Convenience wrappers. */
static inline void hal_gpio_ack_assert(void)   { hal_gpio_ack_write(true); }
static inline void hal_gpio_ack_deassert(void) { hal_gpio_ack_write(false); }

/** Returns true if ACK pin is currently driven to its asserted level. */
bool hal_gpio_ack_is_asserted(void);

/** Accessors (useful for PIO driver setup). */
uint8_t  hal_gpio_data_base(void);
uint8_t  hal_gpio_data_width(void);
uint8_t  hal_gpio_ack_pin(void);
uint64_t hal_gpio_data_mask64(void);

#ifdef __cplusplus
} // extern "C"
#endif
