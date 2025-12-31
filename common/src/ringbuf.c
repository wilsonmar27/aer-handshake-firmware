#include "ringbuf.h"

static inline uint32_t rb_inc(uint32_t idx, uint32_t capacity)
{
    idx += 1u;
    if (idx >= capacity) {
        idx = 0u;
    }
    return idx;
}

bool ringbuf_u32_init(ringbuf_u32_t* rb, uint32_t* storage, uint32_t capacity)
{
    if (!rb || !storage) {
        return false;
    }
    if (capacity < 2u) {
        /* We need one slot empty to distinguish full vs empty. */
        return false;
    }

    rb->buf = storage;
    rb->capacity = capacity;
    rb->head = 0u;
    rb->tail = 0u;
    return true;
}

void ringbuf_u32_reset(ringbuf_u32_t* rb)
{
    if (!rb) return;
    rb->head = 0u;
    rb->tail = 0u;
}

bool ringbuf_u32_is_empty(const ringbuf_u32_t* rb)
{
    if (!rb) return true;
    return rb->head == rb->tail;
}

bool ringbuf_u32_is_full(const ringbuf_u32_t* rb)
{
    if (!rb) return false;
    const uint32_t next = rb_inc((uint32_t)rb->head, rb->capacity);
    return next == rb->tail;
}

uint32_t ringbuf_u32_count(const ringbuf_u32_t* rb)
{
    if (!rb) return 0u;
    const uint32_t h = (uint32_t)rb->head;
    const uint32_t t = (uint32_t)rb->tail;

    if (h >= t) {
        return h - t;
    }
    return (rb->capacity - t) + h;
}

uint32_t ringbuf_u32_free(const ringbuf_u32_t* rb)
{
    if (!rb) return 0u;
    /* One slot is always left empty. */
    return (rb->capacity - 1u) - ringbuf_u32_count(rb);
}

bool ringbuf_u32_push(ringbuf_u32_t* rb, uint32_t v)
{
    if (!rb || !rb->buf) return false;

    const uint32_t h = (uint32_t)rb->head;
    const uint32_t next = rb_inc(h, rb->capacity);

    if (next == rb->tail) {
        return false; /* full */
    }

    rb->buf[h] = v;

    /* Publish the write by advancing head last. */
    rb->head = next;
    return true;
}

bool ringbuf_u32_pop(ringbuf_u32_t* rb, uint32_t* out)
{
    if (!rb || !rb->buf || !out) return false;

    const uint32_t t = (uint32_t)rb->tail;
    if (t == rb->head) {
        return false; /* empty */
    }

    *out = rb->buf[t];

    /* Consume by advancing tail last. */
    rb->tail = rb_inc(t, rb->capacity);
    return true;
}

bool ringbuf_u32_peek(const ringbuf_u32_t* rb, uint32_t* out)
{
    if (!rb || !rb->buf || !out) return false;

    const uint32_t t = (uint32_t)rb->tail;
    if (t == rb->head) {
        return false; /* empty */
    }

    *out = rb->buf[t];
    return true;
}
