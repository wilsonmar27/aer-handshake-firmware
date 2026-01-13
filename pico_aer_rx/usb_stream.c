// usb_stream.c
#include "usb_stream.h"

#include <string.h>
#include <stdio.h>

#include "hal/hal_stdio.h"
#include "hal/hal_time.h"

/* ---------------- Internal state ---------------- */

static usb_stream_cfg_t g_cfg = {
    .timestamps_enabled = true,
    .data_width_bits    = 0,
};


static usb_stream_stats_t g_stats = {0};

/* ---------------- Event record payloads ----------------
 * These are the payload bytes inside HAL_STREAM_EVENT_BIN.
 * We include an explicit record type so the host can resync even if it misses HELLO.
 */

/* No-timestamp record (V1) */
typedef struct __attribute__((packed)) usb_evt_v1_nots_s {
    uint8_t  rec_type; // USB_EVT_REC_V1_NOTS
    uint8_t  flags;
    uint8_t  row;
    uint8_t  col;
} usb_evt_v1_nots_t;

/* cycle-count record (V1) */
typedef struct __attribute__((packed)) usb_evt_v1_ticks_s {
    uint8_t  rec_type; // USB_EVT_REC_V1_TICKS
    uint8_t  flags;
    uint8_t  row;
    uint8_t  col;
    uint32_t t_ticks;  // cycle counter ticks at emission
} usb_evt_v1_ticks_t;

static inline usb_stream_event_rec_type_t active_rec_type(void) {
    return g_cfg.timestamps_enabled ? USB_EVT_REC_V1_TICKS : USB_EVT_REC_V1_NOTS;
}

void usb_stream_init(const usb_stream_cfg_t *cfg)
{
    if (cfg) g_cfg = *cfg;
    g_stats = (usb_stream_stats_t){0};
}

void usb_stream_set_timestamps_enabled(bool enabled)
{
    g_cfg.timestamps_enabled = enabled;
}

usb_stream_event_rec_type_t usb_stream_event_record_type(void)
{
    return active_rec_type();
}

bool usb_stream_send_on_event(uint8_t row, uint8_t col)
{
    return usb_stream_send_event(row, col, (uint8_t)USB_EVT_FLAG_ON);
}

bool usb_stream_send_event(uint8_t row, uint8_t col, uint8_t flags)
{
    if (!hal_stdio_is_connected()) {
        g_stats.events_dropped_not_connected++;
        return false;
    }

    bool ok = false;

    if (g_cfg.timestamps_enabled) {
        usb_evt_v1_ticks_t e;
        e.rec_type = (uint8_t)USB_EVT_REC_V1_TICKS;
        e.flags    = flags;
        e.row      = row;
        e.col      = col;
        e.t_ticks  = hal_cycles_now();
        ok = hal_stream_write(HAL_STREAM_EVENT_BIN, &e, (uint16_t)sizeof(e));
    } else {
        usb_evt_v1_nots_t e;
        e.rec_type = (uint8_t)USB_EVT_REC_V1_NOTS;
        e.flags    = flags;
        e.row      = row;
        e.col      = col;
        ok = hal_stream_write(HAL_STREAM_EVENT_BIN, &e, (uint16_t)sizeof(e));
    }

    if (ok) g_stats.events_sent++;
    return ok;
}

const usb_stream_stats_t *usb_stream_stats(void)
{
    return &g_stats;
}
