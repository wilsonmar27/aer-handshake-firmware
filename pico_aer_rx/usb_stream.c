// usb_stream.c
#include "usb_stream.h"

#include <string.h>
#include <stdio.h>

#include "hal/hal_stdio.h"
#include "hal/hal_time.h"

/* ---------------- Internal state ---------------- */

static usb_stream_cfg_t g_cfg = {
    .timestamps_enabled = true,
    .send_hello_on_init = true,
    .data_width_bits    = 0,
};

static usb_stream_stats_t g_stats = {0};

/* ---------------- HELLO descriptor ----------------
 * Sent inside HAL_STREAM_RAW_BIN (outer framing already provided by hal_stdio).
 *
 * Payload is versioned so host can evolve independently.
 */
#define USB_HELLO_MAGIC_0 'A'
#define USB_HELLO_MAGIC_1 'E'
#define USB_HELLO_MAGIC_2 'R'
#define USB_HELLO_MAGIC_3 'H'
#define USB_HELLO_VER     1u

/* Flags for hello */
enum {
    USB_HELLO_FLAG_HAS_TS = 0x01u,
};

typedef struct __attribute__((packed)) usb_hello_v1_s {
    uint8_t  magic[4];         // "AERH"
    uint8_t  ver;              // 1
    uint8_t  event_rec_type;   // usb_stream_event_rec_type_t
    uint8_t  data_width_bits;  // e.g., 12
    uint8_t  flags;            // USB_HELLO_FLAG_*
    uint32_t rsvd0;            // reserved for future (endianness: LE)
} usb_hello_v1_t;

/* ---------------- Event record payloads ----------------
 * These are the payload bytes inside HAL_STREAM_EVENT_BIN.
 * We include an explicit record type so the host can resync even if it misses HELLO.
 */

/* No-timestamp record (V1) */
typedef struct __attribute__((packed)) usb_evt_v1_nots_s {
    uint8_t  rec_type; // USB_EVT_REC_V1_NOTS
    uint8_t  flags;    // USB_EVT_FLAG_*
    uint16_t row;
    uint16_t col;
} usb_evt_v1_nots_t;

/* Timestamped record (V1) */
typedef struct __attribute__((packed)) usb_evt_v1_ts_s {
    uint8_t  rec_type; // USB_EVT_REC_V1_TS
    uint8_t  flags;    // USB_EVT_FLAG_*
    uint16_t row;
    uint16_t col;
    uint32_t t_us;     // timestamp at event emission (monotonic)
} usb_evt_v1_ts_t;

static inline usb_stream_event_rec_type_t active_rec_type(void) {
    return g_cfg.timestamps_enabled ? USB_EVT_REC_V1_TS : USB_EVT_REC_V1_NOTS;
}

void usb_stream_init(const usb_stream_cfg_t *cfg)
{
    if (cfg) {
        g_cfg = *cfg;
    }
    g_stats = (usb_stream_stats_t){0};

    if (g_cfg.send_hello_on_init) {
        (void)usb_stream_send_hello();
    }
}

void usb_stream_set_timestamps_enabled(bool enabled, bool send_hello)
{
    g_cfg.timestamps_enabled = enabled;
    if (send_hello) {
        (void)usb_stream_send_hello();
    }
}

usb_stream_event_rec_type_t usb_stream_event_record_type(void)
{
    return active_rec_type();
}

bool usb_stream_send_hello(void)
{
    usb_hello_v1_t h;
    h.magic[0] = (uint8_t)USB_HELLO_MAGIC_0;
    h.magic[1] = (uint8_t)USB_HELLO_MAGIC_1;
    h.magic[2] = (uint8_t)USB_HELLO_MAGIC_2;
    h.magic[3] = (uint8_t)USB_HELLO_MAGIC_3;
    h.ver = (uint8_t)USB_HELLO_VER;
    h.event_rec_type  = (uint8_t)active_rec_type();
    h.data_width_bits = (uint8_t)g_cfg.data_width_bits;
    h.flags = (uint8_t)(g_cfg.timestamps_enabled ? USB_HELLO_FLAG_HAS_TS : 0u);
    h.rsvd0 = 0u;

    bool ok_bin = hal_stream_write(HAL_STREAM_RAW_BIN, &h, (uint16_t)sizeof(h));

    /* Human-friendly marker too (helps when watching logs). */
    char marker[96];
    const char *ts = g_cfg.timestamps_enabled ? "1" : "0";
    (void)snprintf(marker, sizeof(marker),
                   "HELLO AER stream v%u rec=%u data_width=%u ts=%s",
                   (unsigned)USB_HELLO_VER,
                   (unsigned)active_rec_type(),
                   (unsigned)g_cfg.data_width_bits,
                   ts);
    bool ok_txt = hal_stream_marker(marker);

    if (ok_bin || ok_txt) {
        g_stats.hello_sent++;
        return true;
    }
    return false;
}

bool usb_stream_send_on_event(uint16_t row, uint16_t col)
{
    return usb_stream_send_event(row, col, (uint8_t)USB_EVT_FLAG_ON);
}

bool usb_stream_send_event(uint16_t row, uint16_t col, uint8_t flags)
{
    if (!hal_stdio_is_connected()) {
        g_stats.events_dropped_not_connected++;
        return false;
    }

    bool ok = false;

    if (g_cfg.timestamps_enabled) {
        usb_evt_v1_ts_t e;
        e.rec_type = (uint8_t)USB_EVT_REC_V1_TS;
        e.flags    = flags;
        e.row      = row;
        e.col      = col;
        e.t_us     = (uint32_t)hal_time_us_now(); // timestamp at event emission
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
