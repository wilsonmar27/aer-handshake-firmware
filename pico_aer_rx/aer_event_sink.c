// aer_event_sink.c
#include "aer_event_sink.h"

#include "pico.h"   // tight_loop_contents()

#include "usb_stream.h" // usb_stream_send_on_event()

static inline void hard_fault_spin(void) {
    while (1) { tight_loop_contents(); }
}

void aer_event_sink_init(aer_event_sink_t *sink, const aer_event_sink_cfg_t *cfg)
{
    if (!sink) hard_fault_spin();

    sink->cfg.enabled = true;
    if (cfg) {
        sink->cfg = *cfg;
    }

    sink->stats = (aer_event_sink_stats_t){0};
}

void aer_event_sink_reset(aer_event_sink_t *sink)
{
    if (!sink) return;
    sink->stats = (aer_event_sink_stats_t){0};
}

void aer_event_sink_set_enabled(aer_event_sink_t *sink, bool enabled)
{
    if (!sink) return;
    sink->cfg.enabled = enabled;
}

const aer_event_sink_stats_t *aer_event_sink_stats(const aer_event_sink_t *sink)
{
    return sink ? &sink->stats : (const aer_event_sink_stats_t *)0;
}

void aer_event_sink_on_event(uint16_t row, uint16_t col, void *user)
{
    aer_event_sink_t *sink = (aer_event_sink_t *)user;
    if (!sink) return;

    sink->stats.events_emitted++;

    if (!sink->cfg.enabled) {
        // Treat as "not sent" but not an error: you're intentionally disabled.
        sink->stats.usb_send_failed++;
        return;
    }

    const bool ok = usb_stream_send_on_event(row, col);
    if (ok) sink->stats.usb_sent_ok++;
    else    sink->stats.usb_send_failed++;
}
