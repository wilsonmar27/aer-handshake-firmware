#include "aer_tx_model.h"

#include <stdlib.h>
#include <string.h>

/* ---------------- Waveform vector ---------------- */

void aer_waveform_init(aer_waveform_t *wf)
{
    if (!wf) return;
    wf->samples = NULL;
    wf->len = 0u;
    wf->cap = 0u;
}

void aer_waveform_free(aer_waveform_t *wf)
{
    if (!wf) return;
    free(wf->samples);
    wf->samples = NULL;
    wf->len = 0u;
    wf->cap = 0u;
}

static bool wf_reserve(aer_waveform_t *wf, size_t need_cap)
{
    if (!wf) return false;
    if (wf->cap >= need_cap) return true;

    size_t new_cap = (wf->cap == 0u) ? 64u : wf->cap;
    while (new_cap < need_cap) {
        /* grow ~2x */
        new_cap = (new_cap < (SIZE_MAX / 2u)) ? (new_cap * 2u) : need_cap;
        if (new_cap < need_cap) new_cap = need_cap;
    }

    aer_tx_sample_t *p = (aer_tx_sample_t *)realloc(wf->samples, new_cap * sizeof(*p));
    if (!p) return false;

    wf->samples = p;
    wf->cap = new_cap;
    return true;
}

/* Append a sample only if it changes DATA or ACK (or if it's the very first) */
static bool wf_push_transition(aer_waveform_t *wf, uint64_t t, aer_raw_word_t data, bool ack)
{
    if (!wf) return false;

    if (wf->len > 0u) {
        const aer_tx_sample_t *last = &wf->samples[wf->len - 1u];
        if (last->data == data && last->ack == ack) {
            /* no transition */
            return true;
        }
        /* Keep time monotonic, If caller schedules same-timestamp transitions,
           we allow equality (stable ordering in code), but never decreasing. */
        if (t < last->t) {
            return false; /* programming error */
        }
    }

    if (!wf_reserve(wf, wf->len + 1u)) return false;

    wf->samples[wf->len].t = t;
    wf->samples[wf->len].data = data;
    wf->samples[wf->len].ack = ack;
    wf->len++;
    return true;
}

/* ---------------- Model ---------------- */

aer_tx_model_cfg_t aer_tx_model_cfg_default(void)
{
    aer_tx_model_cfg_t cfg;
    cfg.ack_rise_delay   = 1u;
    cfg.data_clear_delay = 0u;
    cfg.ack_fall_delay   = 1u;
    cfg.neutral_word     = (aer_raw_word_t)0u;
    cfg.initial_ack      = false;
    return cfg;
}

void aer_tx_model_init(aer_tx_model_t *m,
                       const aer_tx_model_cfg_t *cfg,
                       aer_waveform_t *out,
                       uint64_t t0)
{
    if (!m) return;

    if (cfg) {
        m->cfg = *cfg;
    } else {
        m->cfg = aer_tx_model_cfg_default();
    }

    m->t = t0;
    m->cur_data = m->cfg.neutral_word;
    m->cur_ack  = m->cfg.initial_ack;
    m->out = out;

    if (m->out) {
        /* Initial state snapshot */
        (void)wf_push_transition(m->out, m->t, m->cur_data, m->cur_ack);
    }
}

bool aer_tx_model_emit_word(aer_tx_model_t *m, aer_raw_word_t word)
{
    if (!m || !m->out) return false;

    /* place valid word */
    if (!wf_push_transition(m->out, m->t, word, m->cur_ack)) return false;
    m->cur_data = word;

    /* wait -> ACK rises (receiver latched) */
    uint64_t t_ack_hi = m->t + (uint64_t)m->cfg.ack_rise_delay;
    if (!wf_push_transition(m->out, t_ack_hi, m->cur_data, true)) return false;
    m->cur_ack = true;

    /* place neutral (all zeros) after ACK is high */
    uint64_t t_neutral = t_ack_hi + (uint64_t)m->cfg.data_clear_delay;
    if (!wf_push_transition(m->out, t_neutral, m->cfg.neutral_word, m->cur_ack)) return false;
    m->cur_data = m->cfg.neutral_word;

    /* wait -> ACK falls */
    uint64_t t_ack_lo = t_neutral + (uint64_t)m->cfg.ack_fall_delay;
    if (!wf_push_transition(m->out, t_ack_lo, m->cur_data, false)) return false;
    m->cur_ack = false;

    /* Advance model time to the end of this transaction.
       Next word starts immediately at this timestamp. */
    m->t = t_ack_lo;
    return true;
}

bool aer_tx_model_emit_words(aer_tx_model_t *m,
                             const aer_raw_word_t *words,
                             size_t n_words)
{
    if (!m || !m->out) return false;
    if (!words && n_words != 0u) return false;

    for (size_t i = 0; i < n_words; ++i) {
        if (!aer_tx_model_emit_word(m, words[i])) return false;
    }
    return true;
}
