#ifndef AER_TYPES_H
#define AER_TYPES_H

/*
 * AER core types (portable).
 *
 * NOTE: Keep this header free of any platform-specific includes.
 */

#include <stdint.h>
#include <stdbool.h>
#include "aer_cfg.h"

// In case cpp is used later
#ifdef __cplusplus
extern "C" {
#endif

/* Raw sampled bus word */
typedef uint32_t aer_raw_word_t;

/* High-level classification of a received word AFTER decoding. */
typedef enum aer_word_type_e {
    AER_WORD_INVALID = 0,  // malformed 1-of-4 or otherwise unusable 
    AER_WORD_ROW     = 1,
    AER_WORD_COL     = 2,
    AER_WORD_TAIL    = 3
} aer_word_type_t;

/* Decoded word representation used by higher-level logic.
 *
 * Notes:
 * - For ROW words: `row` is valid, `col` may be ignored.
 * - For COL words: `col` is valid, `row` may be ignored (burst state provides row).
 * - For TAIL: `is_tail` is true; row/col are undefined.
 * - For INVALID: fields are undefined; inspect error flags in codec layer.
 */
typedef struct aer_decoded_s {
    aer_word_type_t type;
    uint8_t         row;     // 0..AER_ROWS-1 
    uint8_t         col;     // 0..AER_COLS-1
    bool            is_tail; // convenience mirror of (type == AER_WORD_TAIL)
} aer_decoded_t;

/* Compile-time sanity checks (C11 or newer). */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(AER_GROUP_WIDTH == 4u, "This implementation assumes 1-of-4 groups (4 wires per group).");
_Static_assert((AER_PAYLOAD_BITS % AER_SYMBOL_BITS) == 0u, "Payload bits must be an integer number of 2-bit symbols.");
_Static_assert(AER_DATA_WIDTH == (AER_NUM_GROUPS * AER_GROUP_WIDTH), "DATA width must equal groups * group width.");
_Static_assert(AER_DATA_WIDTH <= 32u, "aer_raw_word_t packing assumes <= 32 DATA lines.");
_Static_assert(AER_ROWS <= 255u && AER_COLS <= 255u, "row/col types assume <= 255.");
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AER_TYPES_H */
