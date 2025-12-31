#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "../common/include/aer_cfg.h"
#include "../common/include/aer_types.h"
#include "../common/include/aer_codec.h"
#include "../common/include/aer_burst.h"

/* ---------------- tiny test helpers ---------------- */

static int g_failures = 0;

#define TASSERT(cond) do { \
    if (!(cond)) { \
        ++g_failures; \
        fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define TASSERT_EQ_U32(a,b) do { \
    uint32_t _a = (uint32_t)(a); \
    uint32_t _b = (uint32_t)(b); \
    if (_a != _b) { \
        ++g_failures; \
        fprintf(stderr, "[FAIL] %s:%d: %s (%u) != %s (%u)\n", __FILE__, __LINE__, #a, _a, #b, _b); \
    } \
} while (0)

#define TASSERT_EQ_U8(a,b) do { \
    uint8_t _a = (uint8_t)(a); \
    uint8_t _b = (uint8_t)(b); \
    if (_a != _b) { \
        ++g_failures; \
        fprintf(stderr, "[FAIL] %s:%d: %s (%u) != %s (%u)\n", __FILE__, __LINE__, #a, (unsigned)_a, #b, (unsigned)_b); \
    } \
} while (0)

/* ---------------- event capture ---------------- */

typedef struct { uint8_t row, col; } event_t;

typedef struct {
    event_t ev[1024];
    uint32_t n;
} event_sink_t;

static void on_event(uint8_t row, uint8_t col, void* user)
{
    event_sink_t* s = (event_sink_t*)user;
    if (!s) return;
    if (s->n < (uint32_t)(sizeof(s->ev)/sizeof(s->ev[0]))) {
        s->ev[s->n].row = row;
        s->ev[s->n].col = col;
        s->n++;
    }
}

static aer_codec_result_t make_ok_payload(uint8_t payload)
{
    aer_codec_result_t r;
    r.ok = true;
    r.payload = payload;
    r.is_tail = (payload == (uint8_t)AER_TAIL_PAYLOAD);
    r.err_flags = 0u;
    return r;
}

static aer_codec_result_t make_tail(void) { return make_ok_payload((uint8_t)AER_TAIL_PAYLOAD); }

static aer_codec_result_t make_invalid(void)
{
    aer_codec_result_t r;
    r.ok = false;
    r.payload = 0u;
    r.is_tail = false;
    r.err_flags = AER_CODEC_ERR_MULTI_HOT;
    return r;
}

static void test_tail_without_row(void)
{
    aer_burst_t b;
    aer_burst_init(&b);

    event_sink_t sink = {0};

    uint16_t emitted = aer_burst_feed(&b, make_tail(), on_event, &sink);
    TASSERT_EQ_U32(emitted, 0u);
    TASSERT((aer_burst_errors(&b) & AER_BURST_ERR_TAIL_WITHOUT_ROW) != 0u);
    TASSERT_EQ_U32(sink.n, 0u);
    TASSERT(aer_burst_state(&b) == AER_BURST_EXPECT_ROW);
}

static void test_single_col_burst(void)
{
    aer_burst_t b;
    aer_burst_init(&b);

    event_sink_t sink = {0};

    (void)aer_burst_feed(&b, make_ok_payload(5u), on_event, &sink); /* ROW=5 */
    (void)aer_burst_feed(&b, make_ok_payload(3u), on_event, &sink); /* COL=3 */
    uint16_t emitted = aer_burst_feed(&b, make_tail(), on_event, &sink);

    TASSERT_EQ_U32(emitted, 1u);
    TASSERT_EQ_U32(sink.n, 1u);
    TASSERT_EQ_U8(sink.ev[0].row, 5u);
    TASSERT_EQ_U8(sink.ev[0].col, 3u);
    TASSERT(aer_burst_state(&b) == AER_BURST_EXPECT_ROW);
    TASSERT_EQ_U32(b.bursts_completed, 1u);
    TASSERT_EQ_U32(b.events_emitted, 1u);
}

static void test_multi_col_burst(void)
{
    aer_burst_t b;
    aer_burst_init(&b);

    event_sink_t sink = {0};

    (void)aer_burst_feed(&b, make_ok_payload(5u), on_event, &sink); /* ROW=5 */
    (void)aer_burst_feed(&b, make_ok_payload(3u), on_event, &sink); /* COL=3 */
    (void)aer_burst_feed(&b, make_ok_payload(7u), on_event, &sink); /* COL=7 */
    uint16_t emitted = aer_burst_feed(&b, make_tail(), on_event, &sink);

    TASSERT_EQ_U32(emitted, 2u);
    TASSERT_EQ_U32(sink.n, 2u);
    TASSERT_EQ_U8(sink.ev[0].row, 5u);
    TASSERT_EQ_U8(sink.ev[0].col, 3u);
    TASSERT_EQ_U8(sink.ev[1].row, 5u);
    TASSERT_EQ_U8(sink.ev[1].col, 7u);
}

static void test_invalid_words_ignored(void)
{
    aer_burst_t b;
    aer_burst_init(&b);

    event_sink_t sink = {0};

    (void)aer_burst_feed(&b, make_ok_payload(2u), on_event, &sink);   /* ROW=2 */
    (void)aer_burst_feed(&b, make_invalid(), on_event, &sink);       /* ignored */
    (void)aer_burst_feed(&b, make_ok_payload(9u), on_event, &sink);  /* COL=9 */
    uint16_t emitted = aer_burst_feed(&b, make_tail(), on_event, &sink);

    TASSERT_EQ_U32(emitted, 1u);
    TASSERT_EQ_U32(sink.n, 1u);
    TASSERT_EQ_U8(sink.ev[0].row, 2u);
    TASSERT_EQ_U8(sink.ev[0].col, 9u);
}

static void test_col_overflow_warning(void)
{
    aer_burst_t b;
    aer_burst_init(&b);

    event_sink_t sink = {0};

    (void)aer_burst_feed(&b, make_ok_payload(1u), on_event, &sink); /* ROW=1 */

    for (uint32_t i = 0; i < (uint32_t)AER_COLS + 5u; ++i) {
        (void)aer_burst_feed(&b, make_ok_payload((uint8_t)(i & 0x1Fu)), on_event, &sink);
    }

    uint16_t emitted = aer_burst_feed(&b, make_tail(), on_event, &sink);

    TASSERT_EQ_U32(emitted, (uint32_t)AER_COLS);
    TASSERT_EQ_U32(sink.n, (uint32_t)AER_COLS);
    TASSERT((aer_burst_errors(&b) & AER_BURST_WARN_COL_OVERFLOW) != 0u);
}

int main(void)
{
    test_tail_without_row();
    test_single_col_burst();
    test_multi_col_burst();
    test_invalid_words_ignored();
    test_col_overflow_warning();

    if (g_failures == 0) {
        printf("[PASS] test_burst\n");
        return 0;
    }

    fprintf(stderr, "[FAIL] test_burst: %d failures\n", g_failures);
    return 1;
}
