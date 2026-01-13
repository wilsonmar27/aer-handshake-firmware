// aer_rx_poll.h
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "aer_types.h"   // aer_raw_word_t
#include "ringbuf.h"     // ringbuf_u32_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Pure-C polling receiver (Step 4 bring-up).
 *
 * Implements the DI 4-phase word handshake:
 *   wait DATA != 0
 *   latch raw word
 *   assert ACK
 *   wait DATA == 0
 *   deassert ACK
 *   push raw word into ring buffer (producer side)
 *
 * Backpressure behavior:
 *  - If the ring buffer is full, this receiver will NOT acknowledge an outstanding word.
 *    The transmitter will stall with DATA held valid until space is available.
 */

typedef enum aer_rx_poll_status_e {
    AER_RX_POLL_OK = 0,
    AER_RX_POLL_NO_SPACE,           // ring buffer full; did not ACK
    AER_RX_POLL_TIMEOUT_WAIT_VALID, // DATA never became nonzero within timeout
    AER_RX_POLL_TIMEOUT_WAIT_NEUTRAL// DATA never returned to zero within timeout (word may have been pushed)
} aer_rx_poll_status_t;

typedef struct aer_rx_poll_stats_s {
    uint32_t words_ok;         // completed handshake (ACK cycle completed)
    uint32_t dropped_full;     // ring buffer full drops (still handshaked)
    uint32_t timeouts_valid;   // only if enabled (see below)
    uint32_t timeouts_neutral; // only if enabled (see below)
} aer_rx_poll_stats_t;


typedef struct aer_rx_poll_s {
    ringbuf_u32_t *rb;

    // Timeouts (us)
    // wait_valid_timeout_us == 0   => wait forever (idle is not an error)
    uint32_t wait_valid_timeout_us;

    // wait_neutral_timeout_us == 0 => disabled (debug-only)
    uint32_t wait_neutral_timeout_us;

    aer_rx_poll_stats_t stats;
} aer_rx_poll_t;

/**
 * Initialize receiver.
 * Assumes hal_gpio_init() and hal_time_init() have already been called elsewhere.
 */
void aer_rx_poll_init(aer_rx_poll_t *rx,
                      ringbuf_u32_t *rb,
                      uint32_t wait_valid_timeout_us,
                      uint32_t wait_neutral_timeout_us);

/** Reset stats and force ACK deasserted. */
void aer_rx_poll_reset(aer_rx_poll_t *rx);

/**
 * Attempt to receive exactly one raw word.
 * This function may busy-wait (poll) up to the configured timeouts.
 */
aer_rx_poll_status_t aer_rx_poll_step(aer_rx_poll_t *rx);

/**
 * Service loop helper:
 * - tries up to max_words handshakes
 * - optionally stops after time_budget_us (0 => no budget)
 * Returns number of words successfully pushed.
 */
uint32_t aer_rx_poll_service(aer_rx_poll_t *rx, uint32_t max_words, uint32_t time_budget_us);

/** Read-only stats accessor. */
static inline const aer_rx_poll_stats_t *aer_rx_poll_stats(const aer_rx_poll_t *rx) {
    return &rx->stats;
}

#ifdef __cplusplus
} // extern "C"
#endif
