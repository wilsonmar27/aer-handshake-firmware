/*
 * host/aer_rx_replay.c
 *
 * Virtual receiver replay:
 * - Replays a time-ordered waveform of (DATA, ACK) transitions.
 * - Extracts "latched" raw words on ACK rising edges.
 * - Feeds those words through aer_decode_word() and aer_burst_feed().
 *
 * Fault injection:
 * - Optional callback can mutate (data, ack) before processing each sample.
 * - Use to simulate glitches, missing neutral, stuck ACK, etc.
 *
 * Trace loading:
 * - Optional function to load waveform transitions from a text file:
 *     t  data_hex  ack
 *   (whitespace or commas are accepted)
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "aer_tx_model.h" 
#include "aer_codec.h"
#include "aer_burst.h"

/* ---------------- Public-ish types (keep here for now) ---------------- */

typedef bool (*aer_rx_fault_fn_t)(uint64_t t,
                                 aer_raw_word_t* io_data,
                                 bool* io_ack,
                                 void* user);

/* Replay configuration */
typedef struct aer_rx_replay_cfg_s {
    bool latch_on_ack_rise;        /* default true: word latched when ACK rises */
    bool ignore_invalid_words;     /* default true: don't feed burst if codec.ok==false */
    bool count_neutral_as_error;   /* default false: neutral latched at ACK-rise increments a stat flag */

    aer_rx_fault_fn_t fault_fn;    /* optional fault injector */
    void* fault_user;
} aer_rx_replay_cfg_t;

/* Replay stats */
typedef struct aer_rx_replay_stats_s {
    uint32_t samples_seen;

    uint32_t ack_rises;
    uint32_t words_latched;

    uint32_t codec_ok;
    uint32_t codec_invalid;     /* codec.ok == false on latched word */
    uint32_t codec_neutral;     /* latched word was neutral (raw==0 after mask) */

    uint32_t bursts_completed;  /* copied from burst assembler at end */
    uint32_t events_emitted;    /* copied from burst assembler at end */

    uint32_t protocol_issues;   /* simple counter: e.g., ACK-rise with neutral data */
} aer_rx_replay_stats_t;

/* ---------------- Defaults ---------------- */

static aer_rx_replay_cfg_t aer_rx_replay_cfg_default(void)
{
    aer_rx_replay_cfg_t cfg;
    cfg.latch_on_ack_rise      = true;
    cfg.ignore_invalid_words   = true;
    cfg.count_neutral_as_error = false;
    cfg.fault_fn               = NULL;
    cfg.fault_user             = NULL;
    return cfg;
}

/* ---------------- Core replay engine ---------------- */

/* Run replay:
 * - wf: waveform transitions (monotonic time order)
 * - cfg: optional, pass NULL to use defaults
 * - burst: burst assembler instance (caller may inspect errors/counters after)
 * - emit_cb/user: event sink callback used by aer_burst_feed()
 * - out_stats: optional stats output
 */
bool aer_rx_replay_run(const aer_waveform_t* wf,
                       const aer_rx_replay_cfg_t* cfg_in,
                       aer_burst_t* burst,
                       aer_event_cb_t emit_cb,
                       void* emit_user,
                       aer_rx_replay_stats_t* out_stats)
{
    if (!wf || !burst) return false;

    aer_rx_replay_cfg_t cfg = cfg_in ? *cfg_in : aer_rx_replay_cfg_default();

    aer_rx_replay_stats_t st;
    memset(&st, 0, sizeof(st));

    /* Burst should already be initialized, but we won't assume */
    /* NOTE: If we want to preserve state across replays, remove this line */
    aer_burst_reset(burst, false);

    aer_raw_word_t last_data = 0u;
    bool last_ack = false;
    bool have_last = false;

    for (size_t i = 0; i < wf->len; ++i) {
        st.samples_seen++;

        aer_tx_sample_t s = wf->samples[i];

        /* Apply fault injector, if any (mutate s.data/s.ack) */
        if (cfg.fault_fn) {
            if (!cfg.fault_fn(s.t, &s.data, &s.ack, cfg.fault_user)) {
                /* fault_fn can request abort */
                return false;
            }
        }

        if (!have_last) {
            last_data = s.data;
            last_ack  = s.ack;
            have_last = true;
            continue;
        }

        const bool ack_rise = (last_ack == false) && (s.ack == true);

        if (cfg.latch_on_ack_rise && ack_rise) {
            st.ack_rises++;

            /* Latch the word at the moment ACK rises
             * For our TX model, s.data is still the valid word here
             */
            const aer_raw_word_t latched = s.data;
            st.words_latched++;

            const aer_codec_result_t cr = aer_decode_word(latched);
            if (cr.ok) st.codec_ok++;
            else       st.codec_invalid++;

            if (cr.err_flags & AER_CODEC_ERR_NEUTRAL) {
                st.codec_neutral++;
                if (cfg.count_neutral_as_error) {
                    st.protocol_issues++;
                }
            }

            if (!(cfg.ignore_invalid_words && !cr.ok)) {
                (void)aer_burst_feed(burst, cr, emit_cb, emit_user);
            }
        }

        last_data = s.data;
        last_ack  = s.ack;
    }

    st.bursts_completed = burst->bursts_completed;
    st.events_emitted   = burst->events_emitted;

    if (out_stats) {
        *out_stats = st;
    }
    return true;
}

