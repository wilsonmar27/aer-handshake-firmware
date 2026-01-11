// pico/hal/hal_time.h
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Time / timeout helpers for Pico 2 (RP2350).
 *
 * Design goals:
 *  - give you a consistent "deadline in microseconds" API for polling loops
 *  - provide a cheap cycle counter for profiling / tight timeouts when available
 *
 * Notes:
 *  - Microsecond timebase uses Pico SDK's monotonic timer (time_us_64()).
 *  - Cycle counter prefers ARM DWT->CYCCNT if present/enabled; otherwise falls back to a
 *    derived counter from time_us_64() and clk_sys (still useful for coarse profiling).
 */

/** Call once at boot (per-core if you use both cores). Enables cycle counter if available. */
void hal_time_init(void);

/** Monotonic time since boot in microseconds. */
uint64_t hal_time_us_now(void);

/** Create a deadline = now + timeout_us. */
static inline uint64_t hal_time_deadline_us(uint32_t timeout_us) {
    return hal_time_us_now() + (uint64_t)timeout_us;
}

/** Returns true if now >= deadline_us. */
bool hal_time_expired(uint64_t deadline_us);

/** Returns remaining time until deadline (0 if expired). */
uint32_t hal_time_remaining_us(uint64_t deadline_us);

/** Busy-wait until deadline or return immediately if already expired. */
void hal_time_wait_until(uint64_t deadline_us);

/** Sleep-yield for at least us microseconds (uses SDK sleep_us). */
void hal_time_sleep_us(uint32_t us);

/** Spin (busy wait) for exactly us microseconds (uses SDK busy_wait). */
void hal_time_spin_us(uint32_t us);

/* ---------------- Cycle counter (profiling / tight deltas) ---------------- */

/**
 * Returns a 32-bit free-running cycle counter.
 * Wraps naturally (unsigned arithmetic).
 */
uint32_t hal_cycles_now(void);

/** Unsigned wrap-safe diff: returns (newer - older) in cycles. */
static inline uint32_t hal_cycles_diff(uint32_t newer, uint32_t older) {
    return (uint32_t)(newer - older);
}

/** Convert a cycle delta to microseconds (rounded down). */
uint32_t hal_cycles_to_us(uint32_t cycles);

/** Convert microseconds to cycles (may saturate/truncate to 32-bit). */
uint32_t hal_us_to_cycles(uint32_t us);

/** Busy-wait for a number of cycles (wrap-safe). */
void hal_time_spin_cycles(uint32_t cycles);

#ifdef __cplusplus
} // extern "C"
#endif
