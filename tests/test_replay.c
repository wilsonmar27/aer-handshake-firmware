#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32)
  #include <direct.h>
  static int mk_dir(const char* path) { return _mkdir(path); }
#else
  #include <sys/stat.h>
  #include <sys/types.h>
  static int mk_dir(const char* path) { return mkdir(path, 0777); }
#endif

#include "../common/include/aer_cfg.h"
#include "../common/include/aer_types.h"
#include "../common/include/aer_codec.h"
#include "../common/include/aer_burst.h"

#include "../host/aer_tx_model.h"

/* --- Forward declarations for host/aer_rx_replay.c (no header yet) --- */

typedef bool (*aer_rx_fault_fn_t)(uint64_t t,
                                 aer_raw_word_t* io_data,
                                 bool* io_ack,
                                 void* user);

typedef struct aer_rx_replay_cfg_s {
    bool latch_on_ack_rise;
    bool ignore_invalid_words;
    bool count_neutral_as_error;

    aer_rx_fault_fn_t fault_fn;
    void* fault_user;
} aer_rx_replay_cfg_t;

typedef struct aer_rx_replay_stats_s {
    uint32_t samples_seen;

    uint32_t ack_rises;
    uint32_t words_latched;

    uint32_t codec_ok;
    uint32_t codec_invalid;
    uint32_t codec_neutral;

    uint32_t bursts_completed;
    uint32_t events_emitted;

    uint32_t protocol_issues;
} aer_rx_replay_stats_t;

bool aer_rx_replay_run(const aer_waveform_t* wf,
                       const aer_rx_replay_cfg_t* cfg_in,
                       aer_burst_t* burst,
                       aer_event_cb_t emit_cb,
                       void* emit_user,
                       aer_rx_replay_stats_t* out_stats);

/* Fault injectors defined in host/aer_rx_replay.c */
typedef struct aer_fault_glitch_s {
    uint64_t start_t;
    uint64_t end_t;
    aer_raw_word_t xor_mask;
} aer_fault_glitch_t;

bool aer_fault_glitch_data(uint64_t t, aer_raw_word_t* io_data, bool* io_ack, void* user);

typedef struct aer_fault_stuck_ack_s {
    uint64_t start_t;
    bool     level;
} aer_fault_stuck_ack_t;

bool aer_fault_stuck_ack(uint64_t t, aer_raw_word_t* io_data, bool* io_ack, void* user);

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
    event_t ev[256];
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

/* ---------------- trace dump helpers ---------------- */

static void ensure_traces_dir(void)
{
    /* Try to create; ignore "already exists". */
    if (mk_dir("traces") != 0) {
        /* If it already exists, this is fine. */
        /* On POSIX errno==EEXIST; on Windows it’s also nonzero. */
        /* We won't hard-fail: fopen below will decide. */
    }
}

static bool dump_waveform_trace(const char* path, const aer_waveform_t* wf)
{
    if (!path || !wf) return false;
    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "[WARN] Could not open trace file '%s' for writing: %s\n",
                path, strerror(errno));
        return false;
    }

    fprintf(f, "# t data_hex ack\n");
    for (size_t i = 0; i < wf->len; ++i) {
        const aer_tx_sample_t s = wf->samples[i];
        /* print wide enough; plot_bursts.py accepts 0x... */
        fprintf(f, "%llu 0x%08x %u\n",
                (unsigned long long)s.t,
                (unsigned)s.data,
                (unsigned)(s.ack ? 1u : 0u));
    }

    fclose(f);
    return true;
}

/* ---------------- helpers ---------------- */