/* ---------------- Trace loading (optional utility) ----------------
 *
 * Loads transitions from a file containing:
 *   t  data_hex  ack
 * where:
 * - t is unsigned integer (ticks)
 * - data_hex can be "0x..." or hex digits
 * - ack is 0 or 1
 *
 * Separators can be spaces, tabs, or commas
 *
 * Note: This loader appends samples; call aer_waveform_init() first if desired
 */
static bool wf_push_raw(aer_waveform_t* wf, uint64_t t, aer_raw_word_t data, bool ack)
{
    if (!wf) return false;

    /* ensure capacity */
    if (wf->len + 1u > wf->cap) {
        size_t new_cap = (wf->cap == 0u) ? 128u : (wf->cap * 2u);
        aer_tx_sample_t* p = (aer_tx_sample_t*)realloc(wf->samples, new_cap * sizeof(*p));
        if (!p) return false;
        wf->samples = p;
        wf->cap = new_cap;
    }

    /* basic monotonic check */
    if (wf->len > 0u && t < wf->samples[wf->len - 1u].t) {
        return false;
    }

    wf->samples[wf->len].t = t;
    wf->samples[wf->len].data = data;
    wf->samples[wf->len].ack = ack;
    wf->len++;
    return true;
}

bool aer_waveform_load_file(const char* path, aer_waveform_t* wf)
{
    if (!path || !wf) return false;

    FILE* f = fopen(path, "r");
    if (!f) return false;

    char line[256];
    uint32_t line_no = 0;

    while (fgets(line, sizeof(line), f)) {
        line_no++;

        /* skip comments/blank */
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n' || *p == '#') continue;

        /* allow commas */
        for (char* q = p; *q; ++q) {
            if (*q == ',') *q = ' ';
        }

        uint64_t t = 0;
        char data_str[64] = {0};
        unsigned ack_u = 0;

        int n = sscanf(p, "%llu %63s %u",
                       (unsigned long long*)&t, data_str, &ack_u);
        if (n != 3) {
            fprintf(stderr, "[aer_waveform_load_file] parse error at %s:%u: %s", path, line_no, p);
            fclose(f);
            return false;
        }

        /* parse hex data */
        char* end = NULL;
        unsigned long data_ul = strtoul(data_str, &end, 0); /* base 0 handles 0x */
        if (end == data_str) {
            fprintf(stderr, "[aer_waveform_load_file] hex parse error at %s:%u: %s\n", path, line_no, data_str);
            fclose(f);
            return false;
        }

        if (!wf_push_raw(wf, t, (aer_raw_word_t)data_ul, (ack_u != 0u))) {
            fprintf(stderr, "[aer_waveform_load_file] push failed at %s:%u\n", path, line_no);
            fclose(f);
            return false;
        }
    }

    fclose(f);
    return true;
}

/* ---------------- Example fault injectors ----------------
 *
 * You can use these by setting cfg.fault_fn and cfg.fault_user
 */

typedef struct aer_fault_glitch_s {
    uint64_t start_t;
    uint64_t end_t;          /* inclusive range [start_t, end_t] */
    aer_raw_word_t xor_mask; /* toggled bits during the window */
} aer_fault_glitch_t;

/* Glitch: XOR data during a time window (does not touch ACK) */
bool aer_fault_glitch_data(uint64_t t, aer_raw_word_t* io_data, bool* io_ack, void* user)
{
    (void)io_ack;
    aer_fault_glitch_t* g = (aer_fault_glitch_t*)user;
    if (!g || !io_data) return true;

    if (t >= g->start_t && t <= g->end_t) {
        *io_data ^= g->xor_mask;
    }
    return true;
}

typedef struct aer_fault_stuck_ack_s {
    uint64_t start_t;
    bool     level;
} aer_fault_stuck_ack_t;

/* Stuck ACK: force ACK to a fixed level starting at start_t */
bool aer_fault_stuck_ack(uint64_t t, aer_raw_word_t* io_data, bool* io_ack, void* user)
{
    (void)io_data;
    aer_fault_stuck_ack_t* s = (aer_fault_stuck_ack_t*)user;
    if (!s || !io_ack) return true;

    if (t >= s->start_t) {
        *io_ack = s->level;
    }
    return true;
}

typedef struct aer_fault_drop_neutral_s {
    bool enabled;
} aer_fault_drop_neutral_t;

/* Drop neutral: if DATA becomes 0 at any sample, force it back to previous nonzero
 * This simulates "missing neutral" (spacer removed)
 */
bool aer_fault_drop_neutral(uint64_t t, aer_raw_word_t* io_data, bool* io_ack, void* user)
{
    (void)t;
    (void)io_ack;
    aer_fault_drop_neutral_t* d = (aer_fault_drop_neutral_t*)user;
    static aer_raw_word_t last_nonzero = 0u;

    if (!d || !d->enabled || !io_data) return true;

    if (*io_data != 0u) {
        last_nonzero = *io_data;
    } else {
        /* neutral -> replace with last nonzero (if any) */
        if (last_nonzero != 0u) {
            *io_data = last_nonzero;
        }
    }
    return true;
}
