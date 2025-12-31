#ifndef AER_BURST_H
#define AER_BURST_H

/*
 * AER burst assembler (portable)
 *
 * Consumes decoded *valid* words and assembles them into bursts of:
 *   ROW, COL*, TAIL
 *
 * This module is platform-agnostic (no Pico SDK includes).
 */

#include <stdint.h>
#include <stdbool.h>

#include "aer_cfg.h"
#include "aer_types.h"
#include "aer_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum aer_burst_state_e {
    AER_BURST_EXPECT_ROW = 0,
    AER_BURST_EXPECT_COL_OR_TAIL = 1
} aer_burst_state_t;

/* Burst/parser error flags.
 * Keep this minimal to match current hardware observability.
 */
typedef enum aer_burst_err_e {
    AER_BURST_ERR_NONE              = 0u,
    AER_BURST_ERR_TAIL_WITHOUT_ROW  = 1u << 0,

    /* Optional warnings (useful in debug). */
    AER_BURST_WARN_ROW_OOR          = 1u << 8,  // row out of [0..AER_ROWS-1]
    AER_BURST_WARN_COL_OOR          = 1u << 9,  // col out of [0..AER_COLS-1]
    AER_BURST_WARN_COL_OVERFLOW     = 1u << 10  // too many cols buffered
} aer_burst_err_t;

/* Callback signature for emitted events (row, col). */
typedef void (*aer_event_cb_t)(uint8_t row, uint8_t col, void* user);

/* Burst assembler instance. */
typedef struct aer_burst_s {
    aer_burst_state_t state;

    uint8_t row;                 /* current burst row */
    uint8_t cols[AER_COLS];      /* buffered columns for current row burst */
    uint16_t col_count;          /* number of buffered columns */

    uint32_t err_flags;          /* aer_burst_err_t bitmask */
    uint32_t bursts_completed;   /* number of bursts ended by TAIL */
    uint32_t events_emitted;     /* total events emitted */
} aer_burst_t;

/* Initialize burst assembler to a known state (EXPECT_ROW). */
void aer_burst_init(aer_burst_t* b);

/* Reset current burst (clears row/cols), preserves counters unless requested. */
void aer_burst_reset(aer_burst_t* b, bool clear_counters);

/* Feed one decoded word.
 *
 * Input should generally be the output of aer_decode_word().
 * - If word.ok is false, this function ignores it (no state change).
 * - If word.is_tail is true:
 *     - If waiting for row => sets TAIL_WITHOUT_ROW error and stays EXPECT_ROW.
 *     - Else ends burst, emits buffered events, returns count emitted.
 * - Else (non-tail payload):
 *     - If waiting for row => stores row and transitions to EXPECT_COL_OR_TAIL.
 *     - Else buffers column.
 *
 * Returns: number of events emitted by this call (0 except on tail end-of-burst).
 */
uint16_t aer_burst_feed(aer_burst_t* b,
                        aer_codec_result_t word,
                        aer_event_cb_t emit_cb,
                        void* user);

/* Accessors */
static inline aer_burst_state_t aer_burst_state(const aer_burst_t* b) { return b->state; }
static inline uint32_t aer_burst_errors(const aer_burst_t* b) { return b->err_flags; }

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AER_BURST_H */
