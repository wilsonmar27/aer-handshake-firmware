// pico/hal/hal_gpio.c
#include "hal_gpio.h"

#include "hardware/gpio.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#include "pico/platform.h"

#include <string.h>

static hal_gpio_cfg_t g_cfg;
static bool           g_inited = false;

static uint64_t g_data_mask64 = 0;
static uint64_t g_ack_mask64  = 0;
static uint8_t  g_data_shift  = 0;

static inline void sio_write_pin(uint8_t pin, bool level) {
    if (pin < 32u) {
        const uint32_t bit = 1u << pin;
        if (level) sio_hw->gpio_set = bit;
        else       sio_hw->gpio_clr = bit;
    } else {
#if defined(PICO_RP2350)
        const uint32_t bit = 1u << (pin - 32u);
        if (level) sio_hw->gpio_hi_set = bit;
        else       sio_hw->gpio_hi_clr = bit;
#else
        // RP2040 doesn't have >32 GPIOs; ignore gracefully.
        (void)level;
#endif
    }
}

static inline bool sio_read_pin(uint8_t pin) {
    const uint64_t all = (uint64_t)sio_hw->gpio_in
#if defined(PICO_RP2350)
        | (((uint64_t)sio_hw->gpio_hi_in) << 32u)
#endif
        ;
    return (all >> pin) & 1u;
}

static inline void compute_masks_from_cfg(void) {
    g_data_shift = g_cfg.data_base;

    // Guard: we pack into uint32_t in hal_gpio_read_data_raw().
    // Most AER/DI buses here are <= 32 bits (e.g., 24).
    if (g_cfg.data_width == 0u || g_cfg.data_width > 32u) {
        // Hard fail: configuration bug.
        // Using a spin to avoid pulling in stdio.
        while (1) { tight_loop_contents(); }
    }

    g_data_mask64 = ((g_cfg.data_width == 64u) ? ~0ULL : ((1ULL << g_cfg.data_width) - 1ULL)) << g_cfg.data_base;
    g_ack_mask64  = 1ULL << g_cfg.ack_pin;
}

static inline bool ack_level_for_asserted(bool asserted) {
    // If active-high: asserted => 1, deasserted => 0
    // If active-low : asserted => 0, deasserted => 1
    return g_cfg.ack_active_high ? asserted : !asserted;
}

void hal_gpio_init(const hal_gpio_cfg_t *cfg) {
    if (!cfg) {
        while (1) { tight_loop_contents(); }
    }

    // Copy config.
    memset(&g_cfg, 0, sizeof(g_cfg));
    g_cfg = *cfg;

    compute_masks_from_cfg();

    // Init DATA pins as GPIO inputs.
    for (uint8_t i = 0; i < g_cfg.data_width; ++i) {
        const uint8_t pin = (uint8_t)(g_cfg.data_base + i);
        gpio_init(pin);
        gpio_set_function(pin, GPIO_FUNC_SIO);
        gpio_set_dir(pin, GPIO_IN);

        // Configure pulls
        gpio_disable_pulls(pin);
        if (g_cfg.data_pull_down) gpio_pull_down(pin);
        if (g_cfg.data_pull_up)   gpio_pull_up(pin);
    }

    // Init ACK pin as output.
    gpio_init(g_cfg.ack_pin);
    gpio_set_function(g_cfg.ack_pin, GPIO_FUNC_SIO);
    gpio_set_dir(g_cfg.ack_pin, GPIO_OUT);

    // Drive initial deasserted level (explicit level requested by cfg).
    sio_write_pin(g_cfg.ack_pin, g_cfg.ack_deasserted_level);

    g_inited = true;
}

void hal_gpio_idle(void) {
    if (!g_inited) return;

    // Ensure DATA are inputs (safe / no bus fight).
    for (uint8_t i = 0; i < g_cfg.data_width; ++i) {
        const uint8_t pin = (uint8_t)(g_cfg.data_base + i);
        gpio_set_function(pin, GPIO_FUNC_SIO);
        gpio_set_dir(pin, GPIO_IN);
    }

    // Deassert ACK.
    sio_write_pin(g_cfg.ack_pin, g_cfg.ack_deasserted_level);
}

uint64_t hal_gpio_read_all(void) {
    // Single snapshot read via SIO input regs.
    const uint64_t lo = (uint64_t)sio_hw->gpio_in;
#if defined(PICO_RP2350)
    const uint64_t hi = ((uint64_t)sio_hw->gpio_hi_in) << 32u;
    return lo | hi;
#else
    return lo;
#endif
}

uint32_t hal_gpio_read_data_raw(void) {
    // Pack contiguous DATA pins into LSBs.
    const uint64_t all = hal_gpio_read_all();
    const uint64_t raw = (all & g_data_mask64) >> g_data_shift;
    return (uint32_t)raw;
}

void hal_gpio_ack_write(bool asserted) {
    // Drive the physical pin to represent "asserted" according to polarity.
    const bool level = ack_level_for_asserted(asserted);
    gpio_put(g_cfg.ack_pin, level);
}

bool hal_gpio_ack_is_asserted(void) {
    // Read pin level and interpret according to polarity.
    const bool level = sio_read_pin(g_cfg.ack_pin);
    return g_cfg.ack_active_high ? level : !level;
}

uint8_t hal_gpio_data_base(void)  { return g_cfg.data_base; }
uint8_t hal_gpio_data_width(void) { return g_cfg.data_width; }
uint8_t hal_gpio_ack_pin(void)    { return g_cfg.ack_pin; }
uint64_t hal_gpio_data_mask64(void) { return g_data_mask64; }
