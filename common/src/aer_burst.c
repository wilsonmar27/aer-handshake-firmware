#include "aer_burst.h"

static inline uint8_t aer_payload_to_index(uint8_t payload)
{
    /* Lower AER_INDEX_BITS bits hold the index (pad bit(s) should be 0). */
    const uint8_t mask = (uint8_t)((1u << AER_INDEX_BITS) - 1u);
    return (uint8_t)(payload & mask);
}

void aer_burst_init(aer_burst_t* b)
{
    if (!b) return;
    b->state = AER_BURST_EXPECT_ROW;
    b->row = 0u;
    b->col_count = 0u;
    b->err_flags = AER_BURST_ERR_NONE;
    b->bursts_completed = 0u;
    b->events_emitted = 0u;
}

void aer_burst_reset(aer_burst_t* b, bool clear_counters)
{
    if (!b) return;
    b->state = AER_BURST_EXPECT_ROW;
    b->row = 0u;
    b->col_count = 0u;
    b->err_flags = AER_BURST_ERR_NONE;

    if (clear_counters) {
        b->bursts_completed = 0u;
        b->events_emitted = 0u;
    }
}

/* Emit buffered events, then clear buffer for next burst. */
static uint16_t aer_emit_and_clear(aer_burst_t* b, aer_event_cb_t cb, void* user)
{
    uint16_t emitted = 0u;
    if (cb) {
        for (uint16_t i = 0u; i < b->col_count; ++i) {
            cb(b->row, b->cols[i], user);
            ++emitted;
        }
    } else {
        /* If no callback provided, we still "emit" conceptually (count them). */
        emitted = b->col_count;
    }

    b->events_emitted += emitted;
    b->col_count = 0u;
    return emitted;
}

uint16_t aer_burst_feed(aer_burst_t* b,
                        aer_codec_result_t word,
                        aer_event_cb_t emit_cb,
                        void* user)
{
    if (!b) return 0u;

    /* Ignore invalid/neutral/malformed words (codec layer decides ok). */
    if (!word.ok) {
        return 0u;
    }

    /* Tailword ends the current burst (if any). */
    if (word.is_tail) {
        if (b->state == AER_BURST_EXPECT_ROW) {
            /* Only parser error we can detect per spec. */
            b->err_flags |= AER_BURST_ERR_TAIL_WITHOUT_ROW;
            /* Stay in EXPECT_ROW; nothing to emit. */
            return 0u;
        }

        /* End burst: emit buffered (row,col) events. */
        const uint16_t emitted = aer_emit_and_clear(b, emit_cb, user);
        b->bursts_completed += 1u;

        /* Return to expecting the next row. */
        b->state = AER_BURST_EXPECT_ROW;
        return emitted;
    }

    /* Non-tail payload: interpret as row or col depending on state. */
    const uint8_t idx = aer_payload_to_index(word.payload);

    if (b->state == AER_BURST_EXPECT_ROW) {
        b->row = idx;

        /* Optional range warning (useful in debug). */
        if ((uint32_t)b->row >= (uint32_t)AER_ROWS) {
            b->err_flags |= AER_BURST_WARN_ROW_OOR;
        }

        b->col_count = 0u;
        b->state = AER_BURST_EXPECT_COL_OR_TAIL;
        return 0u;
    }

    /* EXPECT_COL_OR_TAIL: buffer column */
    if (b->col_count < (uint16_t)AER_COLS) {
        b->cols[b->col_count++] = idx;

        if ((uint32_t)idx >= (uint32_t)AER_COLS) {
            b->err_flags |= AER_BURST_WARN_COL_OOR;
        }
    } else {
        /* Buffer overflow: keep collecting protocol state, but drop extra cols. */
        b->err_flags |= AER_BURST_WARN_COL_OVERFLOW;
    }

    return 0u;
}
