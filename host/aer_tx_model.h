#ifndef AER_TX_MODEL_H
#define AER_TX_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aer_types.h"   /* aer_raw_word_t */

/* One recorded point in the waveform */
typedef struct aer_tx_sample_s {
    uint64_t       t;      /* time in ticks (arbitrary time value) */
    aer_raw_word_t data;   /* packed DATA bus value */
    bool           ack;    /* ACK line level */
} aer_tx_sample_t;

/* Growable array of samples */
typedef struct aer_waveform_s {
    aer_tx_sample_t *samples;
    size_t           len;
    size_t           cap;
} aer_waveform_t;

/* Timing knobs for the modeled receiver ACK behavior.
   This models the DI diagram: valid -> ack high -> neutral -> ack low. */
typedef struct aer_tx_model_cfg_s {
    uint32_t ack_rise_delay;     /* ticks after valid word is driven before ACK rises */
    uint32_t data_clear_delay;   /* ticks after ACK rises before DATA is cleared to neutral */
    uint32_t ack_fall_delay;     /* ticks after neutral is driven before ACK falls */
    aer_raw_word_t neutral_word; /* 0 */
    bool initial_ack;            /* initial ACK level (0) */
} aer_tx_model_cfg_t;

/* Stateful generator */
typedef struct aer_tx_model_s {
    aer_tx_model_cfg_t cfg;
    uint64_t           t;
    aer_raw_word_t     cur_data;
    bool               cur_ack;
    aer_waveform_t    *out;
} aer_tx_model_t;

/* Waveform helpers */
void aer_waveform_init(aer_waveform_t *wf);
void aer_waveform_free(aer_waveform_t *wf);

/* Convenience defaults */
aer_tx_model_cfg_t aer_tx_model_cfg_default(void);

/* Init generator; appends the initial state as the first sample. */
void aer_tx_model_init(aer_tx_model_t *m,
                       const aer_tx_model_cfg_t *cfg,
                       aer_waveform_t *out,
                       uint64_t t0);

/* Emit one DI transaction for a single raw word:
   valid(word) -> ack high -> neutral -> ack low. */
bool aer_tx_model_emit_word(aer_tx_model_t *m, aer_raw_word_t word);

/* Emit a sequence of words; returns false on allocation failure. */
bool aer_tx_model_emit_words(aer_tx_model_t *m,
                             const aer_raw_word_t *words,
                             size_t n_words);

#endif /* AER_TX_MODEL_H */
