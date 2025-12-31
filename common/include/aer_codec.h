#ifndef AER_CODEC_H
#define AER_CODEC_H

/*
 * AER codec (portable)
 *
 * This layer turns a raw sampled DATA bus word (packed into LSBs of aer_raw_word_t)
 * into a compact payload (AER_PAYLOAD_BITS) and flags any protocol/encoding issues.
 *
 * Validation rules:
 * - Neutral/spacer is all-zero on the physical DATA lines.
 * - A valid non-neutral word has EXACTLY ONE asserted line in EACH 4-wire group.
 * - Any mixed/illegal pattern (multi-hot or missing-hot in any group) is invalid.
 */

#include <stdint.h>
#include <stdbool.h>

#include "aer_cfg.h"
#include "aer_types.h"

// In case cpp is used later
#ifdef __cplusplus
extern "C" {
#endif

/* Bitmask of decode errors/warnings.
 *
 * Notes:
 * - Multiple flags may be set.
 * - Some flags are "hard" errors (ok=false), some are warnings (ok may still be true).
 */
typedef enum aer_codec_err_e {
    AER_CODEC_ERR_NONE          = 0u,

    /* Raw word (after masking to AER_DATA_WIDTH) is neutral/spacer (all zeros). */
    AER_CODEC_ERR_NEUTRAL       = 1u << 0,

    /* Raw word had bits set outside AER_RAW_MASK (information loss if masked). */
    AER_CODEC_ERR_OUT_OF_RANGE  = 1u << 1,

    /* In at least one group, more than one line asserted (not 1-of-4). */
    AER_CODEC_ERR_MULTI_HOT     = 1u << 2,

    /* In at least one group, no line asserted while word is non-neutral. */
    AER_CODEC_ERR_ZERO_HOT      = 1u << 3,

    /* Warning: pad bit(s) set on a non-tail payload (unexpected for 32x32 mode). */
    AER_CODEC_WARN_PAD_BIT_SET  = 1u << 4,
} aer_codec_err_t;

/* Result of decoding a raw word. */
typedef struct aer_codec_result_s {
    bool     ok;         /* true if valid 1-of-4 word and not neutral */
    uint8_t  payload;    /* decoded payload bits (AER_PAYLOAD_BITS in LSBs) */
    bool     is_tail;    /* payload matches AER_TAIL_PAYLOAD and ok==true */
    uint32_t err_flags;  /* aer_codec_err_t bitmask */
} aer_codec_result_t;

/* Decode a raw word from the DATA bus.
 *
 * - The input raw word may contain bits beyond AER_DATA_WIDTH; they are ignored
 *   for decoding but AER_CODEC_ERR_OUT_OF_RANGE is raised if any are set.
 * - Neutral words are not considered valid data words (ok=false, NEUTRAL flag set).
 */
aer_codec_result_t aer_decode_word(aer_raw_word_t raw);

/* Convenience form for callers that prefer out-params.
 * Returns the same value as result.ok.
 */
bool aer_decode_word_ex(aer_raw_word_t raw,
                        uint8_t* out_payload,
                        bool*    out_is_tail,
                        uint32_t* out_err_flags);

/* Encode a payload into a raw 1-of-4 word on the physical bus.
 *
 * Returns true if payload fits within AER_PAYLOAD_BITS.
 * On failure, out_raw is set to 0.
 *
 * This is mainly useful for test vector generation on the host.
 */
bool aer_encode_payload(uint8_t payload,
                        aer_raw_word_t* out_raw,
                        uint32_t* out_err_flags);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AER_CODEC_H */