static void build_words_for_burst(aer_raw_word_t out_words[4])
{
    uint32_t err = 0u;
    bool ok = false;

    aer_raw_word_t w_row = 0u, w_c1 = 0u, w_c2 = 0u, w_tail = 0u;

    ok = aer_encode_payload(5u, &w_row, &err);
    TASSERT(ok); TASSERT_EQ_U32(err, 0u);

    ok = aer_encode_payload(3u, &w_c1, &err);
    TASSERT(ok); TASSERT_EQ_U32(err, 0u);

    ok = aer_encode_payload(7u, &w_c2, &err);
    TASSERT(ok); TASSERT_EQ_U32(err, 0u);

    ok = aer_encode_payload((uint8_t)AER_TAIL_PAYLOAD, &w_tail, &err);
    TASSERT(ok); TASSERT_EQ_U32(err, 0u);

    out_words[0] = w_row;
    out_words[1] = w_c1;
    out_words[2] = w_c2;
    out_words[3] = w_tail;
}

/* Choose an XOR mask that will force a multi-hot in group0 (low nibble) for this raw word. */
static aer_raw_word_t make_multihot_mask_group0(aer_raw_word_t valid_raw)
{
    uint32_t nib = (uint32_t)(valid_raw & 0xFu);
    /* Find a bit in the nibble that is currently 0, and toggle it on (XOR). */
    for (uint32_t bit = 0u; bit < 4u; ++bit) {
        const uint32_t m = (1u << bit);
        if ((nib & m) == 0u) {
            return (aer_raw_word_t)m; /* toggling this bit will create multi-hot */
        }
    }
    /* Worst-case: nib already 0xF (should never happen for valid 1-of-4). */
    return (aer_raw_word_t)0x1u;
}

/* ---------------- tests ---------------- */

static void test_replay_happy_path_and_dump_trace(void)
{
    aer_raw_word_t words[4];
    build_words_for_burst(words);

    aer_waveform_t wf;
    aer_waveform_init(&wf);

    aer_tx_model_cfg_t cfg = aer_tx_model_cfg_default();
    /* Keep defaults: ack_rise_delay=1, data_clear_delay=0, ack_fall_delay=1 */
    aer_tx_model_t tx;
    aer_tx_model_init(&tx, &cfg, &wf, 0u);

    bool ok = aer_tx_model_emit_words(&tx, words, 4u);
    TASSERT(ok);
    TASSERT(wf.len > 0u);

    /* Dump waveform trace for plotting */
    ensure_traces_dir();
    (void)dump_waveform_trace("traces/replay_happy_waveform.txt", &wf);

    aer_burst_t burst;
    aer_burst_init(&burst);

    event_sink_t sink = {0};

    aer_rx_replay_cfg_t rcfg = {0};
    rcfg.latch_on_ack_rise = true;
    rcfg.ignore_invalid_words = true;
    rcfg.count_neutral_as_error = true;
    rcfg.fault_fn = NULL;
    rcfg.fault_user = NULL;

    aer_rx_replay_stats_t st;
    memset(&st, 0, sizeof(st));

    ok = aer_rx_replay_run(&wf, &rcfg, &burst, on_event, &sink, &st);
    TASSERT(ok);

    /* We emitted 4 words => expect 4 ack rises and 4 latched words. */
    TASSERT_EQ_U32(st.ack_rises, 4u);
    TASSERT_EQ_U32(st.words_latched, 4u);

    /* Only row+2 cols should produce 2 events at tail. */
    TASSERT_EQ_U32(sink.n, 2u);
    TASSERT_EQ_U8(sink.ev[0].row, 5u);
    TASSERT_EQ_U8(sink.ev[0].col, 3u);
    TASSERT_EQ_U8(sink.ev[1].row, 5u);
    TASSERT_EQ_U8(sink.ev[1].col, 7u);

    TASSERT_EQ_U32(st.bursts_completed, 1u);
    TASSERT_EQ_U32(st.events_emitted, 2u);

    aer_waveform_free(&wf);
}

