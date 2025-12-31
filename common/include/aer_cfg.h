#ifndef AER_CFG_H
#define AER_CFG_H

/*
 * AER configuration header (portable).
 *
 * This file defines compile-time parameters for the AER DI bus and
 * the target sensor geometry. 
 * 
 * NOTE: Keep this header free of any platform
 * includes so it can be used on host and embedded builds.
 *
 */

#include <stdint.h>

/* ---------------- Sensor geometry ---------------- */
#define AER_ROWS            32u
#define AER_COLS            32u

/* ---------------- Payload / encoding ----------------
 * For the current 32x32 mode:
 *   - 5 index bits (0..31)
 *   - +1 pad bit (always 0) => 6-bit payload
 * Payload is transported as three 2-bit symbols, each encoded 1-of-4.
 */
#define AER_INDEX_BITS      5u
#define AER_PAD_BITS        1u
#define AER_PAYLOAD_BITS    (AER_INDEX_BITS + AER_PAD_BITS) // 6

// Each symbol is 2 bits and maps to one of 4 wires (1-of-4). 
#define AER_SYMBOL_BITS     2u
#define AER_GROUP_WIDTH     4u   // 1-of-4 per group
#define AER_NUM_GROUPS      (AER_PAYLOAD_BITS / AER_SYMBOL_BITS) // 3

// Physical DATA bus width = groups * wires-per-group.
#define AER_DATA_WIDTH      (AER_NUM_GROUPS * AER_GROUP_WIDTH) // 12

// Bitmask for the physical raw word (lowest AER_DATA_WIDTH bits used).
#define AER_RAW_MASK        ((AER_DATA_WIDTH >= 32u) ? 0xFFFFFFFFu : ((1u << AER_DATA_WIDTH) - 1u))

// Reserved tailword payload value
#define AER_TAIL_PAYLOAD    ((1u << AER_PAYLOAD_BITS) - 1u) // 0b111111 for 6 bits

#endif /* AER_CFG_H */
