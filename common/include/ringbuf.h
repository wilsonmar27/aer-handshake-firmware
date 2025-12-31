#ifndef RINGBUF_H
#define RINGBUF_H

/*
 * Portable ring buffer for AER firmware.
 *
 * This is intended for single-producer / single-consumer use (ISR/PIO
 * producer, main loop consumer). It is platform-agnostic: no Pico SDK includes.
 *
 * This implementation stores 32-bit items (aer_raw_word_t or similar).
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ringbuf_u32_s {
    uint32_t* buf;       /* storage provided by caller */
    uint32_t  capacity;  /* number of elements in buf (must be >= 2) */

    /* head: next write index, tail: next read index */
    volatile uint32_t head;
    volatile uint32_t tail;
} ringbuf_u32_t;

/* Initialize with caller-provided storage.
 * Returns false if parameters invalid.
 */
bool ringbuf_u32_init(ringbuf_u32_t* rb, uint32_t* storage, uint32_t capacity);

/* Reset to empty state (does not clear memory). */
void ringbuf_u32_reset(ringbuf_u32_t* rb);

/* Non-mutating state queries. */
bool ringbuf_u32_is_empty(const ringbuf_u32_t* rb);
bool ringbuf_u32_is_full(const ringbuf_u32_t* rb);

/* Number of elements currently stored (0..capacity-1). */
uint32_t ringbuf_u32_count(const ringbuf_u32_t* rb);

/* Remaining free slots (0..capacity-1). */
uint32_t ringbuf_u32_free(const ringbuf_u32_t* rb);

/* Push one item. Returns false if full. */
bool ringbuf_u32_push(ringbuf_u32_t* rb, uint32_t v);

/* Pop one item into *out. Returns false if empty. */
bool ringbuf_u32_pop(ringbuf_u32_t* rb, uint32_t* out);

/* Peek (read without removing) the next item. Returns false if empty. */
bool ringbuf_u32_peek(const ringbuf_u32_t* rb, uint32_t* out);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RINGBUF_H */