static void test_replay_glitch_invalid_word_ignored(void)
{
    aer_raw_word_t words[4];
    build_words_for_burst(words);

    aer_waveform_t wf;
    aer_waveform_init(&wf);

    aer_tx_model_cfg_t cfg = aer_tx_model_cfg_default();
    aer_tx_model_t tx;
    aer_tx_model_init(&tx, &cfg, &wf, 0u);
    bool ok = aer_tx_model_emit_words(&tx, words, 4u);
    TASSERT(ok);

    /* Defaults timeline:
       word0 ack rises at t=1
       word1 ack rises at t=3
       word2 ack rises at t=5
       word3 ack rises at t=7
    */
    aer_fault_glitch_t glitch;
    glitch.start_t = 3u;
    glitch.end_t   = 3u;
    glitch.xor_mask = make_multihot_mask_group0(words[1]);

    aer_burst_t burst;
    aer_burst_init(&burst);

    event_sink_t sink = {0};

    aer_rx_replay_cfg_t rcfg = {0};
    rcfg.latch_on_ack_rise = true;
    rcfg.ignore_invalid_words = true;   /* important: ignore invalid -> dropped col */
    rcfg.count_neutral_as_error = false;
    rcfg.fault_fn = aer_fault_glitch_data;
    rcfg.fault_user = &glitch;

    aer_rx_replay_stats_t st;
    memset(&st, 0, sizeof(st));

    ok = aer_rx_replay_run(&wf, &rcfg, &burst, on_event, &sink, &st);
    TASSERT(ok);

    /* Still 4 ACK rises and 4 latched words, but one should decode invalid. */
    TASSERT_EQ_U32(st.ack_rises, 4u);
    TASSERT_EQ_U32(st.words_latched, 4u);
    TASSERT(st.codec_invalid >= 1u);

    /* Because COL=3 got corrupted and ignored, only COL=7 should remain in burst. */
    TASSERT_EQ_U32(sink.n, 1u);
    TASSERT_EQ_U8(sink.ev[0].row, 5u);
    TASSERT_EQ_U8(sink.ev[0].col, 7u);

    aer_waveform_free(&wf);
}

static void test_replay_ack_stuck_high_prevents_progress(void)
{
    aer_raw_word_t words[4];
    build_words_for_burst(words);

    aer_waveform_t wf;
    aer_waveform_init(&wf);

    aer_tx_model_cfg_t cfg = aer_tx_model_cfg_default();
    aer_tx_model_t tx;
    aer_tx_model_init(&tx, &cfg, &wf, 0u);
    bool ok = aer_tx_model_emit_words(&tx, words, 4u);
    TASSERT(ok);

    /* Force ACK stuck high starting at t>=2 (prevent falling after first word). */
    aer_fault_stuck_ack_t stuck;
    stuck.start_t = 2u;
    stuck.level   = true;

    aer_burst_t burst;
    aer_burst_init(&burst);

    event_sink_t sink = {0};

    aer_rx_replay_cfg_t rcfg = {0};
    rcfg.latch_on_ack_rise = true;
    rcfg.ignore_invalid_words = true;
    rcfg.count_neutral_as_error = false;
    rcfg.fault_fn = aer_fault_stuck_ack;
    rcfg.fault_user = &stuck;

    aer_rx_replay_stats_t st;
    memset(&st, 0, sizeof(st));

    ok = aer_rx_replay_run(&wf, &rcfg, &burst, on_event, &sink, &st);
    TASSERT(ok);

    /* Only the first ACK rise should be detected; subsequent rises cannot occur. */
    TASSERT_EQ_U32(st.ack_rises, 1u);
    TASSERT_EQ_U32(st.words_latched, 1u);

    /* No tail latched => no burst completion, no events emitted. */
    TASSERT_EQ_U32(sink.n, 0u);
    TASSERT_EQ_U32(st.bursts_completed, 0u);
    TASSERT_EQ_U32(st.events_emitted, 0u);

    aer_waveform_free(&wf);
}

int main(void)
{
    test_replay_happy_path_and_dump_trace();
    test_replay_glitch_invalid_word_ignored();
    test_replay_ack_stuck_high_prevents_progress();

    if (g_failures == 0) {
        printf("[PASS] test_replay\n");
        printf("Trace written (if possible): traces/replay_happy_waveform.txt\n");
        return 0;
    }

    fprintf(stderr, "[FAIL] test_replay: %d failures\n", g_failures);
    return 1;
}
