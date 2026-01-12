// aer_event_sink.h
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Event callback glue.
 *
 * This module is meant to be the "sink" your portable burst parser calls when it
 * produces a decoded ON event (row, col).
 *
 * What it does:
 *  - forwards events to usb_stream (which timestamps at emission if enabled)
 *  - keeps simple counters (emitted/sent/dropped)
 *
 * What it does NOT do:
 *  - any visualization logic
 *  - any parsing/decoding (that remains in common/)
 */

typedef struct aer_event_sink_stats_s {
    uint32_t events_emitted;     // callback invoked (events produced by parser)
    uint32_t usb_sent_ok;        // forwarded to usb_stream successfully
    uint32_t usb_send_failed;    // usb_stream returned false (e.g., not connected)
} aer_event_sink_stats_t;

typedef struct aer_event_sink_cfg_s {
    bool enabled;                // global enable (useful to silence stream quickly)
} aer_event_sink_cfg_t;

typedef struct aer_event_sink_s {
    aer_event_sink_cfg_t   cfg;
    aer_event_sink_stats_t stats;
} aer_event_sink_t;

/** Initialize sink. Call after hal_stdio_init() + usb_stream_init(). */
void aer_event_sink_init(aer_event_sink_t *sink, const aer_event_sink_cfg_t *cfg);

/** Reset counters. */
void aer_event_sink_reset(aer_event_sink_t *sink);

/** Enable/disable forwarding (counters still track emitted events). */
void aer_event_sink_set_enabled(aer_event_sink_t *sink, bool enabled);

/** Stats accessor. */
const aer_event_sink_stats_t *aer_event_sink_stats(const aer_event_sink_t *sink);

/**
 * Callback function you pass to the common burst parser.
 *
 * Signature intentionally simple:
 *   - row/col are the decoded pixel address
 *   - user is expected to be (aer_event_sink_t*) or a wrapper that contains it
 *
 * If your common parser expects a different callback type, write a tiny adapter
 * that calls this function.
 */
void aer_event_sink_on_event(uint8_t row, uint8_t col, void *user);

#ifdef __cplusplus
} // extern "C"
#endif
