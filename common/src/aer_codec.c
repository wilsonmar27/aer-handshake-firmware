#include "aer_codec.h"

/* --------- internal helpers --------- */

static inline aer_raw_word_t aer_mask_raw(aer_raw_word_t raw)
{
    return (aer_raw_word_t)(raw & (aer_raw_word_t)AER_RAW_MASK);
}


/*
 * Compute Hamming weight of a group of 4 bits
 */
static inline uint32_t popcount4(uint32_t x)
{
    /* x is at most 4 bits, so simple count is fine */
    x = x - ((x >> 1u) & 0x5u);
    x = (x & 0x3u) + ((x >> 2u) & 0x3u);
    return x;
}

/* 
 * x is one-hot in 4 bits. Return index 0..3. 
 */
static inline uint32_t ctz4(uint32_t x)
{
    if (x & 0x1u) return 0u;
    if (x & 0x2u) return 1u;
    if (x & 0x4u) return 2u;
    return 3u; /* assumes x & 0x8 */
}

/* --------- public API --------- */

aer_codec_result_t aer_decode_word(aer_raw_word_t raw)
{
    aer_codec_result_t r;
    r.ok = false;
    r.payload = 0u;
    r.is_tail = false;
    r.err_flags = AER_CODEC_ERR_NONE;

    /* Detect out-of-range bits (above DATA width). */
    if ((raw & ~(aer_raw_word_t)AER_RAW_MASK) != 0u) {
        r.err_flags |= AER_CODEC_ERR_OUT_OF_RANGE;
    }

    const aer_raw_word_t masked = aer_mask_raw(raw);

    /* Neutral/spacer (all zeros) */
    if (masked == 0u) {
        r.err_flags |= AER_CODEC_ERR_NEUTRAL;
        return r;
    }

    /* Decode each 4-wire group into a 2-bit symbol (0..3). */
    uint32_t payload = 0u;
    bool valid = true;

    for (uint32_t g = 0u; g < (uint32_t)AER_NUM_GROUPS; ++g) {
        const uint32_t shift = g * (uint32_t)AER_GROUP_WIDTH;
        const uint32_t nib = (uint32_t)((masked >> shift) & 0xFu);

        if (nib == 0u) {
            r.err_flags |= AER_CODEC_ERR_ZERO_HOT;
            valid = false;
            continue;
        }

        const uint32_t pc = popcount4(nib);
        if (pc != 1u) {
            r.err_flags |= AER_CODEC_ERR_MULTI_HOT;
            valid = false;
            continue;
        }

        const uint32_t sym = ctz4(nib) & 0x3u; /* 0..3 */
        payload |= (sym << (g * (uint32_t)AER_SYMBOL_BITS));
    }

    r.payload = (uint8_t)(payload & ((1u << AER_PAYLOAD_BITS) - 1u));

    if (valid) {
        r.ok = true;
        if ((uint32_t)r.payload == (uint32_t)AER_TAIL_PAYLOAD) {
            r.is_tail = true;
        } else {
            /* Warning: pad bits should be 0 for normal row/col indices.
             * (For the initial 32x32 mode: the 6th bit is always 0 for row/col.)
             */
            const uint32_t all_payload_mask = (1u << AER_PAYLOAD_BITS) - 1u;
            const uint32_t index_mask       = (1u << AER_INDEX_BITS) - 1u;
            const uint32_t pad_mask         = all_payload_mask & ~index_mask;
            if (((uint32_t)r.payload & pad_mask) != 0u) {
                r.err_flags |= AER_CODEC_WARN_PAD_BIT_SET;
            }
        }
    }

    return r;
}

bool aer_decode_word_ex(aer_raw_word_t raw,
                        uint8_t* out_payload,
                        bool*    out_is_tail,
                        uint32_t* out_err_flags)
{
    const aer_codec_result_t r = aer_decode_word(raw);
    if (out_payload)   { *out_payload = r.payload; }
    if (out_is_tail)   { *out_is_tail = r.is_tail; }
    if (out_err_flags) { *out_err_flags = r.err_flags; }
    return r.ok;
}

bool aer_encode_payload(uint8_t payload,
                        aer_raw_word_t* out_raw,
                        uint32_t* out_err_flags)
{
    if (out_err_flags) { *out_err_flags = AER_CODEC_ERR_NONE; }
    if (out_raw)       { *out_raw = 0u; }

    /* Ensure payload fits in AER_PAYLOAD_BITS. */
    const uint32_t max_payload = (1u << AER_PAYLOAD_BITS) - 1u;
    if ((uint32_t)payload > max_payload) {
        if (out_err_flags) { *out_err_flags |= AER_CODEC_ERR_OUT_OF_RANGE; }
        return false;
    }

    aer_raw_word_t raw = 0u;

    for (uint32_t g = 0u; g < (uint32_t)AER_NUM_GROUPS; ++g) {
        const uint32_t sym = ((uint32_t)payload >> (g * (uint32_t)AER_SYMBOL_BITS)) & 0x3u;
        const uint32_t one_hot = (1u << sym) & 0xFu;
        raw |= (aer_raw_word_t)(one_hot << (g * (uint32_t)AER_GROUP_WIDTH));
    }

    if (out_raw) { *out_raw = raw & (aer_raw_word_t)AER_RAW_MASK; }
    return true;
}
