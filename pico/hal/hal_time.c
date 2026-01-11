// pico/hal/hal_time.c
#include "hal_time.h"

#include "pico/time.h"
#include "hardware/clocks.h"
#include "pico/platform.h"

#if defined(PICO_RP2350)
// Many Pico SDK setups expose CMSIS for Cortex-M33 automatically via pico/platform.h.
// If the build complains that DWT/CoreDebug are undefined, add an explicit include
// for the CMSIS core header here (project-dependent).
#endif

static uint32_t g_clk_sys_hz = 0;
static uint32_t g_cycles_per_us = 0;
static bool     g_dwt_ok = false;

static void enable_dwt_cycle_counter_if_present(void) {
    g_dwt_ok = false;

    /* DWT CYCCNT is available on Cortex-M3+ (incl. M33) if not locked down. */
#if defined(DWT) && defined(CoreDebug) && defined(DWT_CTRL_CYCCNTENA_Msk) && defined(CoreDebug_DEMCR_TRCENA_Msk)
    /* Enable trace (required for DWT) */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* Reset and enable CYCCNT */
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* Quick sanity check: does it count? */
    uint32_t a = DWT->CYCCNT;
    __asm volatile("nop");
    uint32_t b = DWT->CYCCNT;

    if (b != a) {
        g_dwt_ok = true;
    }
#endif
}

void hal_time_init(void) {
    g_clk_sys_hz = (uint32_t)clock_get_hz(clk_sys);
    if (g_clk_sys_hz == 0u) {
        /* Should never happen, but avoid div-by-zero. */
        g_clk_sys_hz = 125000000u; /* common default */
    }

    g_cycles_per_us = g_clk_sys_hz / 1000000u;
    if (g_cycles_per_us == 0u) {
        g_cycles_per_us = 1u;
    }

    enable_dwt_cycle_counter_if_present();
}

uint64_t hal_time_us_now(void) {
    return time_us_64();
}

bool hal_time_expired(uint64_t deadline_us) {
    /* Signed compare is robust even if you ever change timebase to wrap. */
    const int64_t diff = (int64_t)(deadline_us - hal_time_us_now());
    return (diff <= 0);
}

uint32_t hal_time_remaining_us(uint64_t deadline_us) {
    const uint64_t now = hal_time_us_now();
    if (deadline_us <= now) return 0;
    const uint64_t rem = deadline_us - now;
    return (rem > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t)rem;
}

void hal_time_wait_until(uint64_t deadline_us) {
    while (!hal_time_expired(deadline_us)) {
        tight_loop_contents();
    }
}

void hal_time_sleep_us(uint32_t us) {
    sleep_us(us);
}

void hal_time_spin_us(uint32_t us) {
    /* busy_wait_us_32 is tighter than sleep_us (no scheduler/yield). */
    busy_wait_us_32(us);
}

uint32_t hal_cycles_now(void) {
#if defined(DWT) && defined(DWT_CTRL_CYCCNTENA_Msk)
    if (g_dwt_ok) {
        return (uint32_t)DWT->CYCCNT;
    }
#endif
    /* Fallback: derive a cycle-ish counter from microseconds. */
    const uint64_t us = hal_time_us_now();
    return (uint32_t)(us * (uint64_t)g_cycles_per_us);
}

uint32_t hal_cycles_to_us(uint32_t cycles) {
    if (g_cycles_per_us == 0u) return 0u;
    return cycles / g_cycles_per_us;
}

uint32_t hal_us_to_cycles(uint32_t us) {
    const uint64_t c = (uint64_t)us * (uint64_t)g_cycles_per_us;
    return (c > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t)c;
}

void hal_time_spin_cycles(uint32_t cycles) {
    const uint32_t start = hal_cycles_now();
    while (hal_cycles_diff(hal_cycles_now(), start) < cycles) {
        tight_loop_contents();
    }
}
